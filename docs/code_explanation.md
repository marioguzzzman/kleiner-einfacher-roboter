# Code Explanation

This document explains how the robot dog code works, section by section.

---

## Overview

The code controls a 4-servo desktop companion robot using:
- **ESP32** microcontroller (Feather ESP32 V2)
- **PCA9685** PWM driver for servo control
- **HC-SR04** ultrasonic sensor for distance detection

The robot can sit, wave, walk forward/backward, and lay down.

---

## Code Structure

```cpp
// 1. Configuration (Constants and Pin Definitions)
// 2. Library Objects (PWM driver and Ultrasonic sensor)
// 3. Structs and Helpers (Pose structure, angle functions)
// 4. Poses (neutralPose, sittingPose)
// 5. Calibration Functions
// 6. Movement Functions
// 7. Setup
// 8. Main Loop
```

---

## 1. Configuration

```cpp
#define TRIG_PIN 4   // HC-SR04 Trig (A3 on Feather)
#define ECHO_PIN 3   // HC-SR04 Echo (A2 on Feather)
#define CALIBRATION_BUTTON_PIN 0  // SW38 button
```

These constants define which GPIO pins are used for:
- Ultrasonic sensor (TRIG and ECHO)
- Calibration button (built-in SW38)

```cpp
#define SERVOMIN 150    // Min pulse for 0 degrees
#define SERVOMAX 600    // Max pulse for 180 degrees
```

PCA9685 pulse width mapping:
- `SERVOMIN = 150` → 0° position
- `SERVOMAX = 600` → 180° position

```cpp
#define LEFT_LEG 0     // PCA9685 Channel 0
#define RIGHT_LEG 1     // PCA9685 Channel 1
#define LEFT_ARM 2      // PCA9685 Channel 2
#define RIGHT_ARM 3     // PCA9685 Channel 3
```

Channel assignments for the 4 servos.

---

## 2. Servo Mirroring: applyAngle()

```cpp
int applyAngle(uint8_t channel, int angle) {
  if (channel == RIGHT_LEG || channel == RIGHT_ARM) {
    return 180 - angle;
  } else if (channel == LEFT_ARM) {
    return 180 - angle;
  }
  return angle;
}
```

**Purpose:** Handle servos that are physically mounted in opposite orientation.

**Why needed:** Some servos are mounted facing the opposite direction. When you send angle 90, a normal servo goes "straight" - but a mirrored servo goes "straight" when sent 90 as well, but that makes it move the opposite way. This function inverts the angle for mirrored servos.

**Mapping:**
| Channel | Inverted? | Reason |
|---------|-----------|--------|
| LEFT_LEG (0) | No | Mounted normally |
| RIGHT_LEG (1) | Yes | Physically mirrored |
| LEFT_ARM (2) | Yes | Physically mirrored |
| RIGHT_ARM (3) | Yes | Physically mirrored |

---

## 3. Safe Range: clampAngle()

```cpp
int clampAngle(int angle) {
  return constrain(angle, 10, 170);
}
```

**Purpose:** Prevent servos from moving to their mechanical limits.

**Why:** Servos can strain, buzz, or stall at 0° and 180°. This function limits angles to a safe range (10°-170°).

---

## 4. Pulse Conversion: angleToPulse()

```cpp
int angleToPulse(int ang) {
  return map(clampAngle(ang), 0, 180, SERVOMIN, SERVOMAX);
}
```

**Purpose:** Convert an angle (0-180) to a PCA9685 pulse width value.

**Process:**
1. Clamp angle to safe range (10-170)
2. Map 0-180 to SERVOMIN-SERVOMAX (150-600)

---

## 5. Set Single Servo: setServo()

```cpp
void setServo(uint8_t channel, int angle) {
  int corrected = applyAngle(channel, angle);
  pwm.setPWM(channel, 0, angleToPulse(corrected));
}
```

**Purpose:** Set a single servo to a specific angle.

**Process:**
1. Apply mirroring correction (`applyAngle`)
2. Convert to pulse width (`angleToPulse`)
3. Send to PCA9685 (`pwm.setPWM`)

---

## 6. Pose System

```cpp
struct Pose {
  int leftLeg;
  int rightLeg;
  int leftArm;
  int rightArm;
};

Pose neutralPose = {90, 90, 90, 90};
Pose sittingPose = {0, 180, 90, 90};
```

**Purpose:** Define and apply full-body poses.

**How it works:**
- A `Pose` is a struct containing angles for all 4 servos
- `neutralPose` = all limbs at 90° (upright)
- `sittingPose` = legs bent inward, arms neutral

---

## 7. Apply Pose: applyPose()

```cpp
void applyPose(Pose p) {
  setServo(LEFT_LEG, p.leftLeg);
  setServo(RIGHT_LEG, p.rightLeg);
  setServo(LEFT_ARM, p.leftArm);
  setServo(RIGHT_ARM, p.rightArm);
  currentPose = p;
}
```

**Purpose:** Apply a complete pose to all 4 servos at once.

---

## 8. Smooth Movement: smoothServo()

```cpp
void smoothServo(uint8_t channel, int fromAngle, int toAngle, int stepDelay) {
  int step = (fromAngle < toAngle) ? 1 : -1;
  for (int angle = fromAngle; angle != toAngle; angle += step) {
    setServo(channel, angle);
    delay(stepDelay);
  }
  setServo(channel, toAngle);
}
```

**Purpose:** Move a servo gradually instead of snapping instantly.

**Example:**
```cpp
smoothServo(LEFT_ARM, 90, 60, 10);
// Moves arm from 90 to 60 degrees
// One degree at a time with 10ms delay between each step
```

**Why use it:** Creates more natural, organic movement instead of robotic snapping.

---

## 9. Calibration: calibrateServos()

```cpp
void calibrateServos() {
  Serial.println("Entering calibration mode...");
  for (int angle = 0; angle <= 90; angle++) {
    setServo(LEFT_LEG, angle);
    setServo(RIGHT_LEG, angle);
    setServo(LEFT_ARM, angle);
    setServo(RIGHT_ARM, angle);
    delay(40);
  }
  Serial.println("Calibration complete!");
  delay(1000);
}
```

**Purpose:** Gently move all servos to neutral position (90°) on startup.

**When to use:**
- Press and hold **SW38 button** (GPIO 0) during boot
- Robot enters calibration mode
- All servos sweep slowly from 0° to 90°
- Takes about 3.6 seconds (90 steps × 40ms)

**Why needed:** Standard servos don't remember their position when powered off. Calibration ensures all limbs start from a known position.

---

## 10. Setup

```cpp
void setup() {
  Serial.begin(115200);
  pwm.begin();
  pwm.setPWMFreq(50);  // 50Hz = standard servo frequency

  pinMode(CALIBRATION_BUTTON_PIN, INPUT);

  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    calibrateServos();
  } else {
    applyPose(neutralPose);
  }
}
```

**Process:**
1. Start serial communication (115200 baud)
2. Initialize PCA9685
3. Set PWM frequency to 50Hz
4. Check if SW38 button is pressed
   - Yes → Run calibration
   - No → Apply neutral pose

---

## 11. Main Loop

```cpp
void loop() {
  int distance = sonar.ping_cm();

  if (distance > 0 && distance < 15) {
    wave();
  } else {
    walkForward();
    delay(1000);
    walkBackward();
    delay(1000);
    layDown();
    delay(1000);
    sit();
  }
}
```

**Behavior:**
- Reads distance from ultrasonic sensor
- If something is within 15cm → Wave
- Otherwise → Cycle through walk forward, walk backward, lay down, sit

---

## Key Functions Summary

| Function | Purpose |
|----------|---------|
| `applyAngle()` | Mirror servo angles for reversed servos |
| `clampAngle()` | Limit angles to safe range (10-170) |
| `angleToPulse()` | Convert angle to PWM pulse width |
| `setServo()` | Set one servo to an angle |
| `applyPose()` | Apply all 4 servo angles at once |
| `smoothServo()` | Move servo gradually with interpolation |
| `calibrateServos()` | Sweep all servos to neutral position |
| `sit()`, `wave()`, etc. | Predefined movement patterns |

---

## Extending the Code

### Adding New Poses

```cpp
Pose myNewPose = {120, 60, 90, 90};

// Apply it:
applyPose(myNewPose);
```

### Adding Smooth Transitions

```cpp
// Move from current pose to new pose smoothly
smoothServo(LEFT_LEG, currentPose.leftLeg, 120, 10);
smoothServo(RIGHT_LEG, currentPose.rightLeg, 60, 10);
smoothServo(LEFT_ARM, currentPose.leftArm, 90, 10);
smoothServo(RIGHT_ARM, currentPose.rightArm, 90, 10);
```

### Custom Movement Sequences

```cpp
void customDance() {
  for (int i = 0; i < 3; i++) {
    applyPose(sittingPose);
    delay(500);
    applyPose(neutralPose);
    delay(500);
  }
}
```
