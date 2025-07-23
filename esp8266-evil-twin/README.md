\# ESP8266 Evil Twin Attack Node



This repository contains firmware and setup instructions for configuring an ESP8266 device to perform an \*\*Evil Twin WiFi attack\*\*, based on the logic of the \[DevilTwin-ESP8266](https://github.com/Deborshibd/DevilTwin-ESP8266) project.



\## 📦 Contents



\- `bin/evil\_twin\_firmware.bin` – Precompiled firmware used to run the Evil Twin attack

\- `flasher/` – Instructions to flash the firmware to ESP8266

\- `docs/` – Overview of the Evil Twin attack and research goals



\## 🧪 About the Attack



An Evil Twin attack creates a \*\*rogue Wi-Fi access point\*\* with the \*\*same SSID\*\* as a real one. Victims may unknowingly connect to the rogue AP, allowing attackers to:

\- Intercept credentials

\- Redirect users to fake login portals

\- Deny access to legitimate APs



\## 🛠 Setup Instructions



To flash and use the device, see \[`flashing\_guide.txt`](flasher/flashing\_guide.txt).



Once flashed:

1\. The device will broadcast a Wi-Fi access point

2\. Connect to it and open `http://192.168.4.1/admin` in your browser

3\. Select the SSID you want to mimic and start the Evil Twin attack



> This firmware setup and interface are structured similarly to the ESP8266 Deauther v1 project.



\## ⚠️ Disclaimer



This project is for \*\*educational and research use only\*\*.  

Do \*\*not\*\* deploy this on any real network without permission.



\## 🙏 Credits



\- Interface and flashing method inspired by \[Spacehuhn’s ESP8266 Deauther v1](https://github.com/SpacehuhnTech/esp8266\_deauther/tree/v1)

\- Attack logic adapted from \[DevilTwin-ESP8266](https://github.com/Deborshibd/DevilTwin-ESP8266)



