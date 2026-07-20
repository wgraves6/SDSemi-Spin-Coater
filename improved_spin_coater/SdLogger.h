/*
  Driver for the SD slot on the official Arduino Ethernet Shield (W5100).

  Copied in from ethernet_sd_test/ (a standalone bring-up test on the same
  hardware) once basic read/write/list was verified. Used by MotorMap to
  persist calibration data - see MotorMap.cpp.

  Notes:
  - begin() must run before anything else touches the SD card - it drives
    both CS pins HIGH first, which is required because the SD slot and the
    shield's W5100 chip share MOSI/MISO/SCK.
  - appendLine()/printFile() call SD.open()/close(), which can block for a
    few ms (card-dependent). Keep them out of the RPM control loop's hot
    path - MotorMap only touches the card during init and right after a
    calibration sweep finishes, never during a spin.
  - Filenames are 8.3 format (old FAT limitation) - keep log file names
    short (e.g. "LOG.CSV", not "spin_profile_log.csv").
*/

#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD.h>

class SdLogger {
public:
    SdLogger(uint8_t sdCsPin, uint8_t ethernetCsPin);

    bool begin();
    bool isReady() const;

    // Opens filename, writes line + newline, closes. Creates the file if
    // it doesn't exist; appends if it does.
    bool appendLine(const char* filename, const String& line);

    // Streams a file's contents out to output (e.g. Serial) without
    // buffering the whole thing in RAM.
    void printFile(const char* filename, Print& output);

    // Lists entries (name + size) directly under dirPath, e.g. "/".
    void listDirectory(const char* dirPath, Print& output);

private:
    uint8_t _sdCsPin;
    uint8_t _ethernetCsPin;
    bool _ready;
};

#endif
