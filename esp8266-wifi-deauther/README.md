# ESP8266 WiFi Deauther Attack Node

This repository provides a precompiled binary and documentation for configuring an ESP8266 device as a **WiFi deauthentication attack node**, based on the setup from [Spacehuhn's ESP8266 Deauther v1](https://github.com/SpacehuhnTech/esp8266_deauther/tree/v1).

## 📦 Contents

- `bin/wifi_deauther_firmware.bin` – Precompiled firmware for ESP8266 (NodeMCU version).
- `flasher/` – Step-by-step instructions for flashing.
- `docs/` – Description of the attack and research context.

## 🧪 About the Attack

The deauthentication attack takes advantage of unprotected 802.11 management frames to:
- Disconnect devices from a Wi-Fi network
- Disrupt connectivity for denial-of-service testing
- Demonstrate security flaws in wireless protocols

## 🛠 Setup Instructions

To flash and use the device, follow the instructions in [`flashing_guide.txt`](flasher/flashing_guide.txt).

Once flashed:
1. Power the ESP8266 via USB
2. Connect to the Wi-Fi network it creates (e.g., `pwned`)
3. Open a browser and go to `http://192.168.4.1`
4. Use the web interface to scan for networks and launch the attack

> This setup mirrors the process from [esp8266_deauther v1](https://github.com/SpacehuhnTech/esp8266_deauther/tree/v1), without rebuilding the firmware.

## ⚠️ Legal Disclaimer

This repository is intended **strictly for educational and authorized penetration testing**. Do not use on any networks without permission.

## 🙏 Credits

- Firmware: [SpacehuhnTech/esp8266_deauther v1](https://github.com/SpacehuhnTech/esp8266_deauther/tree/v1)
- Flashing tool: [NodeMCU-PyFlasher](https://github.com/marcelstoer/nodemcu-pyflasher)
