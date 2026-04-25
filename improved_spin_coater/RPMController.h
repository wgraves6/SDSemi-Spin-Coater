#ifndef RPM_CONTROLLER_H
#define RPM_CONTROLLER_H

#include <Arduino.h>
#include "MotorMap.h"

class RPMController {
public:
  RPMController();

  void begin(PWMRPMPoint* table, int size);

  void setGains(float kp, float ki);
  void setKd(float kd);

  void setOutputLimits(int minPWM, int maxPWM);
  void setRampRate(int maxStepPerUpdate);
  void setDeadband(float rpmDeadband);
  void setOvershootLimit(float ratio);

  void reset();

  int update(float targetRPM, float measuredRPM);

private:
  // Table
  PWMRPMPoint* _table;
  int _size;

  // Gains
  float _kp;
  float _ki;
  float _kd;

  // State
  float _integral;
  float _lastError;

  // Limits
  float _integralMax;
  int _minPWM;
  int _maxPWM;

  // Behavior tuning
  float _deadband;
  float _maxOvershootRatio;

  // Ramp
  int _maxStep;
  int _lastPWM;

  // Helpers
  float interpolatePWM(float rpm);
};

#endif