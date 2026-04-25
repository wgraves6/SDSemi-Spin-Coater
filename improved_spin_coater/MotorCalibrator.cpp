#include "MotorCalibrator.h"
#include "MotorMap.h"
#include <Arduino.h>

// ---- CONFIG ----
static const int PWM_START = 30;
static const int PWM_END   = 255;
static const int PWM_STEP  = 5;

static const int SETTLE_TIME_MS = 1500;
static const int SAMPLE_TIME_MS = 500;

static const bool AUTO_SAVE_TO_EEPROM = false;

// ---- STATE ----
static CalState _state = CAL_IDLE;

static int currentPWM = PWM_START;
static unsigned long stateStartTime = 0;

static float rpmAccum = 0;
static int sampleCount = 0;

static int mapIndex = 0;

// ---- CALLBACK ----
static PWMCallback _applyPWM = nullptr;

// ---- INTERNAL STORAGE ----
static PWMRPMPoint tempMap[MOTOR_MAP_MAX_POINTS];

// ---- INTERNAL ----
static void printMap() {
  Serial.println("\n=== COPY THIS INTO defaultMap[] ===\n");
  Serial.println("static const PWMRPMPoint defaultMap[] = {");

  for (int i = 0; i < mapIndex; i++) {
    Serial.print("  { ");
    Serial.print((int)tempMap[i].pwm);
    Serial.print(", ");
    Serial.print((int)tempMap[i].rpm);
    Serial.print(" }");

    if (i < mapIndex - 1) Serial.print(",");
    Serial.println();
  }

  Serial.println("};\n");

  Serial.print("static const int defaultSize = ");
  Serial.print(mapIndex);
  Serial.println(";");
}

// ---- API ----

void MotorCalibrator_setPWMCallback(PWMCallback cb) {
  _applyPWM = cb;
}

void MotorCalibrator_start() {
  _state = CAL_SETTLING;

  currentPWM = PWM_START;
  mapIndex = 0;

  stateStartTime = millis();
  rpmAccum = 0;
  sampleCount = 0;

  Serial.println("\n=== Calibration START ===");
}

void MotorCalibrator_reset() {
  _state = CAL_IDLE;
}

bool MotorCalibrator_isRunning() {
  return (_state == CAL_SETTLING || _state == CAL_SAMPLING);
}

bool MotorCalibrator_isDone() {
  return _state == CAL_DONE;
}

CalState MotorCalibrator_getState() {
  return _state;
}

int MotorCalibrator_progress() {
  int totalSteps = (PWM_END - PWM_START) / PWM_STEP;
  if (totalSteps <= 0) return 0;
  return (mapIndex * 100) / totalSteps;
}

void MotorCalibrator_update(float measuredRPM) {

  if (_state == CAL_IDLE || _state == CAL_DONE) return;

  unsigned long now = millis();

  // Apply PWM via callback
  if (_applyPWM) {
    _applyPWM(currentPWM);
  }

  // ---- SETTLING ----
  if (_state == CAL_SETTLING) {
    if (now - stateStartTime >= SETTLE_TIME_MS) {
      _state = CAL_SAMPLING;
      rpmAccum = 0;
      sampleCount = 0;
    }
    return;
  }

  // ---- SAMPLING ----
  if (_state == CAL_SAMPLING) {

    if (now - stateStartTime < (SETTLE_TIME_MS + SAMPLE_TIME_MS)) {
      rpmAccum += measuredRPM;
      sampleCount++;
      return;
    }

    // ---- STORE POINT ----
    float avgRPM = (sampleCount > 0) ? (rpmAccum / sampleCount) : 0;

    if (mapIndex < MOTOR_MAP_MAX_POINTS) {
      tempMap[mapIndex].pwm = currentPWM;
      tempMap[mapIndex].rpm = avgRPM;

      Serial.print("PWM: ");
      Serial.print(currentPWM);
      Serial.print(" RPM: ");
      Serial.println(avgRPM);

      mapIndex++;
    }

    // ---- NEXT STEP ----
    currentPWM += PWM_STEP;

    if (currentPWM > PWM_END) {

      _state = CAL_DONE;

      Serial.println("\n=== Calibration COMPLETE ===");

      printMap();

      if (AUTO_SAVE_TO_EEPROM) {
        PWMRPMPoint* map = MotorMap_get();

        for (int i = 0; i < mapIndex; i++) {
          map[i] = tempMap[i];
        }

        MotorMap_save();
        Serial.println("Saved to EEPROM");
      }

      if (_applyPWM) _applyPWM(0);

      return;
    }

    // Prepare next cycle
    stateStartTime = now;
    _state = CAL_SETTLING;
  }
}