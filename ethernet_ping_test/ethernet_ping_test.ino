#include <SPI.h>
#include <Ethernet.h>
#include "ICMPPing.h"

// Define MAC and Static IP for your shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 177 }; // Adjust for your network
IPAddress pingAddr(192, 168, 1, 1); // Target to ping

SOCKET pingSocket = 0;
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));

void setup() {
  Ethernet.begin(mac, ip);
  Serial.begin(9600);
}

void loop() {
  // Send ICMP Echo Request
  ICMPEchoReply reply = ping(pingAddr, 4);
  if (reply.status == SUCCESS) {
    Serial.print("Ping successful! Time: ");
    Serial.print(millis() - reply.data.time);
    Serial.println("ms");
  } else {
    Serial.println("Ping failed");
  }
  delay(5000);
}
