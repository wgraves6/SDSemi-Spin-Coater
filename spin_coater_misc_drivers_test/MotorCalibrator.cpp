#include "MotorCalibrator.h"
#include "MotorMap.h"
#include <Arduino.h>

// ---- CONFIG ----
static const int PWM_START = 30;
static const int PWM_END   = 255;
static const int PWM_STEP  = 5;

static const int SETTLE_TIME_MS = 1500;
static const int SAMPLE_TIME_MS = 500;

// Toggle this
static const bool AUTO_SAVE_TO_EEPROM = false;

// ---- STATE ----
static bool running = false;

static int currentPWM = PWM_START;
static unsigned long stateStartTime = 0;
static float rpmAccum = 0;
static int sampleCount = 0;

static int mapIndex = 0;

// ---- EXTERNAL ----
extern void applyPWM(int pwm);

// ---- INTERNAL STORAGE FOR PRINT ----
static PWMRPMPoint tempMap[MOTOR_MAP_MAX_POINTS];

// ---- API ----
void MotorCalibrator_start() {
    running = true;
    currentPWM = PWM_START;
    mapIndex = 0;

    stateStartTime = millis();
    rpmAccum = 0;
    sampleCount = 0;

    Serial.println("\n=== Calibration START ===");
}

bool MotorCalibrator_isRunning() {
    return running;
}

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

void MotorCalibrator_update(float measuredRPM) {

    if (!running) return;

    unsigned long now = millis();

    // Apply PWM
    applyPWM(currentPWM);

    // ---- SETTLING ----
    if (now - stateStartTime < SETTLE_TIME_MS) {
        return;
    }

    // ---- SAMPLING ----
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
        running = false;

        Serial.println("\n=== Calibration COMPLETE ===");

        // Print copy-paste version
        printMap();

        // Optional EEPROM save
        if (AUTO_SAVE_TO_EEPROM) {
            PWMRPMPoint* map = MotorMap_get();

            for (int i = 0; i < mapIndex; i++) {
                map[i] = tempMap[i];
            }

            MotorMap_save();
            Serial.println("Saved to EEPROM");
        }

        applyPWM(0);
        return;
    }

    // Reset for next step
    stateStartTime = now;
    rpmAccum = 0;
    sampleCount = 0;
}