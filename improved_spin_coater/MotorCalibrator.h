#ifndef MOTOR_CALIBRATOR_H
#define MOTOR_CALIBRATOR_H

#include <Arduino.h>
#include "MotorMap.h"

typedef void (*PWMCallback)(int);

void MotorCalibrator_setPWMCallback(PWMCallback cb);
void MotorCalibrator_start();
void MotorCalibrator_update(float measuredRPM);
bool MotorCalibrator_isRunning();
int  MotorCalibrator_progress();

#endif