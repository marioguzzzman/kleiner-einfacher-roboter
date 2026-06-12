# Mini Robot Swarm Unit

A desktop companion robot powered by ESP32 with pose-based movement system. This robot features 4 servos for leg and arm movement, ultrasonic distance sensing, and smooth interpolated motion.

![Robot Concept](docs/images/placeholder.png)

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Software Setup](#software-setup)
- [Calibration](#calibration)
- [Usage](#usage)
- [Documentation](#documentation)
- [Troubleshooting](#troubleshooting)
- [Future Work](#future-work)

---

## Overview

This project is a Phase 1 implementation of a desktop companion robot that can perform basic movements:
- Sit
- Wave
- Walk forward/backward
- Lay down

The robot uses pose-based control, allowing for easy extension of new movements and behaviors.

**Hardware:** Adafruit Feather ESP32 V2 + PCA9685 PWM Driver + 4x MG90S Servos + HC-SR04 Ultrasonic Sensor

---

## Features

- Pose-based movement system with smooth transitions
- Calibration mode via built-in button (SW38)
- Servo mirroring for mirrored limb configurations
- Distance sensor integration for reactive behavior
- Modular code structure for easy extension

---

## Hardware Requirements

| Component | Description | Quantity |
|-----------|-------------|----------|
| Adafruit Feather ESP32 V2 | Main microcontroller | 1 |
| PCA9685 16-Channel PWM Driver | Servo motor driver | 1 |
| MG90S Microservos | 9g metal gear servos | 4 |
| HC-SR04 Ultrasonic Sensor | Distance detection | 1 |
| External 5V Power Supply (2A+) | Servo power | 1 |
| Jumper Wires | Dupont cables | Various |

**Source for 3D Model:** [Cobot Four Servo Legs Robot on MakerWorld](https://makerworld.com/en/models/523448-cobot-four-servo-legs-and-distance-sensor-robot)

---

## Wiring Diagram

### PCA9685 to Feather ESP32 V2

```
PCA9685                           Feather ESP32 V2
-------                           ---------------
VCC (Logic Power)    ------>      3V (3.3V output)
GND (Ground)         ------>      GND
SDA (I2C Data)       ------>      SDA (GPIO 21)
SCL (I2C Clock)      ------>      SCL (GPIO 22)
V+ (Servo Power)    ------>      External 5V Supply

Green Terminal (External Power):
  +5V  ----> External 5V Supply (+)
  GND  ----> External 5V Supply (-)
        +--> Feather GND (REQUIRED!)
```

### Servo Channel Assignment

```
PCA9685 Servo Channels:
+---------+
| CH0     | ----> Left Leg
| CH1     | ----> Right Leg
| CH2     | ----> Left Arm
| CH3     | ----> Right Arm
+---------+
```

### HC-SR04 Distance Sensor

```
HC-SR04              Feather ESP32 V2
------              -----------------
VCC      ------>    USB (5V)
GND      ------>    GND
TRIG     ------>    A3 (GPIO 4)
ECHO     ------>    A2 (GPIO 3)
```

### Power Distribution

```
+============================================================+
|                      POWER FLOW                         |
+============================================================+

   USB (PC)                         External 5V Supply
       |                                    |
       v                                    v
  +=========+                        +============+
  | Feather  |                        | Green     |
  | ESP32 V2 |                        | Terminal  |
  +=========+                        +============+
       |                                    |
       | 3V ----> VCC (PCA9685 logic)      |
       | GND ---+> GND (PCA9685)            |
       |        |--> External GND bridge     |
       |        +--> Feather GND (CRITICAL!)|
       |                                    |
       v                                    v
  +========================================+
  |          PCA9685 Servo Driver          |
  +========================================+
       |                    |
       v                    v
  Servo Power (V+)      CH0-CH3
                         |
                         v
                    4x MG90S Servos

+============================================================+

IMPORTANT: All grounds must be connected together!
- Feather GND
- PCA9685 GND
- External 5V Supply GND
```

---

## Software Setup

### 1. Install ESP32 Board Definition

1. Open Arduino IDE
2. Go to **File > Preferences**
3. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools > Board > Boards Manager**
5. Search "ESP32" and install "ESP32 by Espressif Systems"

### 2. Select Board and Port

1. **Board:** Tools > Board > Adafruit Feather ESP32-S2
2. **Port:** Tools > Port > (Select your COM port)

### 3. Install Libraries

Go to **Sketch > Include Library > Manage Libraries** and install:

| Library | Author |
|---------|--------|
| Adafruit PWM Servo Driver | Adafruit |
| NewPing | Tim Eckel |

### 4. Upload Code

1. Open `src/robot_dog/robot_dog.ino`
2. Click Upload button
3. Open Serial Monitor (115200 baud)

---

## Calibration

### Why Calibrate?

Standard servos (MG90S) don't remember their position when powered off. On startup, they may be at a random angle. Calibration gently sweeps all servos to a known neutral position (90°).

### How to Calibrate

1. **Hold the SW38 button** on the Feather ESP32 V2
2. While holding, power on or press the Reset button
3. The robot will enter calibration mode
4. All servos will slowly sweep from 0° to 90° (~3.6 seconds)
5. Robot holds neutral pose for 1 second
6. Calibration complete!

### Without Calibration

If SW38 is not pressed during boot, the robot applies the neutral pose immediately.

### Manual Re-calibration

During normal operation, press SW38 at any time to trigger re-calibration.

---

## Usage

### Default Behavior

The robot cycles through these behaviors in the main loop:

| Distance | Behavior |
|----------|----------|
| < 15 cm | Wave gesture |
| >= 15 cm or no object | Walk Forward → Walk Backward → Lay Down → Sit |

### Serial Monitor Output

```
=================================
Mini Robot Swarm Unit - Booting...
=================================
Applying neutral pose.
Robot ready!
Distance: 50 cm
Action: Walk Forward
Action: Walk Backward
Action: Lay Down
Action: Sit
Distance: 8 cm
Action: Wave
```

### Extending Movement Behaviors

Add new poses in the code:

```cpp
Pose wavePose = {90, 90, 60, 90};
applyPose(wavePose);
```

Use smooth transitions:

```cpp
smoothServo(LEFT_ARM, 90, 60, 10);
delay(200);
smoothServo(LEFT_ARM, 60, 120, 10);
```

---

## Documentation

For detailed information, see the docs folder:

| Document | Contents |
|----------|----------|
| [docs/hardware.md](docs/hardware.md) | Complete wiring diagrams and power distribution |
| [docs/setup.md](docs/setup.md) | Arduino IDE setup and troubleshooting |
| [docs/code_explanation.md](docs/code_explanation.md) | Detailed code breakdown |

---

## Troubleshooting

### Servos Don't Move

1. **Check ground connections** - All GNDs must be connected
2. **Verify external power** - Servos need 5V from external supply
3. **Test with I2C Scanner** - Verify PCA9685 is detected:
   ```cpp
   #include <Wire.h>
   void setup() {
     Wire.begin();
     Serial.begin(115200);
     for (byte i = 1; i < 127; i++) {
       Wire.beginTransmission(i);
       if (Wire.endTransmission() == 0) {
         Serial.print("I2C found: 0x");
         Serial.println(i, HEX);
       }
     }
   }
   ```

### Code Uploads Fail

1. Press and hold **BOOT button** while clicking Upload
2. Try a different USB-C cable (some are charge-only)
3. Install USB drivers (CP2104 or CH9102)

### Servos Move Wrong Direction

- Check `applyAngle()` function in code
- Right leg and both arms may need angle inversion
- Mechanical solution: Realign servo horn

### Robot Jerky Movement

- Calibration may be too fast: Increase delay in `calibrateServos()`
- Use `smoothServo()` for gradual transitions

---

## Future Work

### Phase 2 Goals

- [ ] Web interface control (WiFi)
- [ ] Mobile app integration
- [ ] TouchDesigner communication (OSC/WebSocket)
- [ ] Multiple robot coordination
- [ ] Sound responses
- [ ] Idle animations and breathing effects

---

## License

No license for now. All rights reserved.

---

## Credits

- 3D Model: [Cobot Four Servo Legs Robot](https://makerworld.com/en/models/523448-cobot-four-servo-legs-and-distance-sensor-robot) on MakerWorld
- Built with Arduino, ESP32, and Adafruit components
