/*
  Ethernet shield + on-board SD card reader test, for Arduino Uno R4 WiFi.

  Standalone hardware bring-up test - NOT part of SDSemi-Spin-Coater. Verifies:
    1. The SD card reader initializes and can write/read/list files, via the
       SdLogger driver (SdLogger.h/.cpp, in this same folder).
    2. The Ethernet shield's W5100 chip is detected and gets a link.
    3. Both work together on the shared SPI bus - see the integration notes
       at the top of SdLogger.h for why that isn't automatic.

  Confirmed working on hardware. SdLogger.h/.cpp are written to be dropped
  into improved_spin_coater/ as-is when SD logging gets added there.

  Wiring assumed (standard official Arduino Ethernet Shield pinout):
    - Ethernet (W5100) chip select: pin 10
    - SD card chip select:          pin 4
  Network settings are in config.h, in this same sketch folder.

  Needs only the built-in SPI, Ethernet, and SD libraries - no extra installs.

  Open the Serial Monitor at 9600 baud after uploading.
*/

#include <SPI.h>
#include "config.h"
#include "SdLogger.h"

const uint8_t ETHERNET_CS = 10;
const uint8_t SD_CS = 4;

const unsigned long LOG_INTERVAL_MS = 5000;
unsigned long lastLogTime = 0;
unsigned int logCounter = 0;

SdLogger sdLogger(SD_CS, ETHERNET_CS);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for Serial Monitor to open (native USB on the R4)
  }

  Serial.println("=== SD card test ===");
  if (!sdLogger.begin()) {
    Serial.println("  SdLogger.begin() FAILED - check card is inserted and formatted FAT16/FAT32.");
  } else {
    Serial.println("  SdLogger.begin() OK.");

    sdLogger.appendLine("test.txt", "boot at millis=" + String(millis()));
    Serial.println("  test.txt contents:");
    sdLogger.printFile("test.txt", Serial);

    Serial.println("  Root directory listing:");
    sdLogger.listDirectory("/", Serial);
  }

  Serial.println("=== Ethernet test ===");
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(1000); // let the shield's link come up

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("  Ethernet shield not found - check wiring/SPI connection.");
  } else {
    Serial.println("  Ethernet hardware detected.");
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("  Ethernet cable not connected.");
    } else {
      Serial.println("  Ethernet link is up.");
    }
    Serial.print("  Arduino IP address: ");
    Serial.println(Ethernet.localIP());
  }

  Serial.println("=== Setup complete - logging to SD every 5s below ===");
}

void loop() {
  unsigned long now = millis();
  if (now - lastLogTime >= LOG_INTERVAL_MS) {
    lastLogTime = now;
    logCounter++;

    String line = String(logCounter) + ",millis=" + String(now) + ",link=" +
                  (Ethernet.linkStatus() == LinkON ? "up" : "down/unknown");

    if (sdLogger.appendLine("log.txt", line)) {
      Serial.print("Logged entry #");
      Serial.println(logCounter);
    } else {
      Serial.println("Log write FAILED - could not open log.txt.");
    }
  }
}
