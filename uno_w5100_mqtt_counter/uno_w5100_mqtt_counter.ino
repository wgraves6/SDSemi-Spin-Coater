/*
  MQTT counter publish for Arduino Uno R3 + W5100 Ethernet shield.

  Standalone test - NOT part of SDSemi-Spin-Coater. Next step after
  uno_w5100_ping_test: connects to the MQTT broker inside the Ignition gateway
  (Cirrus Link MQTT Distributor) and publishes a counter every 5s as
    {"count":123}
  on mqttTopic (config.h). With an MQTT Engine Custom Namespace set to parse
  JSON, that shows up in Ignition as an integer tag named "count".

  Requires one extra library (Sketch > Include Library > Manage Libraries...):
    - "PubSubClient" by Nick O'Leary - lightweight MQTT client.
  The JSON is built with snprintf() instead of ArduinoJson - one number
  doesn't need a JSON library, and the Uno only has 2KB of RAM.

  Wiring (standard W5100 shield, plugged straight onto the Uno):
    - Ethernet (W5100) chip select: pin 10
    - SD card chip select:          pin 4 (held HIGH so the card stays off the bus)

  loop() never waits on the broker: a dropped connection is retried once every
  MQTT_RETRY_MS, and each attempt blocks for at most ~3s (1s TCP connect + 2s
  for the broker's reply). That's the pattern to carry into the spin coater.

  Open the Serial Monitor at 9600 baud after uploading.
*/

#include <SPI.h>
#include <PubSubClient.h>
#include "config.h"

const uint8_t ETHERNET_CS = 10;
const uint8_t SD_CS = 4;

const unsigned long PUBLISH_INTERVAL_MS = 5000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long TCP_CONNECT_TIMEOUT_MS = 1000;
const uint16_t MQTT_REPLY_TIMEOUT_S = 2;

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// False when DHCP got no answer and the static IP from config.h is in use.
bool usingDhcp = false;

unsigned long counter = 0;

void startEthernet() {
  Serial.println(F("Requesting IP over DHCP..."));
  usingDhcp = Ethernet.begin(mac, DHCP_TIMEOUT_MS, DHCP_RESPONSE_MS);

  // begin() resets the chip even when DHCP fails, so this tells
  // "no shield" apart from "no DHCP server".
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(F("Ethernet shield not found - is it seated on the Uno?"));
    while (true) delay(1000);
  }

  if (!usingDhcp) {
    // No router - most likely cabled straight to the laptop. The broker
    // stands in as gateway (only used for off-subnet addresses, never here).
    Serial.println(F("No DHCP reply - using static IP from config.h."));
    Ethernet.begin(mac, staticIp, mqttBroker, mqttBroker, staticSubnet);
  }

  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());
}

// Prints why PubSubClient's last connect failed, in words.
void printMqttState(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT:      Serial.println(F("no reply from broker")); break;
    case MQTT_CONNECT_FAILED:          Serial.println(F("TCP connect failed - broker IP/port, cable, or firewall")); break;
    case MQTT_CONNECT_BAD_CREDENTIALS: Serial.println(F("bad username/password - check MQTT Distributor users")); break;
    case MQTT_CONNECT_UNAUTHORIZED:    Serial.println(F("not authorized - check the user's ACLs in MQTT Distributor")); break;
    case MQTT_CONNECT_BAD_CLIENT_ID:   Serial.println(F("client ID rejected")); break;
    default:
      Serial.print(F("state "));
      Serial.println(state);
      break;
  }
}

// Makes one connection attempt if the retry interval has passed.
// Returns immediately otherwise, so loop() keeps running while disconnected.
void maintainMqtt() {
  static unsigned long lastAttempt = 0;
  static bool firstAttempt = true;

  if (mqttClient.connected()) return;

  unsigned long now = millis();
  if (!firstAttempt && now - lastAttempt < MQTT_RETRY_MS) return;
  firstAttempt = false;
  lastAttempt = now;

  Serial.print(F("Connecting to MQTT broker... "));
  if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword)) {
    Serial.println(F("connected."));
  } else {
    Serial.print(F("failed: "));
    printMqttState(mqttClient.state());
  }
}

void publishCounter() {
  static unsigned long lastPublish = 0;

  unsigned long now = millis();
  if (now - lastPublish < PUBLISH_INTERVAL_MS) return;
  lastPublish = now;

  if (!mqttClient.connected()) return;

  counter++;
  char payload[32];
  snprintf(payload, sizeof(payload), "{\"count\":%lu}", counter);

  Serial.print(F("Publish "));
  Serial.print(mqttTopic);
  Serial.print(' ');
  Serial.print(payload);
  Serial.println(mqttClient.publish(mqttTopic, payload) ? F(" OK") : F(" FAILED"));
}

void setup() {
  Serial.begin(9600);

  // An uninitialized SD CS pin can float low and corrupt Ethernet SPI traffic.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Ethernet.init(ETHERNET_CS);
  startEthernet();

  ethClient.setConnectionTimeout(TCP_CONNECT_TIMEOUT_MS);
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setSocketTimeout(MQTT_REPLY_TIMEOUT_S);
}

void loop() {
  if (usingDhcp) {
    Ethernet.maintain();  // renews the DHCP lease before it runs out
  }

  maintainMqtt();
  mqttClient.loop();  // sends keep-alive pings so the broker doesn't drop us
  publishCounter();
}
