// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_DIAG_H
#define ARDUINO_LIGHTFX_DIAG_H

#include "config.h"
#include <Arduino_LSM6DSOX.h>
#include "log.h"

#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001f
#define TEMP_NA_COMPARE_EPSILON      0.0001f

extern const char calibFileName[];

void diag_events_setup();
void diag_setup();
bool imu_setup();
void diag_run();
void updateLineVoltage();
void updateSystemTemp();
void updateSecEntropy();
void logDiagInfo();


enum Unit:uint8_t {Volts, Deg_F, Deg_C};

// Measurement data structure - value, unit, time (unit is fixed once instantiated)
struct Measurement {
    float value;
    time_t time;
    const Unit unit;

    explicit Measurement(float v, time_t t, Unit u) : value(v), time(t), unit(u) {};
    explicit Measurement(Unit u) : Measurement(0.0f, 0, u) {};

    void copy(const Measurement& msmt) volatile;

    inline bool operator<(volatile Measurement &other) const {
        if (unit == other.unit)
            return value < other.value;
        return false;
    };
    inline bool operator>(volatile Measurement &other) const {
        if (unit == other.unit)
            return value > other.value;
        return false;
    }
    virtual inline void reset() volatile {
        value = 0.0f;
        time = 0;
    }
};

// Triplet of min, max and current measurements
struct MeasurementRange {
    Measurement min;
    Measurement max;
    Measurement current;
    void setMeasurement(const Measurement& msmt) volatile;
    explicit MeasurementRange(Unit unit);
    inline void reset() volatile {
        min.reset();
        max.reset();
        current.reset();
    }
};

// self-calibration supporting structures for RP2040 CPU temp sensor
struct MeasurementPair : Measurement {
    uint adcRaw;

    MeasurementPair() : Measurement(Deg_C), adcRaw(0) {};
    void copy(const MeasurementPair& msmt);
    inline void reset() volatile override {
        Measurement::reset();
        adcRaw = 0;
    }
};

struct CalibrationMeasurement {
    MeasurementPair min;
    MeasurementPair max;
    MeasurementPair ref;

    CalibrationMeasurement() : min(), max(), ref() {};
    void setMeasurement(const MeasurementPair& msmt);
    inline void reset() {
        min.reset();
        max.reset();
        ref.reset();
    }
};

struct CalibrationParams {
    const uint ref33 = MV3_3;     //line V3.3 voltage in milliVolts
    float refTemp;  //reference temperature in degrees Celsius
    float vtref;    //ADC voltage reading at refTemp temperature - in milliVolts
    float slope;    //voltage slope - in mV/C
    float refDelta; //the amount of temperature variation that was used for last calibration; meaningful only if valid is true
    time_t time;    //last calibration time; also used to determine whether this params set is valid
    CalibrationParams(): refTemp(0.0f), vtref(0.0f), slope(0.0f), refDelta(0.0f), time(0) {};
    inline bool isValid() const { return time > 0; }
    inline void reset() {
        refTemp = 0.0f;
        vtref = 0.0f;
        slope = 0.0f;
        refDelta = 0.0f;
        time = 0;
    }
};
extern CalibrationMeasurement calibTempMeasurements;
extern CalibrationParams calibCpuTemp;

bool calibrate();
void readCalibrationInfo();
void saveCalibrationInfo();
// end self-calibration support

extern volatile MeasurementRange imuTempRange;
extern volatile MeasurementRange cpuTempRange;
extern volatile MeasurementRange lineVoltage;


inline static float toFahrenheit(const float celsius) {
    return celsius * 9.0f / 5 + 32;
}

inline static Measurement toFahrenheit(const Measurement &msmt) {
    return Measurement {toFahrenheit(msmt.value), msmt.time, Deg_F};
}

Measurement boardTemperature();
MeasurementPair chipTemperature();
Measurement controllerVoltage();

#endif //ARDUINO_LIGHTFX_DIAG_H
