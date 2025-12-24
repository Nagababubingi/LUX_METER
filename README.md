# ESP8266 NodeMCU Lux Meter with Web UI and OTA

## 📌 Project Overview
This project implements a **real-time lux meter** using an **ESP8266 NodeMCU** microcontroller and an LDR sensor to measure ambient light intensity. Illuminance values are computed and visualized through a **responsive web dashboard**. The system also supports **Over-The-Air (OTA)** firmware updates, making maintenance and updates easy without physical access.

## ⚙️ Features
- Real-time lux measurement
- Web-based dashboard UI
- OTA firmware updates
- Data logging support
- Simple and low-cost IoT implementation

## 🛠️ Hardware Components
- ESP8266 NodeMCU
- LDR (Light Dependent Resistor)
- 10 kΩ resistor
- Breadboard and jumper wires

## 🔌 Circuit Connection
3.3V → 10kΩ → A0 → LDR → GND
## 🔌 Circuit Diagram
![ESP8266 NodeMCU Lux Meter Circuit](images/circuit.png)

## ⚡ Functionality
- The LDR and resistor form a voltage divider.
- The ESP8266 reads the analog input and converts it to resistance.
- Resistance is translated to an approximate lux value.
- Lux values are shown live on both the web UI and via OTA updates.

## 📝 Usage
1. Flash the firmware to the NodeMCU.
2. Connect to WiFi.
3. Open a browser and visit the device IP to view the web UI.
4. Use OTA to update firmware remotely if needed.

## 📂 Repo Contents
- `src/` — Project source code
- `platformio.ini` — PlatformIO project configuration
- `README.md` — This file

---
