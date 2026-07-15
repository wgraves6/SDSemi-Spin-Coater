/*
  MPU6050_ignition_upload.ino
  Reads the GY-521 (MPU-6050) over I2C and publishes the readings to Ignition
  over Ethernet via MQTT, on an Arduino R4 WiFi + Arduino Ethernet Shield (W5500).

  This sketch combines:
    - The MPU6050 driver from MPU6050_test/ (Mpu6050.h/.cpp, copied unmodified
      into this sketch folder since Arduino requires driver files to live
      alongside the .ino that uses them).
    - The Ethernet/MQTT upload pattern from arduinoEthernet/arduinoEthernet.ino
      (config.h, copied unmodified for the same reason).

  Wiring:
    MPU6050 VCC -> 5V
    MPU6050 GND -> GND
    MPU6050 SCL -> SCL (dedicated I2C pin on Uno R4 WiFi)
    MPU6050 SDA -> SDA (dedicated I2C pin on Uno R4 WiFi)
    MPU6050 AD0 -> GND (address 0x68)
    Ethernet shield stacked on the R4 (SPI)

  Requires three libraries (Sketch > Include Library > Manage Libraries...):
    - "Ethernet2" by Arduino         - for the W5500 shield chip.
    - "PubSubClient" by Nick O'Leary - lightweight MQTT client.
    - "ArduinoJson" by Benoit Blanchon - builds the JSON payload.
  If your shield is the older W5100 chip, swap the include in config.h from
  <Ethernet2.h> to <Ethernet.h> and nothing else needs to change.

  Network/broker settings live in config.h in this sketch folder - edit them
  there. To watch published messages on the broker machine:
    mosquitto_sub -h localhost -t arduino/mpu6050 -u will -P password

  Open Serial Monitor at 115200 baud after uploading.
*/

#include "Mpu6050.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Separate topic from arduinoEthernet's "arduino/test" so both sketches can
// publish to the same broker without colliding.
const char mpuTopic[] = "arduino/mpu6050";

// AD0 pin tied to GND -> address 0x68 (default).
MPU6050 imu(MPU6050_ADDRESS_AD0_LOW);

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 200; // ms

void connectMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword)) {
      Serial.println("connected.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds.");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for native USB Serial to be ready (Uno R4 WiFi)
  }

  Serial.println(F("MPU-6050 -> Ignition (MQTT) upload starting..."));

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
  Serial.println(F("Calibration complete."));

  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(1000); // let the shield's link come up

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield not found. Check wiring/SPI connection.");
    return;
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable not connected.");
  }

  Serial.print("Arduino IP address: ");
  Serial.println(Ethernet.localIP());

  mqttClient.setServer(mqttBroker, mqttPort);

  Serial.println(F("Setup complete. Publishing readings.\n"));
}

void loop() {
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublish >= publishInterval) {
    lastPublish = now;

    MPU6050Data data;
    if (!imu.readAll(data)) {
      Serial.println(F("Read failed - check I2C connection."));
      return;
    }

    JsonDocument doc;
    doc["accelX"] = data.accelX;
    doc["accelY"] = data.accelY;
    doc["accelZ"] = data.accelZ;
    doc["gyroX"]  = data.gyroX;
    doc["gyroY"]  = data.gyroY;
    doc["gyroZ"]  = data.gyroZ;
    doc["tempC"]  = data.tempC;

    char payload[256];
    serializeJson(doc, payload);

    Serial.print("Publishing: ");
    Serial.println(payload);
    mqttClient.publish(mpuTopic, payload);
  }
  delay(1000);
}
