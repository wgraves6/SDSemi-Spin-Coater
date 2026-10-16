/*
  Network configuration for the Uno R3 + W5100 ping test.
  Edit these values to match your setup.
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Ethernet.h>

// The W5100 has no MAC burned in - the sketch has to supply one, and it must
// be unique on the network. This shield has no sticker, so this is a
// "locally administered" MAC: the 0x02 in the first byte marks it as
// user-assigned, so it can never clash with a real vendor's hardware.
// The last five bytes spell "SDSEM" in ASCII. If you add a second W5100 board,
// change the last byte (e.g. 0x4E) so the two don't fight over the address.
byte mac[] = { 0x02, 0x53, 0x44, 0x53, 0x45, 0x4D };

// On a network with a router (the Raspberry Pi setup later), the router hands
// out the Uno's IP over DHCP, and the router to ping is learned from DHCP too.
const unsigned long DHCP_TIMEOUT_MS = 5000;   // give up on DHCP after this
const unsigned long DHCP_RESPONSE_MS = 2000;  // wait this long for each DHCP reply

// Used when DHCP gets no answer - e.g. the Uno cabled straight to the laptop,
// where there's no router to hand out addresses. The laptop's Ethernet port
// must be set by hand to an address on the same 192.168.2.x subnet.
IPAddress staticIp(192, 168, 2, 12);
IPAddress staticSubnet(255, 255, 255, 0);

// The PC running the MQTT broker / Ignition gateway. For now that's the
// laptop (set its Ethernet port to this IP); later, the Raspberry Pi.
IPAddress ignitionAddr(192, 168, 2, 10);

// The MQTT broker's port. The test opens and closes a TCP connection here to
// confirm the broker is listening - ping alone only proves the PC is on.
const uint16_t mqttPort = 1883;

#endif
