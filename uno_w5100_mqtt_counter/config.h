/*
  Network and MQTT configuration for the Uno R3 + W5100 MQTT counter.
  Edit these values to match your setup.
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Ethernet.h>

// Same MAC as uno_w5100_ping_test - it's the same shield. The 0x02 in the
// first byte marks it as user-assigned, so it can't clash with real hardware.
// Change the last byte if a second W5100 board joins the network.
byte mac[] = { 0x02, 0x53, 0x44, 0x53, 0x45, 0x4D };

// DHCP is tried first (router / Raspberry Pi network). If nothing answers -
// e.g. cabled straight to the laptop - the static IP below is used instead.
const unsigned long DHCP_TIMEOUT_MS = 5000;
const unsigned long DHCP_RESPONSE_MS = 2000;
IPAddress staticIp(192, 168, 2, 12);
IPAddress staticSubnet(255, 255, 255, 0);

// The PC running Ignition with the MQTT Distributor (broker) module.
// For now that's the laptop's Ethernet port; later, the Raspberry Pi.
IPAddress mqttBroker(192, 168, 2, 10);
const uint16_t mqttPort = 1883;

// A user from MQTT Distributor's Users list in the Ignition gateway.
// admin / changeme is the module's default - change both there and here.
const char mqttUser[] = "admin";
const char mqttPassword[] = "changeme";

// Must be unique on the broker - two clients with the same ID will
// repeatedly kick each other off.
const char mqttClientId[] = "unoW5100";

// MQTT Engine needs a Custom Namespace subscribed to this topic (e.g.
// "arduino/#") to turn the messages into Ignition tags.
const char mqttTopic[] = "arduino/uno";

#endif
