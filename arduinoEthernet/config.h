/*
  Network configuration for the Ethernet connectivity test.
  Edit these values to match your setup.
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Ethernet.h>

// MAC address printed on the shield's sticker.
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x73, 0xC5 };

// Static IP assigned to the Arduino itself.
IPAddress ip(192, 168, 2, 11);

// Router/gateway address on the Arduino's subnet (192.168.2.x).
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

// MQTT broker to connect to.
IPAddress mqttBroker(192, 168, 2, 10);
const uint16_t mqttPort = 1883;

// Broker credentials.
const char mqttUser[] = "will";
const char mqttPassword[] = "password";

// Must be unique on the broker - two clients with the same ID will
// repeatedly kick each other off.
const char mqttClientId[] = "arduinoR4WiFi";

// Topic the Arduino publishes test messages to.
const char mqttTopic[] = "arduino/test";

#endif
