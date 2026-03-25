#ifndef RPM_CONTROLLER_H
#define RPM_CONTROLLER_H

#include <Arduino.h>
#include "MotorMap.h"

class RPMController {
public:
    RPMController();

    void begin(PWMRPMPoint* table, int size);

    void setGains(float kp, float ki);
    void setOutputLimits(int minPWM, int maxPWM);
    void setOvershootLimit(float ratio);       // e.g., 1.10 = 10% max overshoot
    void setRampRate(int maxStepPerUpdate);    // max PWM change per loop
    void setDeadband(float rpmDeadband);       // RPM deadband
    void setCorrectiveScalar(float scalar);    // scale target RPM slightly

    int update(float targetRPM, float measuredRPM);
    void reset();

private:
    PWMRPMPoint* _table;
    int _size;

    float _kp;
    float _ki;

    float _integral;
    float _integralMax;

    int _minPWM;
    int _maxPWM;

    float _maxOvershootRatio;
    int _maxStep;
    float _deadband;

    int _lastPWM;
    float _correctiveScalar;

    float interpolatePWM(float rpm);
    int applyRamp(int targetPWM);
};

#endif