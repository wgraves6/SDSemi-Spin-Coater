# Uno R3 + W5100 → Ignition over MQTT

Setting up from a Windows laptop, start to finish. The Uno publishes `{"count":N}` every 5 s to the topic `arduino/uno` on the MQTT broker built into Ignition, and Ignition turns it into a tag.

```
Uno R3 + W5100 shield ──Ethernet cable──> Laptop (192.168.2.10)
   192.168.2.12                             Ignition gateway
                                              ├─ MQTT Distributor (the broker, port 1883)
                                              └─ MQTT Engine (turns messages into tags)
```

## What you need

- Arduino Uno R3 and a W5100 Ethernet shield
- An Ethernet cable and a USB-B cable
- A Windows laptop with an Ethernet port (or a USB Ethernet adapter)
- Arduino IDE 2.x
- Ignition 8.3 with the Cirrus Link **MQTT Distributor** and **MQTT Engine** modules

## 1. Arduino IDE

1. Install Arduino IDE 2.x from arduino.cc.
2. **Tools → Board → Arduino AVR Boards → Arduino Uno.**
3. **Sketch → Include Library → Manage Libraries…**, then search for **PubSubClient** (by Nick O'Leary) and install it.
   The Ethernet and SPI libraries come with the IDE.

## 2. Ignition

1. Install Ignition 8.3 and open the gateway at `http://localhost:8088`.
2. Install the **MQTT Distributor** and **MQTT Engine** modules (free from Cirrus Link / Inductive Automation) from the gateway's **Modules** page.
3. **MQTT Distributor → Users:** note the username and password the Arduino will use.
   The default is `admin` / `changeme`. If you change it, change `mqttUser` / `mqttPassword` in `config.h` too.
4. **MQTT Engine → Namespaces → Custom → Create new:**
   | Field | Value |
   |---|---|
   | Name | `arduino` |
   | Subscriptions | `arduino/#` |
   | JSON Payload | ✅ |

   Without this step, Engine only listens for Sparkplug B messages and ignores the Uno's.

## 3. Laptop Ethernet port

A direct cable has no router, so nothing hands out addresses. Give the laptop's Ethernet port a fixed IP:

1. **Settings → Network & internet → Ethernet →** (your Ethernet adapter) **→ IP assignment → Edit → Manual**, then turn **IPv4** on.
2. IP address `192.168.2.10`, subnet mask `255.255.255.0`. Leave **gateway and DNS blank**.

WiFi keeps working for the internet, because the cable is its own separate network.
Leave the gateway blank, or Windows may try to send internet traffic down the Uno cable.

To undo this later, set IP assignment back to **Automatic (DHCP)**.

> If your WiFi network also uses `192.168.2.x`, the two networks clash. Move the cable network to another range, e.g. `192.168.50.x`.
> Change the laptop's IP and both `staticIp` and `mqttBroker` in `config.h` to match.

## 4. Wire up and upload

1. Plug the shield onto the Uno, run the Ethernet cable from the shield to the laptop, then connect USB.
2. Open `uno_w5100_mqtt_counter.ino`, choose the Uno's COM port under **Tools → Port**, and click **Upload**.
3. Open the **Serial Monitor** at **9600 baud**:
   ```
   Requesting IP over DHCP...
   No DHCP reply - using static IP from config.h.
   IP: 192.168.2.12
   Connecting to MQTT broker... connected.
   Publish arduino/uno {"count":1} OK
   ```
   The DHCP attempt takes about 5 s on a direct cable. That's normal.

## 5. Check it in Ignition

Open the **Designer**, switch the Tag Browser's provider to **MQTT Engine**, and expand **arduino**. The `count` tag should go up by 1 every 5 s.

## Troubleshooting

| What you see | Cause / fix |
|---|---|
| `Ethernet shield not found` | Shield isn't seated properly on the Uno's headers and the 6-pin ICSP header. |
| `TCP connect failed` | Laptop IP isn't `192.168.2.10`, cable unplugged, or Windows Firewall is blocking inbound TCP 1883 on the Ethernet port. |
| `bad username/password` | `config.h` doesn't match **MQTT Distributor → Users**. |
| `not authorized` | That user's permissions don't allow publishing to `arduino/#`. |
| Publishes `OK` but no tag | The Custom Namespace is missing, or its subscription doesn't match `arduino/uno`. |
| Tag holds the text `{"count":5}` | **JSON Payload** isn't ticked on the namespace. |

To test the bare network link, use `../uno_w5100_ping_test` first.
`ping 192.168.2.12` from the laptop should also get replies: the W5100 answers pings in hardware.
Pings *from* the Uno to the laptop fail by default, because Windows Firewall drops them on "Public" networks. That's harmless.

## Moving to the Raspberry Pi / a router later

- **Uno and Pi both on a router:** the Uno gets its IP over DHCP automatically. Only change `mqttBroker` in `config.h` to the Pi's IP.
- **Uno cabled straight to the Pi:** give the Pi `192.168.2.10` with no gateway, exactly like the laptop above.
