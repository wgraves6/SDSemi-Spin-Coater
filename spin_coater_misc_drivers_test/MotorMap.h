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
void MotorMap_init();                  // load from EEPROM or default
void MotorMap_save();                  // persist current map

PWMRPMPoint* MotorMap_get();           // pointer to working map
int MotorMap_size();                   // number of valid points

bool MotorMap_setPoint(int index, int pwm, float rpm);

// Optional utility
void MotorMap_resetToDefault();

#endif