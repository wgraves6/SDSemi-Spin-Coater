/*
  MQTT test publish for Arduino R4 WiFi + Arduino Ethernet Shield (W5500)

  Connects to the network over Ethernet, then connects to an MQTT broker
  and publishes a JSON payload - a fixed message string plus a counter that
  cycles 1 through 10 - on a timer.

  Requires three libraries (Sketch > Include Library > Manage Libraries...):
    - "Ethernet2" by Arduino         - for the W5500 shield chip.
    - "PubSubClient" by Nick O'Leary - lightweight MQTT client.
    - "ArduinoJson" by Benoit Blanchon - builds the JSON payload.
  If your shield turns out to be the older W5100 chip instead, swap the
  include in config.h from <Ethernet2.h> to <Ethernet.h> (the standard,
  built-in Ethernet library) and nothing else needs to change.

  All network and MQTT settings (MAC, static IP, broker address, credentials,
  topic) live in config.h, in the same folder as this sketch — edit them there.

  Before testing, make sure an MQTT broker (e.g. Mosquitto) is running and
  reachable at the address in config.h, with a user "will" configured to
  match the password in config.h. To watch the published messages, run on
  the broker machine:
    mosquitto_sub -h localhost -t arduino/test -u will -P password
  Then upload this sketch and open the Arduino Serial Monitor at 9600 baud.
*/

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

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

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for Serial Monitor to open (native USB on the R4)
  }

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
}

void loop() {
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  static unsigned long lastPublish = 0;
  static int counter = 1;
  unsigned long now = millis();
  if (now - lastPublish >= 5000) {
    lastPublish = now;

    JsonDocument doc;
    doc["message"] = "Dad was here.";
    doc["message2"] = "William was also here.";
    doc["count"] = counter;

    char payload[128];
    serializeJson(doc, payload);

    Serial.print("Publishing: ");
    Serial.println(payload);
    mqttClient.publish(mqttTopic, payload);

    counter = (counter % 10) + 1;
  }
}
