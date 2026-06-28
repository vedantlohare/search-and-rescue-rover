# 🚀 Project VANGUARD - Search & Rescue Rover

> A modular search-and-rescue robotic rover featuring a tracked chassis, 3-DOF robotic arm, browser-based computer vision, real-time telemetry, obstacle sensing, and wireless teleoperation.

<p align="center">
  <img src="docs/demo.gif" width="750" alt="Project Demo">
</p>

> **Note:** Demo GIF and screenshots will be added soon.

---

## Features

* 🚜 Tracked differential-drive rover
* 🤖 3-DOF robotic arm with servo control
* 📡 Wireless teleoperation over WiFi (UDP)
* 📹 Live ESP32-CAM video streaming
* 🧠 Browser-based object detection using TensorFlow.js (COCO-SSD)
* 📍 Radar-style obstacle visualization
* 🎤 Acoustic localization
* 📏 ToF distance sensing
* 📋 Live telemetry console
* 📝 Automatic mission logging
* ⚡ Multi-threaded Python command-and-control station

---

## System Overview

Project VANGUARD is a modular embedded robotics platform developed to explore low-cost search-and-rescue applications.

The rover combines embedded systems, computer vision, wireless networking, and human-in-the-loop control to assist operators in navigating hazardous environments while maintaining situational awareness through real-time telemetry and sensor visualization.

Instead of performing AI inference onboard, the ESP32-CAM streams live video to the base station where object detection is executed inside the browser using TensorFlow.js. This reduces computational load on the embedded hardware while providing responsive detection capabilities.

---

## Hardware

| Component             | Purpose                 |
| --------------------- | ----------------------- |
| ESP32-WROOM           | Main rover controller   |
| ESP32-CAM             | Live video streaming    |
| Arduino UNO           | Robotic arm controller  |
| VL53L0X ToF Sensor    | Obstacle detection      |
| MEMS Microphone Array | Acoustic localization   |
| MG996R Servos         | Shoulder & elbow joints |
| SG90 Servo            | Claw actuation          |
| DC Motors             | Differential drive      |

---

## Tech Stack

### Embedded

* ESP32
* Arduino UNO
* C++

### Base Station

* Python
* CustomTkinter
* UDP Socket Programming
* Multithreading

### Computer Vision

* ESP32-CAM
* TensorFlow.js
* COCO-SSD
* HTML
* CSS
* JavaScript

---

## Repository Structure

```text
VANGUARD_PROJECT/
│
├── README.md
├── LICENSE
├── .gitignore
│
├── base_station/
│   ├── vanguard_gui.py
│   ├── vanguard_vision.html
│   ├── requirements.txt
│   └── assets/
│
├── firmware/
│   ├── node1_vision/
│   ├── node2_brain/
│   └── node3_muscle/
│
├── hardware/
│   ├── wiring_diagrams/
│   ├── BOM.md
│   └── Hardware_Overview.md
│
└── docs/
```

---

## System Architecture

```text
                    +-------------------------+
                    |     Base Station        |
                    | Python GUI + Browser    |
                    +-----------+-------------+
                                |
                        WiFi (UDP Communication)
                                |
        +-----------------------+----------------------+
        |                                              |
+-------v--------+                           +----------v---------+
|  ESP32-WROOM   |                           |     ESP32-CAM      |
| Main Controller|                           | Live Video Stream  |
+-------+--------+                           +--------------------+
        |
 Serial Communication
        |
+-------v--------+
|  Arduino UNO   |
| Servo Control  |
+-------+--------+
        |
+-------v--------+
| Robotic Arm    |
+----------------+
```

---

## Project Highlights

* Distributed multi-controller embedded architecture
* Wireless command-and-control using UDP
* Browser-based AI inference with TensorFlow.js
* Real-time telemetry visualization
* Multi-threaded Python operator interface
* Sensor fusion through ToF and acoustic localization
* Live mission logging and operator alerts
* Modular firmware for independent hardware nodes
---

# Getting Started

## Prerequisites

Before running the project, ensure the following software is installed on your system:

* Python 3.10 or later
* Arduino IDE
* Google Chrome (recommended)
* Git

---

## Clone the Repository

```bash
git clone https://github.com/<your-username>/VANGUARD_PROJECT.git

cd VANGUARD_PROJECT
```

---

## Install Dependencies

Navigate to the base station directory and install the required Python packages.

```bash
cd base_station

pip install -r requirements.txt
```

---

# Configuration

Before running the project, update the network configuration according to your local WiFi network.

### 1. ESP32 Controller IP

Open

```text
base_station/vanguard_gui.py
```

Update

```python
self.esp32_ip = "YOUR_ESP32_IP"
```

---

### 2. ESP32-CAM Stream URL

Open

```text
base_station/vanguard_vision.html
```

Replace

```html
src="http://YOUR_CAMERA_IP:81/stream"
```

with the IP address of your ESP32-CAM.

---

### 3. Laptop IP Address

Open

```text
firmware/node2_brain/node2_brain.ino
```

Update

```cpp
const char* targetIP = "YOUR_LAPTOP_IP";
```

to the IPv4 address of the computer running the Python base station.

---

### 4. WiFi Credentials

Update the following in both

```text
firmware/node1_vision/node1_vision.ino
```

and

```text
firmware/node2_brain/node2_brain.ino
```

```cpp
const char* ssid = "YOUR_WIFI_NAME";

const char* password = "YOUR_WIFI_PASSWORD";
```

Ensure all devices are connected to the same WiFi network.

---

# Running the Project

## Step 1

Flash the firmware to the respective boards using the Arduino IDE.

| Firmware     | Board       |
| ------------ | ----------- |
| node1_vision | ESP32-CAM   |
| node2_brain  | ESP32-WROOM |
| node3_muscle | Arduino UNO |

---

## Step 2

Power the rover and wait for all embedded controllers to connect to the WiFi network.

---

## Step 3

Launch the Python base station.

Use whichever command matches your Python installation.

Examples:

```bash
cd base_station

python vanguard_gui.py
```

or

```bash
cd base_station

py -3.13 vanguard_gui.py
```

or any equivalent command for your local Python environment.

---

## Step 4

Open

```text
base_station/vanguard_vision.html
```

using Google Chrome.

The browser will automatically load the TensorFlow.js model and begin processing the live camera stream.

---

## Step 5

Drive the rover using the keyboard.

| Key | Action     |
| --- | ---------- |
| W   | Forward    |
| A   | Turn Left  |
| S   | Reverse    |
| D   | Turn Right |

The GUI sliders control

* Shoulder Joint
* Elbow Joint
* Claw Actuation

---

# Screenshots

The following screenshots will be added soon.

| Feature             | Preview     |
| ------------------- | ----------- |
| Base Station GUI    | Coming Soon |
| Vision System       | Coming Soon |
| Radar Visualization | Coming Soon |
| Hardware Platform   | Coming Soon |

---

# Future Improvements

Some planned enhancements include:

* Autonomous waypoint navigation
* SLAM-based environment mapping
* ROS2 integration
* Thermal camera support
* Battery health monitoring
* Joystick / Gamepad control
* Autonomous victim tracking
* Multi-rover coordination
* Cloud-based mission logging

---

# License

This project is released under the MIT License.

See the `LICENSE` file for more information.

---

# Author

**Vedant Lohare**

B.Tech Aerospace Engineering

Indian Institute of Technology Kanpur

---

## If you found this project interesting, consider giving the repository a ⭐.
