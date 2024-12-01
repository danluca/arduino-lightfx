// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <queue.h>
#include <timers.h>
#include <hardware/adc.h>
#include <Arduino_LSM6DSOX.h>
#include <FastLED.h>
#include "SchedulerExt.h"
#include "diag.h"
#include "util.h"
#include "filesystem.h"
#include "sysinfo.h"

#define DIAG_QUEUE_TIMEOUT  5000     //enqueuing timeout - 5 seconds

static const uint maxAdc = 1 << ADC_RESOLUTION;
const char calibFileName[] PROGMEM = "/status/calibration.json";

volatile MeasurementRange imuTempRange(Unit::Deg_C);
volatile MeasurementRange cpuTempRange(Unit::Deg_C);
volatile MeasurementRange lineVoltage(Unit::Volts);
CalibrationMeasurement calibTempMeasurements;
CalibrationParams calibCpuTemp;
QueueHandle_t diagQueue;
TaskWrapper *diagTask;
static uint16_t tmrRndEntropyId = 10;
static uint16_t tmrSysTempId = 11;
static uint16_t tmrSysVoltageId = 12;
static uint16_t tmrSaveSysInfoId = 13;
static uint16_t tmrDiagInfoId = 14;

// declarations ahead
void deviceSetup();
void diagExecute();
void updateSecEntropy();
Measurement boardTemperature();
void updateSystemTemp();
void updateLineVoltage();
void logDiagInfo();
void enqueueRndEntropy(TimerHandle_t xTimer);
void enqueueSysTemp(TimerHandle_t xTimer);
void enqueueSysVoltage(TimerHandle_t xTimer);
void enqueueSaveSysInfo(TimerHandle_t xTimer);
void enqueueDiagInfo(TimerHandle_t xTimer);

// diag task definition - priority is overwritten during setup, see diagSetup
TaskDef diagDef {deviceSetup, diagExecute, 1792, "Diag", 1, CORE_0};
enum DiagAction:uint8_t {RND_ENTROPY, SYS_TEMP, SYS_VOLTAGE, SAVE_SYS_INFO, DIAG_INFO} event;
//static array of diagnostic events - must be in same order as the DiagAction, they're accessed that way
static DiagAction diagEvents[] = {RND_ENTROPY, SYS_TEMP, SYS_VOLTAGE, SAVE_SYS_INFO, DIAG_INFO};

bool imu_setup() {
    // initialize the IMU (Inertial Measurement Unit)
    if (!IMU.begin()) {
        Log.errorln(F("Failed to initialize IMU!"));
        Log.errorln(F("IMU NOT AVAILABLE - TERMINATING THIS THREAD"));
        vTaskSuspend(xTaskGetCurrentTaskHandle());
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

void deviceSetup() {
    readCalibrationInfo();

    adc_setup();

    imu_setup();

    sysInfo->setSysStatus(SYS_STATUS_DIAG);

    Log.infoln(F("Diagnostic devices initialized - system status: %X"), sysInfo->getSysStatus());
}

/**
 * Called from main thread - sets up a higher priority thread for handling the events dispatched by the diagQueue
 */
void diagSetup() {
    //add secure strength entropy event to pseudo-random number generator - repeated each 6 minutes
    TimerHandle_t thRndEntropy = xTimerCreate("rndEntropy", pdMS_TO_TICKS(6*60*1000), pdTRUE, &tmrRndEntropyId, enqueueRndEntropy);
    if (thRndEntropy == nullptr)
        Log.errorln(F("Cannot create rndEntropy timer - Ignored."));
    if (xTimerStart(thRndEntropy, 0) != pdPASS)
        Log.errorln(F("Cannot start the rndEntropy timer - Ignored."));

    //read system temperature event - both the IMU chip as well as the CPU internal ADC based temp sensor - repeated each 32 seconds
    TimerHandle_t thSysTemp = xTimerCreate("sysTemp", pdMS_TO_TICKS(32*1000), pdTRUE, &tmrSysTempId, enqueueSysTemp);
    if (thSysTemp == nullptr)
        Log.errorln(F("Cannot create sysTemp timer - Ignored."));
    if (xTimerStart(thSysTemp, 0) != pdPASS)
        Log.errorln(F("Cannot start the sysTemp timer - Ignored."));

    //read the system's line voltage event - uses ADC - repeated each 34 seconds
    TimerHandle_t thSysVoltage = xTimerCreate("sysVoltage", pdMS_TO_TICKS(34 * 1000), pdTRUE, &tmrSysVoltageId, enqueueSysVoltage);
    if (thSysVoltage == nullptr)
        Log.errorln(F("Cannot create sysVoltage timer - Ignored."));
    if (xTimerStart(thSysVoltage, 0) != pdPASS)
        Log.errorln(F("Cannot start the sysVoltage timer - Ignored."));
#ifndef DISABLE_LOGGING
    //log the thread, memory and diagnostic measurements info event - no-op if logging is disabled - repeated each 27 seconds
    TimerHandle_t thDiagInfo = xTimerCreate("diagInfo", pdMS_TO_TICKS(27 * 1000), pdTRUE, &tmrDiagInfoId, enqueueDiagInfo);
    if (thDiagInfo == nullptr)
        Log.errorln(F("Cannot create diagInfo timer - Ignored."));
    if (xTimerStart(thDiagInfo, 0) != pdPASS)
        Log.errorln(F("Cannot start the diagInfo timer - Ignored."));
#endif
    //save the current system info event to filesystem - repeated each 90 seconds
    TimerHandle_t thSaveSysInfo = xTimerCreate("saveSysInfo", pdMS_TO_TICKS(90 * 1000), pdTRUE, &tmrSaveSysInfoId, enqueueSaveSysInfo);
    if (thSaveSysInfo == nullptr)
        Log.errorln(F("Cannot create saveSysInfo timer - Ignored."));
    if (xTimerStart(thSaveSysInfo, 0) != pdPASS)
        Log.errorln(F("Cannot start the saveSysInfo timer - Ignored."));

    //setup the diagnostic thread, higher priority to avoid interruption during I2C communication
    diagQueue = xQueueCreate(20, sizeof(DiagAction));
    diagDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle()) + 1;
    diagTask = Scheduler.startTask(&diagDef);
    Log.infoln(F("Diagnostic thread [%s] - priority %d - has been setup id %X. Events are dispatching."), diagTask->getName(),
               uxTaskPriorityGet(diagTask->getTaskHandle()), uxTaskGetTaskNumber(diagTask->getTaskHandle()));
}

/**
 * Callback for rndEntropy timer - this is called from Timer task. Enqueues a RND_ENTROPY message for the diagnostic task.
 * @param xTimer the rndEntropy timer that fired the callback
 */
void enqueueRndEntropy(TimerHandle_t xTimer) {
    BaseType_t qResult = xQueueSend(diagQueue, &diagEvents[RND_ENTROPY], pdMS_TO_TICKS(DIAG_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending RND_ENTROPY message to diagnostic task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent RND_ENTROPY event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for sysTemp timer - this is called from Timer task. Enqueues a SYS_TEMP message for the diagnostic task.
 * @param xTimer the sysTemp timer that fired the callback
 */
void enqueueSysTemp(TimerHandle_t xTimer) {
    BaseType_t qResult = xQueueSend(diagQueue, &diagEvents[SYS_TEMP], pdMS_TO_TICKS(DIAG_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending SYS_TEMP message to diagnostic task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent SYS_TEMP event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for sysVoltage timer - this is called from Timer task. Enqueues a SYS_VOLTAGE message for the diagnostic task.
 * @param xTimer the sysVoltage timer that fired the callback
 */
void enqueueSysVoltage(TimerHandle_t xTimer) {
    BaseType_t qResult = xQueueSend(diagQueue, &diagEvents[SYS_VOLTAGE], pdMS_TO_TICKS(DIAG_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending SYS_VOLTAGE message to diagnostic task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent SYS_VOLTAGE event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for saveSysInfo timer - this is called from Timer task. Enqueues a SAVE_SYS_INFO message for the diagnostic task.
 * @param xTimer the saveSysInfo timer that fired the callback
 */
void enqueueSaveSysInfo(TimerHandle_t xTimer) {
    BaseType_t qResult = xQueueSend(diagQueue, &diagEvents[SAVE_SYS_INFO], pdMS_TO_TICKS(DIAG_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending SAVE_SYS_INFO message to diagnostic task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent SAVE_SYS_INFO event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for diagInfo timer - this is called from Timer task. Enqueues a DIAG_INFO message for the diagnostic task.
 * Note: The diagInfo timer is not created if logging is disabled
 * @param xTimer the diagInfo timer that fired the callback
 */
void enqueueDiagInfo(TimerHandle_t xTimer) {
    BaseType_t qResult = xQueueSend(diagQueue, &diagEvents[DIAG_INFO], pdMS_TO_TICKS(DIAG_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending DIAG_INFO message to diagnostic task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent DIAG_INFO event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * The ScheduleExt task scheduler executes this in a continuous loop - this is the main dispatching method of the diagnostic task
 * Receives events from the diagnostic queue and executes appropriate handlers.
 */
void diagExecute() {
    DiagAction msg;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(diagQueue, &msg, portMAX_DELAY))
        return;
    //the reception was successful, hence the msg is not null anymore
    switch (msg) {
        case RND_ENTROPY: updateSecEntropy(); break;
        case SYS_TEMP: updateSystemTemp(); break;
        case SYS_VOLTAGE: updateLineVoltage(); break;
        case SAVE_SYS_INFO: saveSysInfo(); break;
        case DIAG_INFO: logDiagInfo(); break;
        default:
            Log.errorln(F("Event type %d not supported"), msg);
            break;
    }
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
    //log task and RAM metrics
    logTaskStats();
    //logSystemInfo();
#endif
}

