/*
  MPU6050_ignition_upload.ino
  Reads the GY-521 (MPU-6050) over I2C, computes vibration statistics over a
  rolling sample window, and publishes them to Ignition over Ethernet via
  MQTT, on an Arduino R4 WiFi + Arduino Ethernet Shield (W5500).

  This sketch combines:
    - The MPU6050 driver from MPU6050_test/ (Mpu6050.h/.cpp, copied unmodified
      into this sketch folder since Arduino requires driver files to live
      alongside the .ino that uses them). Gravity calibration and the
      RMS/peak/crest-factor sample-window logic live there too - this .ino
      only wires the driver's output up to MQTT/Ignition.
    - The Ethernet/MQTT upload pattern from arduinoEthernet/arduinoEthernet.ino
      (config.h, copied unmodified for the same reason).

  Vibration metrics (published once per sample window instead of raw
  accel/gyro samples - see MPU6050VibrationStats in Mpu6050.h):
    - rmsAccelG   : RMS of the acceleration magnitude after the static
                    gravity vector (measured at startup, sensor held still)
                    is subtracted out, in g.
    - peakAccelG  : largest instantaneous magnitude seen in the window, in g.
    - crestFactor : peakAccelG / rmsAccelG (unitless) - a spike-y, impulsive
                    vibration reads a high crest factor; smooth/periodic
                    vibration reads close to sqrt(2).

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

// How often a vibration-stats window is finalized and published to Ignition.
// Raise this to publish (and upload) less often; lower it to publish more
// often. Accel is still sampled every SAMPLE_INTERVAL_MS regardless - only
// the publish/upload cadence changes.
const unsigned long SAMPLE_INTERVAL_MS = 5;     // how often accel is polled
const unsigned long PUBLISH_INTERVAL_MS = 1000; // how often a window is uploaded (was 500ms)

// AD0 pin tied to GND -> address 0x68 (default).
MPU6050 imu(MPU6050_ADDRESS_AD0_LOW);

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

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

// Writes `value` into `doc[key]` as raw JSON text with a fixed number of
// decimal places, guaranteeing the payload always contains a decimal point
// (e.g. "0.0000", never "0"). Without this, ArduinoJson can render a
// whole-number float without a decimal point, and Ignition's MQTT/JSON
// tag-creation will infer an Int tag instead of a Float tag from that first
// payload - which then rejects or truncates every later fractional value.
void addFloatField(JsonDocument &doc, const char *key, float value, uint8_t decimals, char *buf, size_t bufSize) {
  MPU6050::formatFixed(value, decimals, buf, bufSize);
  doc[key] = serialized(buf);
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
  Serial.println(F("Gyro calibration complete."));

  Serial.println(F("Keep the sensor still (in its resting orientation) for gravity calibration..."));
  imu.calibrateGravity(500);
  Serial.println(F("Gravity calibration complete."));

  imu.setVibrationWindow(SAMPLE_INTERVAL_MS, PUBLISH_INTERVAL_MS);
  imu.beginVibrationWindow();

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

  Serial.println(F("Setup complete. Publishing vibration stats.\n"));
}

void loop() {
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  MPU6050VibrationStats stats;
  if (!imu.updateVibration(stats)) {
    return;
  }

  JsonDocument doc;
  char rmsBuf[16], peakBuf[16], crestBuf[16], tempBuf[16];
  addFloatField(doc, "rmsAccelG",   stats.rmsAccelG,   4, rmsBuf,   sizeof(rmsBuf));
  addFloatField(doc, "peakAccelG",  stats.peakAccelG,  4, peakBuf,  sizeof(peakBuf));
  addFloatField(doc, "crestFactor", stats.crestFactor, 4, crestBuf, sizeof(crestBuf));
  doc["sampleCount"] = stats.sampleCount;

  char payload[256];
  serializeJson(doc, payload);

  Serial.print("Publishing: ");
  Serial.println(payload);
  mqttClient.publish(mpuTopic, payload);
}
