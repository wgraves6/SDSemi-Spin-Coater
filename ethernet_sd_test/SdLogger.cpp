#include "SdLogger.h"

SdLogger::SdLogger(uint8_t sdCsPin, uint8_t ethernetCsPin)
  : _sdCsPin(sdCsPin), _ethernetCsPin(ethernetCsPin), _ready(false) {}

bool SdLogger::begin() {
    // Deselect both SPI devices before either touches the shared bus.
    pinMode(_ethernetCsPin, OUTPUT);
    digitalWrite(_ethernetCsPin, HIGH);
    pinMode(_sdCsPin, OUTPUT);
    digitalWrite(_sdCsPin, HIGH);

    _ready = SD.begin(_sdCsPin);
    return _ready;
}

bool SdLogger::isReady() const {
    return _ready;
}

bool SdLogger::appendLine(const char* filename, const String& line) {
    if (!_ready) return false;

    File f = SD.open(filename, FILE_WRITE);
    if (!f) return false;
    f.println(line);
    f.close();
    return true;
}

void SdLogger::printFile(const char* filename, Print& output) {
    if (!_ready) return;

    File f = SD.open(filename, FILE_READ);
    if (!f) return;
    while (f.available()) {
        output.write(f.read());
    }
    f.close();
}

void SdLogger::listDirectory(const char* dirPath, Print& output) {
    if (!_ready) return;

    File dir = SD.open(dirPath);
    if (!dir) return;
    while (File entry = dir.openNextFile()) {
        output.print(entry.isDirectory() ? "  [DIR] " : "  ");
        output.print(entry.name());
        if (!entry.isDirectory()) {
            output.print("\t");
            output.print(entry.size());
            output.println(" bytes");
        } else {
            output.println();
        }
        entry.close();
    }
    dir.close();
}
