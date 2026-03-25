#include "RPMController.h"

RPMController::RPMController() {
  _table = nullptr;
  _size = 0;

  // Default gains (safe starting point)
  _kp = 0.035f;
  _ki = 0.0010f;
  _kd = 0.08f;

  _integral = 0;
  _lastError = 0;

  _integralMax = 5000;

  _minPWM = 0;
  _maxPWM = 255;

  _deadband = 10.0f;
  _maxOvershootRatio = 1.10f;

  _maxStep = 4;
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

void RPMController::setKd(float kd) {
  _kd = kd;
}

void RPMController::setOutputLimits(int minPWM, int maxPWM) {
  _minPWM = minPWM;
  _maxPWM = maxPWM;
}

void RPMController::setRampRate(int maxStepPerUpdate) {
  _maxStep = maxStepPerUpdate;
}

void RPMController::setDeadband(float rpmDeadband) {
  _deadband = rpmDeadband;
}

void RPMController::setOvershootLimit(float ratio) {
  _maxOvershootRatio = ratio;
}

void RPMController::reset() {
  _integral = 0;
  _lastError = 0;
  _lastPWM = 0;
}

int RPMController::update(float targetRPM, float measuredRPM) {

  if (_table == nullptr || _size < 2) return 0;

  // ---- Error ----
  float error = targetRPM - measuredRPM;

  // ---- Feedforward (slightly conservative) ----
  float ffPWM = interpolatePWM(targetRPM) * 0.93f;

  // ---- Deadband ----
  if (abs(error) < _deadband) error = 0;

  // ---- Conditional integration ----
  if (abs(error) < 500) {
    _integral += error;
  }

  // ---- Anti-windup clamp ----
  if (_integral > _integralMax) _integral = _integralMax;
  if (_integral < -_integralMax) _integral = -_integralMax;

  // ---- Derivative (rate damping) ----
  float derivative = error - _lastError;
  _lastError = error;

  // ---- Asymmetric proportional gain ----
  float kp_used = (error >= 0) ? _kp : _kp * 2.0f;

  // ---- PID feedback ----
  float fbPWM = (kp_used * error) + (_ki * _integral) + (_kd * derivative);

  float output = ffPWM + fbPWM;

  // ---- Strong overshoot braking ----
  if (measuredRPM > targetRPM) {
    output -= (measuredRPM - targetRPM) * 0.25f;
  }

  // ---- Hard overshoot ceiling (safety) ----
  float maxAllowedRPM = targetRPM * _maxOvershootRatio;
  if (measuredRPM > maxAllowedRPM) {
    output -= (measuredRPM - maxAllowedRPM) * 0.3f;
  }

  // ---- Clamp output ----
  if (output > _maxPWM) output = _maxPWM;
  if (output < _minPWM) output = _minPWM;

  // ---- Asymmetric ramp limiting ----
  int targetPWM = (int)output;
  int delta = targetPWM - _lastPWM;

  if (delta > _maxStep) delta = _maxStep;            // slow ramp up
  if (delta < -_maxStep * 3) delta = -_maxStep * 3;  // fast ramp down

  _lastPWM += delta;

  return _lastPWM;
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
      t = t * t * (3 - 2 * t);
      return pwm1 + t * (pwm2 - pwm1);
    }
  }

  return 0;
}