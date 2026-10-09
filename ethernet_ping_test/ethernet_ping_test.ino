/*
  Ethernet shield ICMP ping test, for Arduino Uno R4 WiFi + W5500 shield.

  Standalone hardware bring-up test - NOT part of SDSemi-Spin-Coater. Pings
  pingAddr (config.h) every 5s and prints the round-trip time.

  Uses ICMPPing.h/.cpp in this folder (Blake Foster's Arduino-Ping library,
  ported to Ethernet library 2.x). Needs only the built-in SPI and Ethernet
  libraries.

  Wiring assumed (standard Arduino Ethernet Shield 2 pinout):
    - Ethernet (W5500) chip select: pin 10
    - SD card chip select:          pin 4 (held HIGH so the card stays off the bus)

  Note: ping() blocks for up to PING_TIMEOUT (1s) per retry. Fine here, but
  don't drop it into the spin coater's loop() as-is - use the async API
  (ICMPPING_ASYNCH_ENABLE in ICMPPing.h) there.

  Open the Serial Monitor at 9600 baud after uploading.
*/

#include <SPI.h>
#include "config.h"
#include "ICMPPing.h"

const uint8_t ETHERNET_CS = 10;
const uint8_t SD_CS = 4;

const int PING_RETRIES = 4;
const unsigned long PING_INTERVAL_MS = 5000;

// Use the highest socket so the Ethernet library's own sockets (which it
// hands out from 0 upward) don't collide with the raw ICMP socket.
const uint8_t PING_SOCKET = MAX_SOCK_NUM - 1;
ICMPPing ping(PING_SOCKET, 0x42);

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000) {
    ; // wait for Serial Monitor (native USB on the R4), but don't hang forever
  }

  // An uninitialized SD CS pin can float low and corrupt Ethernet SPI traffic.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Ethernet.init(ETHERNET_CS);
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(1000); // let the shield's link come up

  switch (Ethernet.hardwareStatus()) {
    case EthernetNoHardware:
      Serial.println("Ethernet shield not found - check wiring/SPI connection.");
      while (true) delay(1000);
    case EthernetW5500:
      Serial.println("W5500 detected.");
      break;
    default:
      Serial.println("Ethernet chip detected, but it's not a W5500.");
      break;
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable not connected.");
  }

  Serial.print("Arduino IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Pinging ");
  Serial.println(pingAddr);
}

void loop() {
  unsigned long sentAt = millis();
  ICMPEchoReply reply = ping(pingAddr, PING_RETRIES);

  if (reply.status == SUCCESS) {
    Serial.print("Reply from ");
    Serial.print(reply.addr);
    Serial.print(": seq=");
    Serial.print(reply.data.seq);
    Serial.print(" time=");
    Serial.print(millis() - reply.data.time);
    Serial.println("ms");
  } else {
    Serial.print("Ping failed after ");
    Serial.print(millis() - sentAt);
    Serial.print("ms, status=");
    switch (reply.status) {
      case SEND_TIMEOUT: Serial.println("SEND_TIMEOUT (no ARP reply - host down or wrong subnet?)"); break;
      case NO_RESPONSE:  Serial.println("NO_RESPONSE (host didn't answer - firewall?)"); break;
      case BAD_RESPONSE: Serial.println("BAD_RESPONSE (TTL exceeded en route)"); break;
      default:           Serial.println((int)reply.status); break;
    }
  }

  delay(PING_INTERVAL_MS);
}
