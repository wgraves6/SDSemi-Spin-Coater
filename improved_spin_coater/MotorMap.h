#ifndef MOTOR_MAP_H
#define MOTOR_MAP_H

#include <Arduino.h>

// ===== Shared Type =====
struct PWMRPMPoint {
    int pwm;
    float rpm;
};

// ===== Config =====
#define MOTOR_MAP_MAX_POINTS 60

// ===== API =====
void MotorMap_init();                  // load from SD card or default
void MotorMap_save();                  // persist current map to SD card

// Replaces the active map with `count` points copied from `points` (e.g.
// a freshly finished calibration sweep). Does not touch the SD card -
// call MotorMap_save() after to persist.
void MotorMap_setActive(const PWMRPMPoint* points, int count);

PWMRPMPoint* MotorMap_get();
int          MotorMap_size();

#endif