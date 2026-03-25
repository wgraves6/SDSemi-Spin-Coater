#include "RPMController.h"

RPMController::RPMController() {
  _table = nullptr;
  _size = 0;

  _kp = 0.04f;
  _ki = 0.0015f;

  _integral = 0;
  _integralMax = 5000;  // anti-windup clamp

  _minPWM = 0;
  _maxPWM = 255;

  _maxOvershootRatio = 1.10f;
  _maxStep = 5;       // max PWM change per update
  _deadband = 20.0f;  // RPM

  _lastPWM = 0;
}

void RPMController::begin(PWMRPMPoint* table, int size) {
  _table = table;
  _size = size;
}

void RPMController::setGains(float kp, float ki) {
  _kp = kp;
  _ki = ki;
}

void RPMController::setOutputLimits(int minPWM, int maxPWM) {
  _minPWM = minPWM;
  _maxPWM = maxPWM;
}

void RPMController::setOvershootLimit(float ratio) {
  _maxOvershootRatio = ratio;
}

void RPMController::setRampRate(int maxStepPerUpdate) {
  _maxStep = maxStepPerUpdate;
}

void RPMController::setDeadband(float rpmDeadband) {
  _deadband = rpmDeadband;
}


void RPMController::reset() {
  _integral = 0;
  _lastPWM = 0;
}

int RPMController::update(float targetRPM, float measuredRPM) {



  if (_table == nullptr || _size < 2) return 0;

  // ---- Overshoot ceiling ----
  float maxAllowedRPM = targetRPM * _maxOvershootRatio;

  // ---- Feedforward ----
  float ffPWM = interpolatePWM(targetRPM);

  // ---- Error ----
  float error = targetRPM - measuredRPM;

  // ---- Deadband (prevents jitter) ----
  if (abs(error) < _deadband) error = 0;

  // ---- Asymmetric gains ----
  float kp_used = (error >= 0) ? _kp : _kp * 2.5f;

  // ---- Integral handling ----
  if (error > 0) _integral += error;
  else _integral *= 0.85f;

  // ---- Anti-windup ----
  if (_integral > _integralMax) _integral = _integralMax;
  if (_integral < -_integralMax) _integral = -_integralMax;

  float fbPWM = (kp_used * error) + (_ki * _integral);
  float output = ffPWM + fbPWM;

  // ---- Hard overshoot protection ----
  if (measuredRPM > maxAllowedRPM) output -= (measuredRPM - maxAllowedRPM) * 0.08f;

  // ---- Clamp output ----
  if (output > _maxPWM) output = _maxPWM;
  if (output < _minPWM) output = _minPWM;

  // ---- Ramp limiting ----
  int finalPWM = applyRamp((int)output);

  _lastPWM = finalPWM;
  return finalPWM;
}

float RPMController::interpolatePWM(float rpm) {
  if (rpm <= _table[0].rpm) return _table[0].pwm;
  if (rpm >= _table[_size - 1].rpm) return _table[_size - 1].pwm;

  for (int i = 0; i < _size - 1; i++) {
    if (rpm >= _table[i].rpm && rpm <= _table[i + 1].rpm) {
      float rpm1 = _table[i].rpm;
      float rpm2 = _table[i + 1].rpm;
      float pwm1 = _table[i].pwm;
      float pwm2 = _table[i + 1].pwm;
      float t = (rpm - rpm1) / (rpm2 - rpm1);
      return pwm1 + t * (pwm2 - pwm1);
    }
  }

  return 0;
}

int RPMController::applyRamp(int targetPWM) {
  int delta = targetPWM - _lastPWM;
  if (delta > _maxStep) delta = _maxStep;
  if (delta < -_maxStep) delta = -_maxStep;
  return _lastPWM + delta;
}