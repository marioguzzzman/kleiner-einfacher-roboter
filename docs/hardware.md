# Hardware Guide

## Components List

| Component | Description | Quantity |
|-----------|-------------|----------|
| Adafruit Feather ESP32 V2 | Main microcontroller with WiFi | 1 |
| PCA9685 16-Channel PWM Driver | Servo motor driver board | 1 |
| MG90S Microservos | 9g metal gear servos for legs and arms | 4 |
| HC-SR04 Ultrasonic Distance Sensor | For detecting nearby objects | 1 |
| External 5V Power Supply | 2A minimum for servos | 1 |
| Jumper Wires | Dupont cables for connections | Various |
| Breadboard | Optional, for organizing connections | 1 |

---

## Wiring Diagram

### PCA9685 to Feather ESP32 V2

```
+==============================================================+
|                    PCA9685 Servo Driver                     |
+==============================================================+
|                                                              |
|   VCC (Logic Power) ----+----> 3V  (Feather 3.3V output)    |
|   GND  (Ground) --------+----> GND (Feather GND)           |
|   SDA  (I2C Data) -------+----> SDA (Feather SDA/GPIO 21)    |
|   SCL  (I2C Clock) ------+----> SCL (Feather SCL/GPIO 22)    |
|   V+   (Servo Power) ---+----> External 5V Supply            |
|                                                              |
|   +========================================================+ |
|   |  Servo Channels (Yellow Header)                        | |
|   +========================================================+ |
|   | CH0: Left Leg                                         | |
|   | CH1: Right Leg                                        | |
|   | CH2: Left Arm                                         | |
|   | CH3: Right Arm                                        | |
|   +========================================================+ |
|                                                              |
|   External Power Input (Green Screw Terminal):               |
|   +5V -----> External 5V Power Supply (+)                   |
|   GND -----> External 5V Power Supply (-)                  |
|             +-> Feather GND (MUST be connected!)             |
|                                                              |
+==============================================================+
```

### HC-SR04 Distance Sensor to Feather ESP32 V2

```
+==============================================================+
|                  HC-SR04 Ultrasonic Sensor                  |
+==============================================================+

    +-----------+         Feather ESP32 V2
    |  HC-SR04  |
    +-----------+
    | VCC       |-----> USB (5V from Feather)
    | GND       |-----> GND (Feather GND)
    | TRIG      |-----> A3 (GPIO 4)
    | ECHO      |-----> A2 (GPIO 3)  [Optional: Use voltage divider]
    +-----------+

+==============================================================+
```

### Power Distribution Summary

```
+==============================================================+
|                     POWER FLOW DIAGRAM                      |
+==============================================================+

    USB Cable                     External 5V Supply
    from PC                           |
        |                             |
        v                             v
  +===========+               +===============+
  |  Feather  |               |   Green      |
  |  ESP32 V2 |               |  Terminal    |
  +===========+               +===============+
        |                             |
        | 3V -----> VCC (PCA9685 logic)
        | GND --+-> GND (PCA9685)
        |       |-> GND (External supply bridge)
        |       +-> Feather GND (CRITICAL!)
        |                             |
        v                             v
  +===============================+
  |        PCA9685 Servo Driver   |
  +===============================+
        |                   |
        v                   v
    Servo Power       Servo Channels
    (V+ rail)         CH0-CH3
                      |
                      v
                  Servos (4x)
                  MG90S

+==============================================================+
```

### Servo Channel Mapping

```
+==============================================================+
|               SERVO CHANNEL ASSIGNMENTS                     |
+==============================================================+

    PCA9685
    +-------+
    | CH0   |-----> Left Leg
    | CH1   |-----> Right Leg
    | CH2   |-----> Left Arm
    | CH3   |-----> Right Arm
    +-------+

    Each servo wire connection (looking at yellow header):
    +===+===+===+===+===+===+===+===+
    | G | V | S | G | V | S | G | V | ...  (GND, V+, Signal)
    +===+===+===+===+===+===+===+===+
      0   1   2   3   4   5   6   7  ...

+==============================================================+
```

---

## Critical Notes

### 1. Ground Connection (CRITICAL!)
All grounds MUST be connected together:
- Feather GND
- PCA9685 GND
- External 5V supply GND

Without this common ground, PWM signals will not work correctly.

### 2. Power Requirements
- Servos require 5V at 2A minimum
- USB power from Feather is NOT sufficient for multiple servos
- Use external 5V power supply connected to green terminal

### 3. Servo Signal Direction
The following servos are physically mirrored and need angle inversion:
- RIGHT_LEG (CH1): Inverted
- LEFT_ARM (CH2): Inverted
- RIGHT_ARM (CH3): Inverted

LEFT_LEG (CH0) is normal (no inversion).

---

## Optional: Voltage Divider for HC-SR04 ECHO

The HC-SR04 outputs 5V on ECHO, but Feather ESP32 uses 3.3V logic.
If you have resistors available, use this voltage divider:

```
ECHO pin ----[ 1k Ohm ]---- GPIO A2 (Pin 3)
                          |
                    [ 2k Ohm ]
                          |
                         GND
```

This creates a 3.3V signal from the 5V ECHO output.

**Note:** Many users report success without this divider for testing.
For permanent installations, add the voltage divider for safety.
