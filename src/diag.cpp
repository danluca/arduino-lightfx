// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <queue.h>
#include <timers.h>
#include <hardware/adc.h>
#include <Arduino_LSM6DSOX.h>
#include <FastLED.h>
#include "SchedulerExt.h"
#include "diag.h"
#include "util.h"
#include "filesystem.h"
#include "FxSchedule.h"
#include "sysinfo.h"
#include "constants.hpp"
#include "log.h"
#if LOGGING_ENABLED == 1
#include "stringutils.h"
#endif

#define DIAG_QUEUE_TIMEOUT  0     //enqueuing timeout - 0 per https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/05-Software-timers/01-Software-timers

static constexpr uint maxAdc = 1 << ADC_RESOLUTION;

volatile MeasurementRange imuTempRange(Unit::Deg_C);
volatile MeasurementRange cpuTempRange(Unit::Deg_C);
volatile MeasurementRange wifiTempRange(Unit::Deg_C);
volatile MeasurementRange lineVoltage(Unit::Volts);
CalibrationMeasurement calibTempMeasurements;
CalibrationParams calibCpuTemp;
QueueHandle_t diagQueue;
//TaskWrapper *diagTask;
static uint16_t tmrRndEntropyId = 10;
static uint16_t tmrSysTempId = 11;
static uint16_t tmrSysVoltageId = 12;
static uint16_t tmrSaveSysInfoId = 13;
static uint16_t tmrDiagInfoId = 14;

// declarations ahead
void deviceSetup();
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
// TaskDef diagDef {deviceSetup, diagExecute, 3072, "Diag", 1, CORE_1};
enum DiagAction:uint8_t {RND_ENTROPY, SYS_TEMP, SYS_VOLTAGE, DIAG_INFO} event;

/**
 * Initializes the Inertial Measurement Unit - IMU
 * @return true if inertial unit was setup ok, false otherwise
 */
bool imu_setup() {
    // initialize the IMU (Inertial Measurement Unit)
    if (!IMU.begin()) {
        log_error(F("Failed to initialize IMU!"));
        log_error(F("IMU NOT AVAILABLE - TERMINATING THIS THREAD"));
        vTaskSuspend(xTaskGetCurrentTaskHandle());
        //while (true) yield();
    }
    log_info(F("IMU sensor OK"));
    // print the board temperature
    const Measurement temp = boardTemperature();
    log_info(F("Board temperature %.2f 'C (%.2f 'F) at %s"), temp.value, toFahrenheit(temp.value), TimeFormat::asString(temp.time).c_str());
    return true;
}

/**
 * Initializes the RP2040 chip internal Analog-Digital Converter
 */
void adc_setup() {
    //disable ADC
    //hw_clear_bits(&adc_hw->cs, ADC_CS_EN_BITS);
    //enable ADC, including temp sensor
    adc_init();
    adc_gpio_init(A0);  //this is the controller voltage pin
    adc_set_temp_sensor_enabled(true);
    analogReadResolution(ADC_RESOLUTION);   //get us the higher resolution of the ADC
    log_info("ADC OK %u bit resolution", ADC_RESOLUTION);
}

/**
 * Setup of devices supporting diagnostics
 */
void deviceSetup() {
    adc_setup();

    imu_setup();

    sysInfo->fillBoardId();

    readCalibrationInfo();

    sysInfo->setSysStatus(SYS_STATUS_DIAG);

    taskDelay(250);

    log_info(F("Diagnostic devices initialized - system status: %#hX"), sysInfo->getSysStatus());
}

/**
 * Called from main thread - sets up a higher priority thread for handling the events dispatched by the diagQueue
 */
void diagSetup() {
    //add secure strength entropy event to pseudo-random number generator - repeated each 6 minutes
    const TimerHandle_t thRndEntropy = xTimerCreate("rndEntropy", pdMS_TO_TICKS(6*60*1000), pdTRUE, &tmrRndEntropyId, enqueueRndEntropy);
    if (thRndEntropy == nullptr)
        log_error(F("Cannot create rndEntropy timer - Ignored."));
    else if (xTimerStart(thRndEntropy, 0) != pdPASS)
        log_error(F("Cannot start the rndEntropy timer - Ignored."));

    //read system temperature event - both the IMU chip and the CPU internal ADC based temp sensor - repeated each 32 seconds
    const TimerHandle_t thSysTemp = xTimerCreate("sysTemp", pdMS_TO_TICKS(32*1000), pdTRUE, &tmrSysTempId, enqueueSysTemp);
    if (thSysTemp == nullptr)
        log_error(F("Cannot create sysTemp timer - Ignored."));
    else if (xTimerStart(thSysTemp, 0) != pdPASS)
        log_error(F("Cannot start the sysTemp timer - Ignored."));

    //read the system's line voltage event - uses ADC - repeated each 34 seconds
    const TimerHandle_t thSysVoltage = xTimerCreate("sysVoltage", pdMS_TO_TICKS(34 * 1000), pdTRUE, &tmrSysVoltageId, enqueueSysVoltage);
    if (thSysVoltage == nullptr)
        log_error(F("Cannot create sysVoltage timer - Ignored."));
    else if (xTimerStart(thSysVoltage, 0) != pdPASS)
        log_error(F("Cannot start the sysVoltage timer - Ignored."));
    //log the thread, memory and diagnostic measurements info event - no-op if logging is disabled - repeated each 27 seconds
    const TimerHandle_t thDiagInfo = xTimerCreate("diagInfo", pdMS_TO_TICKS(27 * 1000), pdTRUE, &tmrDiagInfoId, enqueueDiagInfo);
    if (thDiagInfo == nullptr)
        log_error(F("Cannot create diagInfo timer - Ignored."));
    else if (xTimerStart(thDiagInfo, 0) != pdPASS)
        log_error(F("Cannot start the diagInfo timer - Ignored."));
    //save the current system info event to filesystem - repeated each 90 seconds
    const TimerHandle_t thSaveSysInfo = xTimerCreate("saveSysInfo", pdMS_TO_TICKS(90 * 1000), pdTRUE, &tmrSaveSysInfoId, enqueueSaveSysInfo);
    if (thSaveSysInfo == nullptr)
        log_error(F("Cannot create saveSysInfo timer - Ignored."));
    else if (xTimerStart(thSaveSysInfo, 0) != pdPASS)
        log_error(F("Cannot start the saveSysInfo timer - Ignored."));

    //setup the diagnostic thread, higher priority to avoid interruption during I2C communication
    diagQueue = xQueueCreate(20, sizeof(DiagAction));
    deviceSetup();
    // diagDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle()) + 1;
    // diagTask = Scheduler.startTask(&diagDef);
    const TaskHandle_t diagTask = xTaskGetCurrentTaskHandle();
    log_info(F("Diagnostic thread [%s] - priority %lu - has been setup. Events are dispatching."), pcTaskGetName(diagTask), uxTaskPriorityGet(diagTask));
}

/**
 * Callback for rndEntropy timer - this is called from Timer task. Enqueues a RND_ENTROPY message for the diagnostic task.
 * @param xTimer the rndEntropy timer that fired the callback
 */
void enqueueRndEntropy(TimerHandle_t xTimer) {
    constexpr DiagAction msg = RND_ENTROPY;
    if (const BaseType_t qResult = xQueueSend(diagQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending RND_ENTROPY message to diagnostic task for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent RND_ENTROPY event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for sysTemp timer - this is called from Timer task. Enqueues a SYS_TEMP message for the diagnostic task.
 * @param xTimer the sysTemp timer that fired the callback
 */
void enqueueSysTemp(TimerHandle_t xTimer) {
    constexpr DiagAction msg = SYS_TEMP;
    if (const BaseType_t qResult = xQueueSend(diagQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending SYS_TEMP message to diagnostic task for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent SYS_TEMP event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for sysVoltage timer - this is called from Timer task. Enqueues a SYS_VOLTAGE message for the diagnostic task.
 * @param xTimer the sysVoltage timer that fired the callback
 */
void enqueueSysVoltage(TimerHandle_t xTimer) {
    constexpr DiagAction msg = SYS_VOLTAGE;
    if (const BaseType_t qResult = xQueueSend(diagQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending SYS_VOLTAGE message to diagnostic task for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent SYS_VOLTAGE event successfully to diagnostic task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for saveSysInfo timer - this is called from Timer task. Enqueues a SAVE_SYS_INFO message for the alarm task.
 * @param xTimer the saveSysInfo timer that fired the callback
 */
void enqueueSaveSysInfo(TimerHandle_t xTimer) {
    constexpr MiscAction msg = SAVE_SYS_INFO;
    if (const BaseType_t qResult = xQueueSend(almQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending SAVE_SYS_INFO message to ALM queue for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent SAVE_SYS_INFO event successfully to ALM queue for timer %d [%s]"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer));
}

/**
 * Callback for diagInfo timer - this is called from Timer task. Enqueues a DIAG_INFO message for the diagnostic task.
 * Note: The diagInfo timer is not created if logging is disabled
 * @param xTimer the diagInfo timer that fired the callback
 */
void enqueueDiagInfo(TimerHandle_t xTimer) {
    constexpr DiagAction msg = DIAG_INFO;
    if (const BaseType_t qResult = xQueueSend(diagQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending DIAG_INFO message to DIAG queue for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent DIAG_INFO event successfully to DIAG queue for timer %d [%s]"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer));
}

/**
 * The ScheduleExt task scheduler executes this in a continuous loop - this is the main dispatching method of the diagnostic task
 * Receives events from the diagnostic queue and executes appropriate handlers.
 */
void diagExecute() {
    DiagAction msg;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(diagQueue, &msg, portMAX_DELAY))
    // if (pdFALSE == xQueueReceive(diagQueue, &msg, 0))
        return;
    //the reception was successful, hence the msg is not null anymore
    switch (msg) {
        case RND_ENTROPY: updateSecEntropy(); break;
        case SYS_TEMP: updateSystemTemp(); break;
        case SYS_VOLTAGE: updateLineVoltage(); break;
#if LOGGING_ENABLED == 1
        case DIAG_INFO: logDiagInfo(); break;
#endif
        default:
            log_error(F("Event type %d not supported"), msg);
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
        log_warn(F("Measurement::copy - incompatible units, cannot copy %f unit %d at %s into current %.2f unit %d measurement"), msmt.value, msmt.unit, TimeFormat::asString(msmt.time).c_str(), value, unit);
}

/**
 * Sets the current measurement while capturing min and max
 * Note: The range min/max measurements as well as incoming measurement are expected to have the same unit,
 * as otherwise the min/max comparisons are not meaningful
 * @param msmt measurement to set
 */
void MeasurementRange::setMeasurement(const Measurement &msmt) volatile {
    if (current.unit != msmt.unit) {
        log_warn(F("MeasurementRange::setMeasurement - incompatible units, cannot set %f unit %d at %s into range of unit %d"), msmt.value, msmt.unit, TimeFormat::asString(msmt.time).c_str(), current.unit);
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
MeasurementRange::MeasurementRange(const Unit unit) : min(unit), max(unit), current(unit) {
}

/**
 * Copy a measurement pair into current pair
 * @param msmt measurement pair to copy
 */
void MeasurementPair::copy(const MeasurementPair &msmt) {
    Measurement::copy(msmt);
    adcRaw = msmt.adcRaw;
}

/**
 * Sets the current measurement into the calibration instance. Handles updating the min/max relative to the incoming reading
 * @param msmt measurement pair to set
 */
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
    const float adcRange = (float)calibTempMeasurements.max.adcRaw - (float)calibTempMeasurements.min.adcRaw;
    const float tempRange = calibTempMeasurements.min.value - calibTempMeasurements.max.value;
    calibCpuTemp.refTemp = calibTempMeasurements.ref.value;
    calibCpuTemp.vtref = (float)calibTempMeasurements.ref.adcRaw * (float)CalibrationParams::ref33 / maxAdc;
    calibCpuTemp.slope = adcRange * (float)CalibrationParams::ref33 / maxAdc / tempRange;
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

    const float range = fabs(calibTempMeasurements.min.value - calibTempMeasurements.max.value);
    bool changes = false;

    if (calibCpuTemp.isValid()) {
        //recalibration, when the temp range is significantly larger than last calibration or cpu/imu measurements differ significantly
        const bool bRecalRange = range > (calibCpuTemp.refDelta + 5.0f);
        const bool bRecalDeviation = fabs(cpuTempRange.current.value-imuTempRange.current.value) > 5.0f && range > 3.0f;

        if(bRecalRange || bRecalDeviation) {
            //re-calibrate
            buildCalParams();
            changes = true;
        }
    } else {
        //first time calibration, we need a temp range of at least 5'C
        const float gapLow = fabs(calibTempMeasurements.ref.value - calibTempMeasurements.min.value);
        const float gapHigh = fabs(calibTempMeasurements.max.value - calibTempMeasurements.ref.value);
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
        log_warn(F("IMU temperature not available - using %f 'C for board temperature value"), IMU_TEMPERATURE_NOT_AVAILABLE);
    return Measurement {IMU_TEMPERATURE_NOT_AVAILABLE, now(), Deg_C};
}

/**
 * Reads the input line voltage of the controller box - expected to be 12V. Measured through a resistive divisor on a ADC pin
 * @return measurement object with line voltage, current time and Volts unit
 */
Measurement controllerVoltage() {
    constexpr uint avgSize = 8;  //we'll average 8 readings back to back
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += analogRead(A0);
    log_debug(F("Voltage %d average reading: %d"), avgSize, valSum/avgSize);
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
    const uint curAdc = adc_get_selected_input();
    constexpr uint avgSize = 8;   //we'll average 8 readings back to back

    adc_select_input(4);    //internal temperature sensor is on ADC channel 4
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += adc_read();
    adc_select_input(curAdc);   //restore the ADC input selection
    log_debug(F("Internal temperature value: %u; average reading: %u"), avgSize, valSum/avgSize);
    const uint adcRaw = valSum/avgSize;
    const auto tV = (float)valSum*MV3_3/avgSize/maxAdc;   //voltage in mV
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

/**
 * Serializes the MeasurementPair instance into the JSON object provided
 * @param obj measurement pair object to serialize
 * @param json receiving JSON object
 */
static void serializeMeasurementPair(const MeasurementPair& obj, JsonObject& json) {
    json["value"] = obj.value;
    json["time"] = obj.time;
    json["adc"] = obj.adcRaw;
    json["unit"] = obj.unit;
}

/**
 * Deserializes & updates a measurement pair (instance provided by reference) from the data in the JSON object
 * @param obj measurement pair to update with deserialized data
 * @param json JSON object containing data to deserialize
 */
void static deserializeMeasurementPair(MeasurementPair& obj, JsonObject& json) {
    obj.value = json["value"];
    obj.time = json["time"];
    obj.adcRaw = json["adc"];
}

/**
 * Serializes the calibration measurement object into the JSON object provided
 * @param obj calibration measurement to serialize
 * @param json receiving JSON object
 */
static void serializeCalibrationMeasurement(CalibrationMeasurement& obj, JsonObject& json) {
    JsonObject jsMin = json["min"].to<JsonObject>();
    serializeMeasurementPair(obj.min, jsMin);
    JsonObject jsMax = json["max"].to<JsonObject>();
    serializeMeasurementPair(obj.max, jsMax);
    JsonObject jsRef = json["ref"].to<JsonObject>();
    serializeMeasurementPair(obj.ref, jsRef);
}

/**
 * Deserializes & updates a calibration measurement object (instance provided by reference) from the data in the JSON object
 * @param obj calibration measurement object to update from deserialized data
 * @param json JSON object containing data to deserialize
 */
static void deserializeCalibrationMeasurement(CalibrationMeasurement& obj, JsonObject& json) {
    auto jsMin = json["min"].as<JsonObject>();
    deserializeMeasurementPair(obj.min, jsMin);
    auto jsMax = json["max"].as<JsonObject>();
    deserializeMeasurementPair(obj.max, jsMax);
    auto jsRef = json["ref"].as<JsonObject>();
    deserializeMeasurementPair(obj.ref, jsRef);
}

/**
 * Serializes the calibration parameters object into the JSON object provided
 * @param obj calibration parameters object to serialize
 * @param json receiving JSON object
 */
static void serializeCalibrationParams(const CalibrationParams& obj, JsonObject& json) {
    json["ref33"] = CalibrationParams::ref33;
    json["refTemp"] = obj.refTemp;
    json["vtref"] = obj.vtref;
    json["slope"] = obj.slope;
    json["refDelta"] = obj.refDelta;
    json["time"] = obj.time;
}

/**
 * Deserializes & updates a calibration parameters object (instance provided by reference) from the data in the JSON object
 * @param obj calibration parameters object to update from deserialized data
 * @param json JSON object containing data to deserialize
 */
static void deserializeCalibrationParams(CalibrationParams& obj, JsonObject& json) {
    //obj.ref33 = json["ref33"];
    obj.refTemp = json["refTemp"];
    obj.vtref = json["vtref"];
    obj.slope = json["slope"];
    obj.refDelta = json["refDelta"];
    obj.time = json["time"];
}

/**
 * Read calibration information from the file system
 */
void readCalibrationInfo() {
    auto json = new String();
    json->reserve(512);  // approximation
    if (const size_t calibSize = SyncFsImpl.readFile(calibFileName, json); calibSize > 0) {
        JsonDocument doc;
        if (const DeserializationError error = deserializeJson(doc, *json)) {
            log_error(F("Error reading the CPU temp calibration information JSON file %s [%zd bytes]: %s - calibration information state NOT restored. Content read:\n%s"), calibFileName, calibSize, error.c_str(), json->c_str());
            delete json;
            return;
        }
        auto msmt = doc["measurements"].as<JsonObject>();
        deserializeCalibrationMeasurement(calibTempMeasurements, msmt);
        auto cbparams = doc["calibParams"].as<JsonObject>();
        deserializeCalibrationParams(calibCpuTemp, cbparams);
        log_info(F("CPU temp calibration Information restored from %s [%d bytes]: min %.2f 'C, max %.2f 'C; params: tempRange=%.2f, refTemp=%.2f, VTref=%f, slope=%f, time=%s"),
                   calibFileName, calibSize, calibTempMeasurements.min.value, calibTempMeasurements.max.value, calibCpuTemp.refDelta, calibCpuTemp.refTemp, calibCpuTemp.vtref,
                   calibCpuTemp.slope, TimeFormat::asString(calibCpuTemp.time).c_str());
    }
    delete json;
}

/**
 * Save the calibration information into the file system as JSON file
 */
void saveCalibrationInfo() {
    JsonDocument doc;
    auto msmt = doc["measurements"].to<JsonObject>();
    serializeCalibrationMeasurement(calibTempMeasurements, msmt);
    auto cbparams = doc["calibParams"].to<JsonObject>();
    serializeCalibrationParams(calibCpuTemp, cbparams);
    auto str = new String();    //larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    const size_t sz = serializeJson(doc, *str);
    if (SyncFsImpl.writeFile(calibFileName, str))
        log_info(F("Successfully saved CPU temp calibration information file %s [%zd bytes]"), calibFileName, sz);
    else
        log_error(F("Failed to create/write the CPU temp calibration information file %s"), calibFileName);
    delete str;
}

/**
 * Take a measurement of the power supply voltage of the LightFx system and updates the holding object
 */
void updateLineVoltage() {
    lineVoltage.setMeasurement(controllerVoltage());
    log_info(F("Board Vcc voltage %.2f V"), lineVoltage.current.value);
}

/**
 * Take temperature measurements from two sources:
 *   - IMU module - has a built-in temperature sensor, more precise
 *   - RP2040 chip through channel 4 of the ADC, less precise and in need of calibration
 */
void updateSystemTemp() {
    MeasurementPair chipTemp = chipTemperature();
    const Measurement msmt = boardTemperature();
    if (fabs(msmt.value - IMU_TEMPERATURE_NOT_AVAILABLE) > TEMP_NA_COMPARE_EPSILON) {
        imuTempRange.setMeasurement(msmt);
        //if calibration is valid and the measurement is within 15'C of the higher precision IMU sensor, use the CPU temperature measurement
        if (calibCpuTemp.isValid() && fabs(msmt.value - chipTemp.value) < 15.0f)
            cpuTempRange.setMeasurement(chipTemp);
        chipTemp.value = msmt.value;
        chipTemp.time = msmt.time;
        calibTempMeasurements.setMeasurement(chipTemp);

        if (calibrate())
            saveCalibrationInfo();
    }
    log_info(F("CPU internal temperature %.2f 'C (%.2f 'F) (ADC %u)"), cpuTempRange.current.value, toFahrenheit(cpuTempRange.current.value), chipTemp.adcRaw);
    log_info(F("CPU temperature calibration parameters valid=%s, refTemp=%f, vtRef=%f, slope=%f, refDelta=%f, time=%s"), StringUtils::asString(calibCpuTemp.isValid()), calibCpuTemp.refTemp,
               calibCpuTemp.vtref, calibCpuTemp.slope, calibCpuTemp.refDelta, TimeFormat::asString(calibCpuTemp.time).c_str());
    log_info(F("Board temperature %.2f (last %.2f) 'C (%.2f 'F); range [%.2f - %.2f] 'C"), msmt.value, imuTempRange.current.value, toFahrenheit(imuTempRange.current.value),
               imuTempRange.min.value, imuTempRange.max.value);
}

/**
 * Adds entropy to the FastLED's random16 implementation from the ECC608 security chip
 */
void updateSecEntropy() {
    const uint16_t rnd = secRandom16();
    random16_add_entropy(rnd);
    log_info(F("Secure random value %hu added as entropy to pseudo random number generator"), rnd);
}

/**
 * Logs the diagnostic information of current tasks and memory
 */
void logDiagInfo() {
    //log task and RAM metrics
    logTaskStats();
    //logSystemInfo();
}

/**
 * Gets the temperature of the WiFi submodule from its CPU ESP32 temperature sensor
 * Note: This method is called by the task than handles WiFi communication (CORE0 currently) in order to keep all WiFi module interactions
 * in the same task.
 */
void wifi_temp() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI))
        return;
    //read the ESP32 WiFi chip's temperature
    const Measurement wifiTemp {WiFi.getTemperature(), now(), Deg_C};
    //add the measurement if the jump from previous measurement is reasonable
    const float fTemp = toFahrenheit(wifiTemp.value);
    //I've noticed a suspect Fahrenheit value of 0x80 (128) that is not real (by feeling the chip) - this seems to be some sort of error/NA value
    //if not first reading or current value is within 4 degrees 'C of 53.33'C (128'F) (53.33 'C +/- 4) then consider the 128'F value of the reading, otherwise ignore these (erroneous) readings
    const bool bInvalid = fabs(fTemp - 128.0) < TEMP_NA_COMPARE_EPSILON && (wifiTempRange.current.time == 0 || fabs(wifiTempRange.current.value - 53.33) > 4.0);
#if LOGGING_ENABLED == 1
    if (bInvalid) {
        if (wifiTempRange.current.time == 0)
            log_warn( F("Discarding WiFi temperature measurement of %.2f 'C - 128 'F error value detected"), wifiTemp.value);
        else
            log_warn(F("Discarding WiFi temperature measurement %.2f 'C (%.2f 'F) - not within allowed range for inclusion [49.33 - 57.33] 'C"), wifiTemp.value, fTemp);
    }
#endif
    if (!bInvalid) {
        log_info(F("WiFi subsystem temperature %.2f 'C (%.2f 'F) (last measurement %.2f 'C, %.2f 'F); range [%.2f - %.2f] 'C"), wifiTemp.value, fTemp,
            wifiTempRange.current.value, toFahrenheit(wifiTempRange.current.value), wifiTempRange.min.value, wifiTempRange.max.value);
        wifiTempRange.setMeasurement(wifiTemp);
    }
}

