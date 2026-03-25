#ifndef MOTOR_CALIBRATOR_H
#define MOTOR_CALIBRATOR_H

#include <Arduino.h>
#include "MotorMap.h"

// Starts the calibration sweep
void MotorCalibrator_start();

// Call repeatedly in loop() with current RPM
void MotorCalibrator_update(float measuredRPM);

// Returns true while calibration is running
bool MotorCalibrator_isRunning();

#endif