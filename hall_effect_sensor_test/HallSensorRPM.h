#ifndef HALL_SENSOR_RPM_H
#define HALL_SENSOR_RPM_H

#include <Arduino.h>

class HallSensorRPM {
public:
    HallSensorRPM(uint8_t pin, uint8_t magnetsPerRev);

    void begin();

    float getRPM();
    unsigned long getPeriod();

    int currentState;

    static void isrHandler();

private:
    static HallSensorRPM* _instance;
    void handleInterrupt();

    uint8_t _pin;
    uint8_t _magnetsPerRev;

    volatile unsigned long _lastPulseTime;
    volatile unsigned long _period;

    // filtering
    const unsigned long _minPulseInterval = 500; // microseconds (debounce)
};

#endif