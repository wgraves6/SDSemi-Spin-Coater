#ifndef MOTOR_CALIBRATOR_H
#define MOTOR_CALIBRATOR_H

#include <Arduino.h>
#include "MotorMap.h"

// ---- Calibration State ----
enum CalState {
  CAL_IDLE,
  CAL_SETTLING,
  CAL_SAMPLING,
  CAL_DONE
};

// ---- API ----

// Start calibration
void MotorCalibrator_start();

// Update (call every loop)
void MotorCalibrator_update(float measuredRPM);

// Status
bool MotorCalibrator_isRunning();
bool MotorCalibrator_isDone();
CalState MotorCalibrator_getState();

// Progress (0–100%)
int MotorCalibrator_progress();

// Reset to idle
void MotorCalibrator_reset();

// Inject motor control callback
typedef void (*PWMCallback)(int);
void MotorCalibrator_setPWMCallback(PWMCallback cb);

#endif