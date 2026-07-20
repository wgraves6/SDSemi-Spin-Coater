#include "MotorMap.h"
#include "SdLogger.h"

// ===== Internal Storage =====
static PWMRPMPoint mapData[MOTOR_MAP_MAX_POINTS];
static int mapSize = 0;

// ===== Default Map (stored in flash) =====
// Fallback used when the SD card is missing, unformatted, or has no saved
// calibration file yet - the map this project shipped with before SD
// logging existed.
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

// ===== SD Card =====
// Shares the Ethernet shield's SD slot - see SdLogger.h for why both CS
// pins have to be handled together. Pins match the shield's fixed wiring
// (same as ethernet_sd_test/, where this was verified on hardware).
static const uint8_t SD_CS_PIN = 4;
static const uint8_t ETHERNET_CS_PIN = 10;
static const char* MOTOR_MAP_FILENAME = "MOTORMAP.CSV";  // 8.3-safe name

static SdLogger sdCard(SD_CS_PIN, ETHERNET_CS_PIN);

// ===== Internal =====
// File format: one "pwm,rpm" line per point, no header.
static bool loadFromSd() {
  if (!sdCard.begin()) {
    Serial.println("MotorMap: SD card not available.");
    return false;
  }

  File f = SD.open(MOTOR_MAP_FILENAME, FILE_READ);
  if (!f) {
    Serial.println("MotorMap: no saved map on SD card.");
    return false;
  }

  int count = 0;
  while (f.available() && count < MOTOR_MAP_MAX_POINTS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int comma = line.indexOf(',');
    if (comma < 0) continue;

    mapData[count].pwm = line.substring(0, comma).toInt();
    mapData[count].rpm = line.substring(comma + 1).toFloat();
    count++;
  }
  f.close();

  if (count == 0) {
    Serial.println("MotorMap: MOTORMAP.CSV had no valid points.");
    return false;
  }

  mapSize = count;
  Serial.print("MotorMap: loaded ");
  Serial.print(mapSize);
  Serial.println(" points from SD card.");
  return true;
}

static void loadDefault() {
  mapSize = defaultSize;

  for (int i = 0; i < mapSize; i++) {
    mapData[i] = defaultMap[i];
  }

  Serial.println("MotorMap: using built-in default map.");
}

// ===== Public API =====
void MotorMap_init() {
  if (!loadFromSd()) {
    loadDefault();
  }
}

void MotorMap_setActive(const PWMRPMPoint* points, int count) {
  if (count > MOTOR_MAP_MAX_POINTS) count = MOTOR_MAP_MAX_POINTS;

  for (int i = 0; i < count; i++) {
    mapData[i] = points[i];
  }
  mapSize = count;
}

void MotorMap_save() {
  if (!sdCard.isReady() && !sdCard.begin()) {
    Serial.println("MotorMap: SD card not available, cannot save.");
    return;
  }

  // appendLine() only appends - clear the old file first so a shorter new
  // map doesn't leave stale trailing points from a longer previous one.
  SD.remove(MOTOR_MAP_FILENAME);

  for (int i = 0; i < mapSize; i++) {
    String line = String(mapData[i].pwm) + "," + String(mapData[i].rpm, 2);
    sdCard.appendLine(MOTOR_MAP_FILENAME, line);
  }

  Serial.print("MotorMap: saved ");
  Serial.print(mapSize);
  Serial.println(" points to SD card.");
}

PWMRPMPoint* MotorMap_get() {
  return mapData;
}

int MotorMap_size() {
  return mapSize;
}

