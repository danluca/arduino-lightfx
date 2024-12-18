// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include <LittleFSWrapper.h>
#include <ArduinoJson.h>
#include "rtos.h"
#include "FastLED.h"
#include "diag.h"
#include "util.h"
#include "sysinfo.h"

using namespace std::chrono;
const uint maxAdc = 1 << ADC_RESOLUTION;
const char calibFileName[] PROGMEM = LITTLEFS_FILE_PREFIX "/calibration.json";

volatile MeasurementRange imuTempRange(Unit::Deg_C);
volatile MeasurementRange cpuTempRange(Unit::Deg_C);
volatile MeasurementRange lineVoltage(Unit::Volts);
CalibrationMeasurement calibTempMeasurements;
CalibrationParams calibCpuTemp;

bool imu_setup() {
    // initialize the IMU (Inertial Measurement Unit)
    if (!IMU.begin()) {
        Log.errorln(F("Failed to initialize IMU!"));
        Log.errorln(F("IMU NOT AVAILABLE - TERMINATING THIS THREAD"));
        osThreadExit();
        //while (true) yield();
    }
    Log.infoln(F("IMU sensor OK"));
    // print the board temperature
    Measurement temp = boardTemperature();
    Log.infoln(F("Board temperature %D 'C (%D 'F) at %y"), temp.value, toFahrenheit(temp.value), temp.time);
    return true;
}

void adc_setup() {
    //disable ADC
    //hw_clear_bits(&adc_hw->cs, ADC_CS_EN_BITS);
    //enable ADC, including temp sensor
    adc_init();
    adc_set_temp_sensor_enabled(true);
    analogReadResolution(ADC_RESOLUTION);   //get us the higher resolution of the ADC
    Log.infoln("ADC OK %d bit resolution", ADC_RESOLUTION);
}

void diag_setup() {
    readCalibrationInfo();

    adc_setup();

    imu_setup();

    sysInfo->setSysStatus(SYS_STATUS_DIAG);

    Log.infoln(F("Diagnostic devices initialized - system status: %X"), sysInfo->getSysStatus());
}

events::EventQueue diagQueue;
events::Event<void(void)> evDiagSetup(&diagQueue, diag_setup);
events::Event<void(void)> evRndEntropy(&diagQueue, updateSecEntropy);
events::Event<void(void)> evSysTemp(&diagQueue, updateSystemTemp);
events::Event<void(void)> evSysVoltage(&diagQueue, updateLineVoltage);
events::Event<void(void)> evSaveSysInfo(&diagQueue, saveSysInfo);
#ifndef DISABLE_LOGGING
events::Event<void(void)> evDiagInfo(&diagQueue, logDiagInfo);
#endif
rtos::Thread *diagThread;

/**
 * Called from main thread - sets up a higher priority thread for handling the events dispatched by the diagQueue
 */
void diag_events_setup() {
    //setup event
    evDiagSetup.delay(2s);
    evDiagSetup.period(events::non_periodic);
    evDiagSetup.post();

    //add secure strength entropy event to pseudo-random number generator
    evRndEntropy.delay(7s);
    evRndEntropy.period(6min);
    evRndEntropy.post();

    //read system temperature event - both the IMU chip as well as the CPU internal ADC based temp sensor
    evSysTemp.delay(11s);
    evSysTemp.period(32s);
    evSysTemp.post();

    //read the system's line voltage event - uses ADC
    evSysVoltage.delay(15s);
    evSysVoltage.period(34s);
    evSysVoltage.post();
#ifndef DISABLE_LOGGING
    //log the thread, memory and diagnostic measurements info event - no-op if logging is disabled
    evDiagInfo.delay(19s);
    evDiagInfo.period(27s);
    evDiagInfo.post();
#endif
    //save the current system info event to filesystem
    evSaveSysInfo.delay(23s);
    evSaveSysInfo.period(90s);
    evSaveSysInfo.post();

    //setup the diagnostic thread, higher priority to avoid interruption during I2C communication
    diagThread = new rtos::Thread(osPriorityAboveNormal, 1792, nullptr, "Diag");
    diagThread->start(callback(&diagQueue, &events::EventQueue::dispatch_forever));
    Log.infoln(F("Diagnostic thread [%s] - priority above normal - has been setup id %X. Events are dispatching."), diagThread->get_name(), diagThread->get_id());
}

/**
 * Copying data from another measurement, for a volatile instance
 * @param msmt measurement to copy fields from
 */
void Measurement::copy(const Measurement &msmt) volatile {
    if (unit == msmt.unit) {
        value = msmt.value;
        time = msmt.time;
    } else
        Log.warningln(F("Measurement::copy - incompatible units, cannot copy %F unit %d at %y into current %F unit %d measurement"), msmt.value, msmt.unit, msmt.time, value, unit);
}

/**
 * Sets the current measurement while capturing min and max
 * Note: The range min/max measurements as well as incoming measurement are expected to have the same unit,
 * as otherwise the min/max comparisons are not meaningful
 * @param msmt measurement to set
 */
void MeasurementRange::setMeasurement(const Measurement &msmt) volatile {
    if (current.unit != msmt.unit) {
        Log.warningln(F("MeasurementRange::setMeasurement - incompatible units, cannot set %F unit %d at %y into range of unit %d"), msmt.value, msmt.unit, msmt.time, current.unit);
        return;
    }
    if (msmt < min || !min.time)
        min.copy(msmt);
    if (msmt > max)
        max.copy(msmt);
    current.copy(msmt);
}

/**
 * Initializes the unit of all measurements in the range
 * @param unit the unit
 */
MeasurementRange::MeasurementRange(const Unit unit) : min(unit), current(unit), max(unit) {
}

void MeasurementPair::copy(const MeasurementPair &msmt) {
    Measurement::copy(msmt);
    adcRaw = msmt.adcRaw;
}

void CalibrationMeasurement::setMeasurement(const MeasurementPair &msmt) {
    if (msmt < min || !min.time)
        min.copy(msmt);
    if (msmt > max)
        max.copy(msmt);
    ref.copy(msmt);
}

/**
 * Build the calibration parameters, leveraging the pre-calculated range arguments passed in
 */
void buildCalParams() {
    float adcRange = (float)calibTempMeasurements.max.adcRaw - (float)calibTempMeasurements.min.adcRaw;
    float tempRange = calibTempMeasurements.min.value - calibTempMeasurements.max.value;
    calibCpuTemp.refTemp = calibTempMeasurements.ref.value;
    calibCpuTemp.vtref = (float)calibTempMeasurements.ref.adcRaw * (float)calibCpuTemp.ref33 / maxAdc;
    calibCpuTemp.slope = adcRange * (float)calibCpuTemp.ref33 / maxAdc / tempRange;
    calibCpuTemp.refDelta = fabs(tempRange);
    calibCpuTemp.time = calibTempMeasurements.ref.time;
}

/**
 * If the reference points are enough apart, extract the calibration parameters
 * @return true if calibration changes were made; false otherwise
 */
bool calibrate() {
    //check if we have valid min and max values
    if (calibTempMeasurements.min.time == 0 || calibTempMeasurements.max.time == 0)
        return false;

    float range = fabs(calibTempMeasurements.min.value - calibTempMeasurements.max.value);
    bool changes = false;

    if (calibCpuTemp.isValid()) {
        //recalibration, when the temp range is significantly larger than last calibration or cpu/imu measurements differ significantly
        bool bRecalRange = range > (calibCpuTemp.refDelta + 5.0f);
        bool bRecalDeviation = fabs(cpuTempRange.current.value-imuTempRange.current.value) > 5.0f && range > 3.0f;

        if(bRecalRange || bRecalDeviation) {
            //re-calibrate
            buildCalParams();
            changes = true;
        }
    } else {
        //first time calibration, we need a temp range of at least 5'C
        float gapLow = fabs(calibTempMeasurements.ref.value - calibTempMeasurements.min.value);
        float gapHigh = fabs(calibTempMeasurements.max.value - calibTempMeasurements.ref.value);
        if (range > 5.0f && gapLow >= 1.0f && gapHigh >= 1.0f) {
            //proceed to calibrate
            buildCalParams();
            changes = true;
        }
    }
    return changes;
}

/**
 * Reads the temperature of the IMU chip, if available, in degrees Celsius
 * @return measurement object with temperature of the IMU chip or IMU_TEMPERATURE_NOT_AVAILABLE along with current time and Celsius unit
 */
Measurement boardTemperature() {
    if (IMU.temperatureAvailable()) {
        float tempC = 0.0f;
        IMU.readTemperatureFloat(tempC);
        return Measurement {tempC, now(), Deg_C};
    } else
        Log.warningln(F("IMU temperature not available - using %D 'C for board temperature value"), IMU_TEMPERATURE_NOT_AVAILABLE);
    return Measurement {IMU_TEMPERATURE_NOT_AVAILABLE, now(), Deg_C};
}

/**
 * Reads the input line voltage of the controller box - expected to be 12V. Measured through a resistive divisor on a ADC pin
 * @return measurement object with line voltage, current time and Volts unit
 */
Measurement controllerVoltage() {
    const uint avgSize = 8;  //we'll average 8 readings back to back
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += analogRead(A0);
    Log.traceln(F("Voltage %d average reading: %d"), avgSize, valSum/avgSize);
    valSum = valSum*MV3_3/avgSize;
    valSum = valSum/VCC_DIV_R5*(VCC_DIV_R5+VCC_DIV_R4)/maxAdc;  //watch out not to exceed uint range, these are large numbers. operations order tuned to avoid overflow
    return Measurement {(float)valSum/1000.0f, now(), Volts};
}

/**
 * Attempt to read the CPU's temperature in degrees Celsius using ADC channel 4 (internal temperature sensor)
 * Note: This sensor requires individual board calibration and is very imprecise
 * @return measurement object with temperature along with current time and Celsius unit
 */
MeasurementPair chipTemperature() {
    uint curAdc = adc_get_selected_input();
    const uint avgSize = 8;   //we'll average 8 readings back to back

    adc_select_input(4);    //internal temperature sensor is on ADC channel 4
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += adc_read();
    adc_select_input(curAdc);   //restore the ADC input selection
#ifndef DISABLE_LOGGING
    Log.traceln(F("Internal temperature value: %d; average reading: %d"), avgSize, valSum/avgSize);
#endif
    uint adcRaw = valSum/avgSize;
    auto tV = (float)(valSum*MV3_3/avgSize/maxAdc);   //voltage in mV
    MeasurementPair result;
    if (calibCpuTemp.isValid()) {
        //per RP2040 documentation - datasheet, section 4.9.5 Temperature Sensor, page 565 - the formula is 27 - (ADC_Voltage - 0.706)/0.001721
        //the Vtref is typical of 0.706V at 27'C with a slope of -1.721mV per degree Celsius
        //float temp = 27.0f - (tV - CHIP_RP2040_TEMP_SENSOR_VOLTAGE_27) / CHIP_RP2040_TEMP_SENSOR_VOLTAGE_SLOPE;
        result.value = calibCpuTemp.refTemp - (tV - calibCpuTemp.vtref) / calibCpuTemp.slope;
    } else
        result.value = IMU_TEMPERATURE_NOT_AVAILABLE;
    result.time = now();
    result.adcRaw = adcRaw;
    return result;
}


static void serializeMeasurementPair(MeasurementPair& obj, JsonObject& json) {
    json["value"] = obj.value;
    json["time"] = obj.time;
    json["adc"] = obj.adcRaw;
    json["unit"] = obj.unit;
}
void static deserializeMeasurementPair(MeasurementPair& obj, JsonObject& json) {
    obj.value = json["value"];
    obj.time = json["time"];
    obj.adcRaw = json["adc"];
}
static void serializeCalibrationMeasurement(CalibrationMeasurement& obj, JsonObject& json) {
    JsonObject jsMin = json["min"].to<JsonObject>();
    serializeMeasurementPair(obj.min, jsMin);
    JsonObject jsMax = json["max"].to<JsonObject>();
    serializeMeasurementPair(obj.max, jsMax);
    JsonObject jsRef = json["ref"].to<JsonObject>();
    serializeMeasurementPair(obj.ref, jsRef);
}
static void deserializeCalibrationMeasurement(CalibrationMeasurement& obj, JsonObject& json) {
    JsonObject jsMin = json["min"].as<JsonObject>();
    deserializeMeasurementPair(obj.min, jsMin);
    JsonObject jsMax = json["max"].as<JsonObject>();
    deserializeMeasurementPair(obj.max, jsMax);
    JsonObject jsRef = json["ref"].as<JsonObject>();
    deserializeMeasurementPair(obj.ref, jsRef);
}
static void serializeCalibrationParams(CalibrationParams& obj, JsonObject& json) {
    json["ref33"] = obj.ref33;
    json["refTemp"] = obj.refTemp;
    json["vtref"] = obj.vtref;
    json["slope"] = obj.slope;
    json["refDelta"] = obj.refDelta;
    json["time"] = obj.time;
}
static void deserializeCalibrationParams(CalibrationParams& obj, JsonObject& json) {
    //obj.ref33 = json["ref33"];
    obj.refTemp = json["refTemp"];
    obj.vtref = json["vtref"];
    obj.slope = json["slope"];
    obj.refDelta = json["refDelta"];
    obj.time = json["time"];
}

void readCalibrationInfo() {
    auto json = new String();
    json->reserve(512);  // approximation
    size_t calibSize = readTextFile(calibFileName, json);
    if (calibSize > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *json);
        if (error) {
            Log.errorln(F("Error reading the CPU temp calibration information JSON file %s [%d bytes]: %s - calibration information state NOT restored. Content read:\n%s"), calibFileName, calibSize, error.c_str(), json->c_str());
            delete json;
            return;
        }
        JsonObject msmt = doc["measurements"].as<JsonObject>();
        deserializeCalibrationMeasurement(calibTempMeasurements, msmt);
        JsonObject cbparams = doc["calibParams"].as<JsonObject>();
        deserializeCalibrationParams(calibCpuTemp, cbparams);
#ifndef DISABLE_LOGGING
        Log.infoln(F("CPU temp calibration Information restored from %s [%d bytes]: min %F 'C, max %F 'C; params: tempRange=%F, refTemp=%F, VTref=%F, slope=%F, time=%y"),
                   calibFileName, calibSize, calibTempMeasurements.min.value, calibTempMeasurements.max.value, calibCpuTemp.refDelta, calibCpuTemp.refTemp, calibCpuTemp.vtref,
                   calibCpuTemp.slope, calibCpuTemp.time);
#endif
    }
    delete json;
}

void saveCalibrationInfo() {
    JsonDocument doc;
    JsonObject msmt = doc["measurements"].to<JsonObject>();
    serializeCalibrationMeasurement(calibTempMeasurements, msmt);
    JsonObject cbparams = doc["calibParams"].to<JsonObject>();
    serializeCalibrationParams(calibCpuTemp, cbparams);
    auto str = new String();    //larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    size_t sz = serializeJson(doc, *str);
    if (writeTextFile(calibFileName, str))
        Log.infoln(F("Successfully saved CPU temp calibration information file %s [%d bytes]"), calibFileName, sz);
    else
        Log.errorln(F("Failed to create/write the CPU temp calibration information file %s"), calibFileName);
    delete str;
}

void updateLineVoltage() {
    lineVoltage.setMeasurement(controllerVoltage());
    Log.infoln(F("Board Vcc voltage %D V"), lineVoltage.current.value);
}

void updateSystemTemp() {
    MeasurementPair chipTemp = chipTemperature();
    Measurement msmt = boardTemperature();
    if (fabs(msmt.value - IMU_TEMPERATURE_NOT_AVAILABLE) > TEMP_NA_COMPARE_EPSILON) {
        imuTempRange.setMeasurement(msmt);
        if (calibCpuTemp.isValid())
            cpuTempRange.setMeasurement(chipTemp);
        chipTemp.value = msmt.value;
        chipTemp.time = msmt.time;
        calibTempMeasurements.setMeasurement(chipTemp);

        if (calibrate())
            saveCalibrationInfo();
    }
#ifndef DISABLE_LOGGING
    Log.infoln(F("CPU internal temperature %D 'C (%D 'F) (ADC %u)"), cpuTempRange.current.value, toFahrenheit(cpuTempRange.current.value), chipTemp.adcRaw);
    Log.infoln(F("CPU temperature calibration parameters valid=%T, refTemp=%F, vtRef=%F, slope=%F, refDelta=%F, time=%y"), calibCpuTemp.isValid(), calibCpuTemp.refTemp,
               calibCpuTemp.vtref, calibCpuTemp.slope, calibCpuTemp.refDelta, calibCpuTemp.time);
    Log.infoln(F("Board temperature %D (last %D) 'C (%D 'F); range [%D - %D] 'C"), msmt.value, imuTempRange.current.value, toFahrenheit(imuTempRange.current.value),
               imuTempRange.min.value, imuTempRange.max.value);
#endif
}

void updateSecEntropy() {
    uint16_t rnd = secRandom16();
    random16_add_entropy(rnd);
    Log.infoln(F("Secure random value %i added as entropy to pseudo random number generator"), rnd);
}

void logDiagInfo() {
#ifndef DISABLE_LOGGING
    //log RAM metrics
    logAllThreadInfo();
    logHeapAndStackInfo();
    //logSystemInfo();
    //logCPUStats();
#endif
}

