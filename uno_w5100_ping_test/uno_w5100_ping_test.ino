/*
  Ethernet ping test for Arduino Uno R3 + W5100 Ethernet shield.

  Standalone hardware bring-up test - NOT part of SDSemi-Spin-Coater. It's the
  first step toward the Uno talking MQTT to the Ignition gateway. It:
    1. Gets an IP address from the router over DHCP, using the MAC in config.h.
       With no router (Uno cabled straight to the laptop), it falls back to
       the static IP in config.h.
    2. Every 5s, pings the router (DHCP only) and the Ignition/broker PC, then
       checks that the MQTT port (1883) on that PC accepts a TCP connection.
  Reading the three results together shows where a problem is:
    router FAIL                    -> cable, shield, or the Uno's network setup
    (direct to laptop: Ignition FAIL with SEND_TIMEOUT means the same thing,
     or the laptop's Ethernet port isn't set to the IP in config.h)
    router OK,  Ignition PC FAIL   -> wrong PC address, or Windows firewall
                                      blocking ping (it does by default)
    ping OK,    MQTT port FAIL     -> broker not running, or firewall blocking 1883

  Uses ICMPPing.h/.cpp in this folder (Blake Foster's Arduino-Ping library,
  ported to Ethernet library 2.x - handles the W5100 as well as the W5500).
  Needs only the built-in SPI and Ethernet libraries.

  Wiring (standard W5100 shield, plugged straight onto the Uno):
    - W5100 talks over SPI on the ICSP header (D11/D12/D13)
    - Ethernet (W5100) chip select: pin 10
    - SD card chip select:          pin 4 (held HIGH so the card stays off the bus)

  Note: ping() and connect() each block for up to ~1s. That's fine in a test,
  but don't copy this loop into the spin coater as it is - a blocked loop()
  can't update the motor.

  The W5100 replies to pings in hardware, so you can also ping the Uno from
  the PC (ping <IP printed at startup>) while this runs.

  Open the Serial Monitor at 9600 baud after uploading.
*/

#include <SPI.h>
#include "config.h"
#include "ICMPPing.h"

const uint8_t ETHERNET_CS = 10;
const uint8_t SD_CS = 4;

const int PING_RETRIES = 2;
const unsigned long TEST_INTERVAL_MS = 5000;
const unsigned long MQTT_CONNECT_TIMEOUT_MS = 1000;

// The W5100 has only 4 sockets. Use the highest one so the Ethernet library's
// own sockets (DHCP, TCP - handed out from 0 upward) don't collide with the
// raw ICMP socket.
const uint8_t PING_SOCKET = MAX_SOCK_NUM - 1;
ICMPPing ping(PING_SOCKET, 0x42);

// False when DHCP got no answer and the static IP from config.h is in use.
bool usingDhcp = false;

// Pings addr and prints one result line. Returns true on a reply.
bool pingHost(const __FlashStringHelper *label, IPAddress addr) {
  Serial.print(label);
  Serial.print(addr);
  Serial.print(F(" ... "));

  unsigned long sentAt = millis();
  ICMPEchoReply reply = ping(addr, PING_RETRIES);

  if (reply.status == SUCCESS) {
    Serial.print(F("OK "));
    Serial.print(millis() - reply.data.time);
    Serial.println(F("ms"));
    return true;
  }

  Serial.print(F("FAIL after "));
  Serial.print(millis() - sentAt);
  Serial.print(F("ms: "));
  switch (reply.status) {
    case SEND_TIMEOUT: Serial.println(F("SEND_TIMEOUT (no ARP reply - host off or wrong subnet?)")); break;
    case NO_RESPONSE:  Serial.println(F("NO_RESPONSE (host didn't answer - firewall?)")); break;
    case BAD_RESPONSE: Serial.println(F("BAD_RESPONSE (TTL exceeded en route)")); break;
    default:           Serial.println((int)reply.status); break;
  }
  return false;
}

// Opens a TCP connection to the MQTT port and closes it straight away -
// enough to prove the broker is listening, without speaking MQTT.
bool checkMqttPort() {
  Serial.print(F("MQTT port  "));
  Serial.print(ignitionAddr);
  Serial.print(':');
  Serial.print(mqttPort);
  Serial.print(F(" ... "));

  EthernetClient client;
  client.setConnectionTimeout(MQTT_CONNECT_TIMEOUT_MS);
  bool open = client.connect(ignitionAddr, mqttPort);
  client.stop();

  Serial.println(open ? F("OPEN") : F("CLOSED (broker not running, or firewall blocking the port)"));
  return open;
}

void printMac() {
  for (uint8_t i = 0; i < sizeof(mac); i++) {
    if (mac[i] < 0x10) Serial.print('0');
    Serial.print(mac[i], HEX);
    if (i < sizeof(mac) - 1) Serial.print(':');
  }
  Serial.println();
}

void startEthernet() {
  Serial.print(F("MAC: "));
  printMac();

  Serial.println(F("Requesting IP over DHCP..."));
  usingDhcp = Ethernet.begin(mac, DHCP_TIMEOUT_MS, DHCP_RESPONSE_MS);

  // begin() resets the chip even when DHCP fails, so the hardware check is
  // valid here - it tells "no shield" apart from "no DHCP server".
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(F("Ethernet shield not found - is it seated on the Uno?"));
    while (true) delay(1000);
  }

  if (!usingDhcp) {
    // No router - most likely cabled straight to the laptop. There's no
    // gateway either, so the laptop stands in for it (only used for
    // addresses off the 192.168.2.x subnet, which this test never sends to).
    Serial.println(F("No DHCP reply - using static IP from config.h."));
    Ethernet.begin(mac, staticIp, ignitionAddr, ignitionAddr, staticSubnet);
  }

  if (Ethernet.hardwareStatus() != EthernetW5100) {
    Serial.println(F("Note: chip isn't a W5100 - test still runs."));
  }

  Serial.print(F("IP:      "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet:  "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("Gateway: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("DNS:     "));
  Serial.println(Ethernet.dnsServerIP());
  Serial.println();
}

void setup() {
  Serial.begin(9600);

  // An uninitialized SD CS pin can float low and corrupt Ethernet SPI traffic.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Ethernet.init(ETHERNET_CS);
  startEthernet();
}

void loop() {
  if (usingDhcp) {
    // Renews the DHCP lease before it runs out. Cheap when nothing is due.
    switch (Ethernet.maintain()) {
      case 1: Serial.println(F("DHCP renew failed")); break;
      case 3: Serial.println(F("DHCP rebind failed")); break;
      case 4:
        Serial.print(F("DHCP rebound, IP now "));
        Serial.println(Ethernet.localIP());
        break;
    }

    // Only a DHCP network has a real router to ping.
    pingHost(F("Router     "), Ethernet.gatewayIP());
  }
  pingHost(F("Ignition   "), ignitionAddr);
  // Checked even if the ping failed: Windows blocks ping by default but may
  // still allow 1883.
  checkMqttPort();
  Serial.println();

  delay(TEST_INTERVAL_MS);
}
