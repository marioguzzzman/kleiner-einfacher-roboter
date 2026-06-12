# Software Setup Guide

## Arduino IDE Setup

### Step 1: Install ESP32 Board Definitions

1. Open Arduino IDE
2. Go to **File > Preferences**
3. In the "Additional Board Manager URLs" field, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click "OK"
5. Go to **Tools > Board > Boards Manager**
6. Search for "ESP32"
7. Install "ESP32 by Espressif Systems"

### Step 2: Select Your Board

1. Connect your Feather ESP32 V2 via USB-C
2. Go to **Tools > Board**
3. Select **"Adafruit Feather ESP32-S2"**
4. Go to **Tools > Port**
5. Select the COM port (e.g., COM4, /dev/ttyUSB0)

### Step 3: Install Required Libraries

1. Go to **Sketch > Include Library > Manage Libraries**
2. Search for and install these libraries:

| Library | Author | Purpose |
|---------|--------|---------|
| Adafruit PWM Servo Driver | Adafruit | PCA9685 control |
| NewPing | Tim Eckel | HC-SR04 ultrasonic sensor |

### Step 4: Verify Board Connection

Upload this test sketch to verify connection:

```cpp
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 connected!");
}

void loop() {
}
```

Open **Tools > Serial Monitor** (Ctrl+Shift+M) and set baud to **115200**.
You should see "ESP32 connected!" printed.

---

## Troubleshooting Board Connection

### Board Not Detected?

1. Try a different USB-C cable (some are charge-only)
2. Press the **BOOT** button on Feather while uploading
3. Install USB drivers:
   - **CP2104** driver for older Feather boards
   - **CH9102** driver for newer boards

### No COM Port Visible?

Windows:
```
1. Open Device Manager (Win + R, type "devmgmt.msc")
2. Look under "Ports (COM & LPT)"
3. Install driver if needed
```

Linux:
```bash
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

macOS:
```bash
ls /dev/tty.*
```

---

## Uploading the Code

1. Open `src/robot_dog/robot_dog.ino` in Arduino IDE
2. Verify board settings:
   - Board: Adafruit Feather ESP32-S2
   - Port: (Your COM port)
   - Upload Speed: 115200
3. Click the **Upload** button (arrow icon)
4. Wait for "Done uploading"
5. Open Serial Monitor to see robot output

---

## Finding Your COM Port

### Windows - Device Manager
1. Press `Windows + R`
2. Type `devmgmt.msc`
3. Expand "Ports (COM & LPT)"
4. Look for "Silicon Labs CP210x" or "USB Serial Device"

### macOS - Terminal
```bash
ls /dev/tty.SLAB_USBtoUART
```

### Linux - Terminal
```bash
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

### Quick Trick
1. Unplug the Feather
2. Note the COM ports listed
3. Plug in the Feather
4. The NEW port that appears is your board

---

## Serial Monitor Settings

- Baud Rate: **115200**
- Line Ending: **Newline** or **Both NL & CR**

Expected output on boot:
```
=================================
Mini Robot Swarm Unit - Booting...
=================================
Applying neutral pose.
Robot ready!
Distance: XX cm
```
