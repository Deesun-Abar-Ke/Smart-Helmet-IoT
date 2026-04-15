# 🏍️ IoT Smart Helmet: Accident Detection & Health Monitoring System

An Arduino-based IoT Smart Helmet designed to enhance rider safety through real-time physiological monitoring, environmental sensing, and automated emergency SOS alerts. 

This system continuously fuses data from multiple sensors to detect critical falls, monitor the rider's heart rate and environment for heat-stress risks, and provide proximity warnings. In the event of a severe accident, it autonomously initiates a phone call and sends an SMS alert with vital statistics to emergency contacts.

---

## ✨ Key Features

* **💥 Advanced Fall Detection:** Utilizes an MPU6050 (Accelerometer + Gyroscope) paired with an SW-420 Vibration sensor to accurately detect high-impact falls and orientation changes, minimizing false positives.
* **❤️ Health & Vitals Monitoring:** Tracks real-time Beats Per Minute (BPM) using a heart rate sensor with a custom dynamic-threshold filtering algorithm.
* **🌡️ Environmental Context:** Monitors ambient temperature and humidity via a DHT22 sensor to assess the risk of heatstroke or rider fatigue.
* **🚧 Proximity Warning:** Uses an HC-SR04 Ultrasonic sensor to detect obstacles and provide short-range proximity awareness in dense traffic.
* **📡 Automated SOS (GSM):** Integrates a SIM800L GSM module to automatically dial emergency contacts and dispatch an SMS containing the rider's vital signs and environmental context upon crash detection.

---

## 🛠️ Hardware Architecture

### Microcontroller
* **Arduino UNO:** The central processing hub that handles sensor fusion and emergency logic.

### Sensors & Modules
* **MPU6050:** 6-axis Accelerometer and Gyroscope (I2C)
* **DHT22:** Digital Temperature and Humidity Sensor
* **HC-SR04:** Ultrasonic Proximity Sensor
* **SW-420:** Vibration Sensor Module
* **Heart Rate Sensor:** Analog pulse sensor (Compatible with MAX30102 conceptual architecture)
* **SIM800L:** GSM Module for Cellular Communication (Requires stable 4.0V-4.2V power supply)

---

## 🔌 Circuit & Pin Mapping

Ensure your components are wired to the Arduino UNO exactly as mapped below:

| Component | Arduino Pin | Notes |
| :--- | :--- | :--- |
| **DHT22** | `D4` | Data pin |
| **HC-SR04 (Ultrasonic)** | `D8` (TRIG), `D9` (ECHO) | |
| **SW-420 (Vibration)**| `D12` | Digital Input |
| **Heart Rate Sensor** | `A0` | Analog Input |
| **MPU6050** | `A4` (SDA), `A5` (SCL) | I2C Communication |
| **SIM800L (GSM)** | `D10` (TX -> SIM RX), `D11` (RX -> SIM TX) | Uses SoftwareSerial |

> **⚠️ Power Warning for SIM800L:** The SIM800L module can draw up to 2A during transmission. Do *not* power it directly from the Arduino's 5V pin. Use a dedicated buck converter (like LM2596) tuned to ~4.0V - 4.2V, and ensure the Arduino and external power supply share a common Ground.

---

## 💻 Software & Libraries

To compile and upload the code, you will need the Arduino IDE and the following libraries installed:
* `Wire.h` (Built-in I2C library)
* `SoftwareSerial.h` (Built-in serial communication)
* [`MPU6050` by Electronic Cats](https://github.com/ElectronicCats/mpu6050) (or equivalent I2Cdev implementation)
* [`DHT sensor library` by Adafruit](https://github.com/adafruit/DHT-sensor-library)

---

## ⚙️ How It Works (Core Logic)

### 1. The Fall Detection Algorithm
The system samples the MPU6050 every 50ms to calculate the total squared acceleration vector ($ax^2 + ay^2 + az^2$). If this value drops below the `fallThreshold` (indicating a free-fall or sudden severe impact state) **AND** the SW-420 vibration sensor registers a physical shock, the system confirms a crash.

### 2. Emergency Protocol
Once a crash is confirmed, the Arduino triggers the `makeCall()` and `sendSMSAlert()` functions. It dials the predefined emergency number, rings for 20 seconds, hangs up, and then dispatches an SMS structured as:
```text
ALERT: Fall detected! Immediate help required.
BPM: 85
Temp: 32.5 C
Hum: 65.0 %
Dist: 120 cm
