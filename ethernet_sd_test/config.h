/*
  Network configuration for the Ethernet + SD shield test.
  Copied from SDSemi-Spin-Coater/arduinoEthernet/config.h since it's the same
  physical shield - edit if you're testing on different hardware/network.
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

#endif
