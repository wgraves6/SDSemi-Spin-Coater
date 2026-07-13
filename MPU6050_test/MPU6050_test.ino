/*
  MPU6050_test.ino
  Test sketch for the GY-521 (MPU-6050) driver on Arduino Uno R4 WiFi.

  Wiring:
    MPU6050 VCC -> 5V
    MPU6050 GND -> GND
    MPU6050 SCL -> SCL (dedicated I2C pin on Uno R4 WiFi)
    MPU6050 SDA -> SDA (dedicated I2C pin on Uno R4 WiFi)
    MPU6050 AD0 -> GND (address 0x68) [leave unconnected = also 0x68 on most boards]

  Place MPU6050.h and MPU6050.cpp in the same sketch folder as this file,
  or install them as a library under Documents/Arduino/libraries/MPU6050/.

  Open Serial Monitor at 115200 baud after uploading.
*/

#include "MPU6050.h"

// AD0 pin tied to GND -> address 0x68 (default). Change to
// MPU6050_ADDRESS_AD0_HIGH (0x69) if AD0 is tied to 3.3V/5V instead.
MPU6050 imu(MPU6050_ADDRESS_AD0_LOW);

unsigned long lastPrint = 0;
const unsigned long printInterval = 200; // ms

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for native USB Serial to be ready (Uno R4 WiFi)
  }

  Serial.println(F("MPU-6050 (GY-521) test starting..."));

  // Uno R4 WiFi dedicated I2C pins are used automatically by Wire.begin()
  if (!imu.begin(MPU6050_ACCEL_RANGE_2G, MPU6050_GYRO_RANGE_250DPS, MPU6050_DLPF_44HZ)) {
    Serial.println(F("ERROR: MPU-6050 not found. Check wiring/address (SDA/SCL, power, AD0)."));
    while (1) {
      delay(1000);
    }
  }

  Serial.println(F("MPU-6050 found and initialized."));
  Serial.println(F("Keep the sensor still for gyro calibration..."));
  imu.calibrateGyro(500);
  Serial.println(F("Calibration complete. Starting readings.\n"));

  Serial.println(F("AccelX(g)\tAccelY(g)\tAccelZ(g)\tGyroX(dps)\tGyroY(dps)\tGyroZ(dps)\tTemp(C)"));
}

void loop() {
  unsigned long now = millis();
  if (now - lastPrint >= printInterval) {
    lastPrint = now;

    MPU6050Data data;
    if (imu.readAll(data)) {
      Serial.print(data.accelX, 3); Serial.print('\t');
      Serial.print(data.accelY, 3); Serial.print('\t');
      Serial.print(data.accelZ, 3); Serial.print('\t');
      Serial.print(data.gyroX, 2);  Serial.print('\t');
      Serial.print(data.gyroY, 2);  Serial.print('\t');
      Serial.print(data.gyroZ, 2);  Serial.print('\t');
      Serial.println(data.tempC, 2);
    } else {
      Serial.println(F("Read failed - check I2C connection."));
    }
  }
}
