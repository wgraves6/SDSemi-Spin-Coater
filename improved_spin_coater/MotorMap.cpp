#include "MotorMap.h"
#include <EEPROM.h>

// ===== Internal Storage =====
static PWMRPMPoint mapData[MOTOR_MAP_MAX_POINTS];
static int mapSize = 0;

// ===== Default Map (stored in flash) =====
static const PWMRPMPoint defaultMap[] = {
  { 30, 531 },
  { 35, 924 },
  { 40, 1256 },
  { 45, 1564 },
  { 50, 1839 },
  { 55, 2090 },
  { 60, 2312 },
  { 65, 2495 },
  { 70, 2653 },
  { 75, 2813 },
  { 80, 2960 },
  { 85, 3091 },
  { 90, 3204 },
  { 95, 3300 },
  { 100, 3377 },
  { 105, 3465 },
  { 110, 3532 },
  { 115, 3594 },
  { 120, 3655 },
  { 125, 3706 },
  { 130, 3760 },
  { 135, 3807 },
  { 140, 3846 },
  { 145, 3890 },
  { 150, 3908 },
  { 155, 3937 },
  { 160, 3973 },
  { 165, 3994 },
  { 170, 4027 },
  { 175, 4055 },
  { 180, 4087 },
  { 185, 4100 },
  { 190, 4108 },
  { 195, 4133 },
  { 200, 4145 },
  { 205, 4161 },
  { 210, 4187 },
  { 215, 4199 },
  { 220, 4214 },
  { 225, 4230 },
  { 230, 4248 },
  { 235, 4260 },
  { 240, 4272 },
  { 245, 4296 },
  { 250, 4309 },
  { 255, 4384 }
};

static const int defaultSize = 46;

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

