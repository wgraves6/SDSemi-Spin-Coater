#include <Wire.h>

void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();

  Serial.println("Scanning Qwiic I2C bus...");
}

void loop() {
  byte count = 0;

  for (byte i = 1; i < 127; i++) {
    Wire1.beginTransmission(i);
    if (Wire1.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(i, HEX);
      count++;
      delay(10);
    }
  }

  if (count == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.println("Done");
  }

  Serial.println("-------------------");
  delay(3000);
}