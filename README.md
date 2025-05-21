# Smart Parking Occupancy 🚗📊

An ESP32-based smart parking system that uses IoT cloud monitoring and QR code authentication for secure vehicle exit control.

## 🔧 Project Overview

This project aims to manage parking lot occupancy and exit control using sensors, cloud dashboards, and QR-based user interaction. All data is monitored via Arduino IoT Cloud and visualized on a dashboard.

## Features

- 🚘 Vehicle detection using HC-SR04 ultrasonic sensor  
- 🌡️ Temperature and humidity monitoring with DHT11  
- 📶 Real-time variable sync with Arduino Cloud (MQTT)  
- 🔓 Secure exit control via QR code (local web server)  
- 📲 Manual servo control via dashboard toggle  
- 📈 Dashboard graphs for revenue and parking durations  
- 🖼 OLED display for status, messages, and QR code  
- 💡 RGB LED to indicate occupancy status

## 🛠️ Hardware Used

| Component        | Description                              |
|------------------|------------------------------------------|
| ESP32 Dev Board  | Core microcontroller                     |
| HC-SR04          | Ultrasonic distance sensor               |
| DHT11            | Temperature and humidity sensor          |
| OLED (SH1106)    | 128x64 screen for status and QR display  |
| RGB LED          | Occupancy status indicator               |
| Servo Motor      | Simulates parking gate                   |
| LDR              | Optional light sensing                   |


## System Architecture
<img src="https://github.com/user-attachments/assets/96710e68-a9e6-47fa-9737-34cabb7545da" width=30% height=30%>
<img src="https://github.com/user-attachments/assets/1c89c96d-04bd-4e05-96d8-b0b8fc6b96bc" width=30% height=30%>


## Dashboard Preview

- Temperature / Humidity
- Occupancy %
- Daily Income
- Last Park Duration
- Servo Control Toggle
- QR Activation Log
<img src="https://github.com/user-attachments/assets/1f3543c9-ce18-4e16-90db-ee57e8f7b4f0" width=30% height=30%>
<img src="https://github.com/user-attachments/assets/9def991b-9618-4eef-bc6e-375a36835bca" width=30% height=30%>

## QR Exit Control

- When a vehicle exits, a QR code appears on the OLED.
- The QR links to a local endpoint: `http://<esp32-ip>/unlock`
- Upon scanning, the ESP32 triggers the servo motor (gate opens).
- Prevents unauthorized exits and logs activity to the cloud.
