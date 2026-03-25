#include "MotorMap.h"
#include <EEPROM.h>

// ===== Internal Storage =====
static PWMRPMPoint mapData[MOTOR_MAP_MAX_POINTS];
static int mapSize = 0;

// ===== Default Map (stored in flash) =====
static const PWMRPMPoint defaultMap[] = {
  { 0, 0 }, { 5, 0 }, { 10, 0 }, { 15, 0 }, { 20, 0 }, { 25, 0 },
  { 30, 250 }, { 35, 575 }, { 40, 800 }, { 45, 1100 }, { 50, 1350 },
  { 55, 1550 }, { 60, 1750 }, { 65, 1950 }, { 70, 2100 }, { 75, 2200 },
  { 80, 2350 }, { 85, 2450 }, { 90, 2550 }, { 95, 2650 }, { 100, 2750 },
  { 105, 2850 }, { 110, 2950 }, { 115, 3000 }, { 120, 3100 }, { 125, 3150 },
  { 130, 3200 }, { 135, 3250 }, { 140, 3300 }, { 145, 3350 }, { 150, 3400 },
  { 155, 3450 }, { 160, 3450 }, { 165, 3450 }, { 170, 3500 }, { 175, 3550 },
  { 180, 3550 }, { 185, 3550 }, { 190, 3600 }, { 195, 3600 }, { 200, 3650 },
  { 205, 3650 }, { 210, 3650 }, { 215, 3700 }, { 220, 3700 }, { 225, 3700 },
  { 230, 3700 }, { 235, 3700 }, { 240, 3750 }, { 245, 3750 }, { 250, 3750 },
  { 255, 3850 }
};

static const int defaultSize =
    sizeof(defaultMap) / sizeof(defaultMap[0]);

// ===== EEPROM Header =====
struct MotorMapHeader {
    uint16_t magic;
    uint16_t count;
};

#define MOTOR_MAP_MAGIC 0xBEEF

// ===== Internal =====
static bool loadFromEEPROM() {
    int addr = 0;

    MotorMapHeader header;
    EEPROM.get(addr, header);
    addr += sizeof(header);

    if (header.magic != MOTOR_MAP_MAGIC) return false;
    if (header.count <= 0 || header.count > MOTOR_MAP_MAX_POINTS) return false;

    mapSize = header.count;

    for (int i = 0; i < mapSize; i++) {
        EEPROM.get(addr, mapData[i]);
        addr += sizeof(PWMRPMPoint);
    }

    return true;
}

static void loadDefault() {
    mapSize = defaultSize;

    for (int i = 0; i < mapSize; i++) {
        mapData[i] = defaultMap[i];
    }
}

// ===== Public API =====
void MotorMap_init() {
    if (!loadFromEEPROM()) {
        loadDefault();
    }
}

void MotorMap_save() {
    int addr = 0;

    MotorMapHeader header;
    header.magic = MOTOR_MAP_MAGIC;
    header.count = mapSize;

    EEPROM.put(addr, header);
    addr += sizeof(header);

    for (int i = 0; i < mapSize; i++) {
        EEPROM.put(addr, mapData[i]);
        addr += sizeof(PWMRPMPoint);
    }
}

PWMRPMPoint* MotorMap_get() {
    return mapData;
}

int MotorMap_size() {
    return mapSize;
}

bool MotorMap_setPoint(int index, int pwm, float rpm) {
    if (index < 0 || index >= mapSize) return false;

    mapData[index].pwm = pwm;
    mapData[index].rpm = rpm;
    return true;
}

void MotorMap_resetToDefault() {
    loadDefault();
}