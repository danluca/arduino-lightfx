// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_DIAG_H
#define ARDUINO_LIGHTFX_DIAG_H

#include "config.h"

#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001f
#define TEMP_NA_COMPARE_EPSILON      0.001f

void diagSetup();
void diagExecute();

enum Unit:uint8_t {Volts, Deg_F, Deg_C};

// Measurement data structure - value, unit, time (unit is fixed once instantiated)
struct Measurement {
    virtual ~Measurement() = default;

    float value;
    time_t time;
    const Unit unit;

    explicit Measurement(const float v, const time_t t, const Unit u) : value(v), time(t), unit(u) {};
    explicit Measurement(const Unit u) : Measurement(0.0f, 0, u) {};

    void copy(const Measurement& msmt) volatile;

    bool operator<(const volatile Measurement &other) const {
        if (unit == other.unit)
            return value < other.value;
        return false;
    };
    bool operator>(const volatile Measurement &other) const {
        if (unit == other.unit)
            return value > other.value;
        return false;
    }
    virtual void reset() volatile {
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

    void reset() volatile {
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

    void reset() volatile override {
        Measurement::reset();
        adcRaw = 0;
    }
};

struct CalibrationMeasurement {
    MeasurementPair min;
    MeasurementPair max;
    MeasurementPair ref;

    CalibrationMeasurement() {};
    void setMeasurement(const MeasurementPair& msmt);

    void reset() {
        min.reset();
        max.reset();
        ref.reset();
    }
};

struct CalibrationParams {
    static constexpr uint ref33 = MV3_3;     //line V3.3 voltage in milliVolts
    float refTemp;  //reference temperature in degrees Celsius
    float vtref;    //ADC voltage reading at refTemp temperature - in milliVolts
    float slope;    //voltage slope - in mV/C
    float refDelta; //the amount of temperature variation that was used for last calibration; meaningful only if valid is true
    time_t time;    //last calibration time; also used to determine whether this params set is valid
    CalibrationParams(): refTemp(0.0f), vtref(0.0f), slope(0.0f), refDelta(0.0f), time(0) {};
    [[nodiscard]] bool isValid() const { return time > 0; }

    void reset() {
        refTemp = 0.0f;
        vtref = 0.0f;
        slope = 0.0f;
        refDelta = 0.0f;
        time = 0;
    }
};
extern CalibrationMeasurement calibTempMeasurements;
extern CalibrationParams calibCpuTemp;

void readCalibrationInfo();
void saveCalibrationInfo();
// end self-calibration support

extern volatile MeasurementRange imuTempRange;
extern volatile MeasurementRange cpuTempRange;
extern volatile MeasurementRange wifiTempRange;
extern volatile MeasurementRange lineVoltage;


static float toFahrenheit(const float celsius) {
    return celsius * 9.0f / 5 + 32;
}

static Measurement toFahrenheit(const Measurement &msmt) {
    return Measurement {toFahrenheit(msmt.value), msmt.time, Deg_F};
}

#endif //ARDUINO_LIGHTFX_DIAG_H
