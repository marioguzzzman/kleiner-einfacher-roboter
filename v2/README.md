# v2 — ESP32-C3 SuperMini rebuild (walk + obstacle-stop + voice)

Second generation of the Little Dog quadruped. Same 3D body as
[v1](../v1/), but rebuilt around an **ESP32-C3 SuperMini** and given a
**synthesized voice** (a dial-up-modem "handshake") via an I2S amplifier.
Electronics bring-up is **complete and verified** — the robot trots, freezes
when something comes within 10 cm, and "speaks" when it stops.

> Full wiring (ASCII schematic, power rails, common-ground star) and the
> debugging lessons live in **[docs/HANDOFF_WIRING.md](docs/HANDOFF_WIRING.md)**
> and **[docs/HANDOFF_LEARNINGS.md](docs/HANDOFF_LEARNINGS.md)**. Read those first.

---

## Hardware

| Component | Role |
|-----------|------|
| ESP32-C3 SuperMini | MCU (USB-C, 3.3 V logic) |
| PCA9685 (HW-170) | 16-ch PWM servo driver, I2C @ **0x40** |
| 4× MS18 micro servos | legs (fore/aft swing) |
| HC-SR04 + 1k/2k divider | distance sensor ("eyes") |
| MAX98357A + speaker | I2S mono amp — the "voice" |
| 5 V bench supply (~3 A) | servo power into PCA9685 V+ |

## Pin map (ESP32-C3)

| GPIO | Net | | GPIO | Net |
|------|-----|-|------|-----|
| 0 | HC-SR04 TRIG        | | 6 | PCA9685 SDA |
| 1 | HC-SR04 ECHO (÷)    | | 7 | PCA9685 SCL |
| 3 | MAX98357A BCLK      | | 10 | (unused — ex-OE) |
| 4 | MAX98357A LRC       | | | |
| 5 | MAX98357A DIN       | | | |

Power: PCA9685 **VCC ← 3.3 V** (logic) and **V+ ← 5 V** (servos) are separate.
MAX98357A **SD → 3.3 V** (enable), **GAIN → GND** (15 dB). **All grounds common**,
including the bench supply's negative. See the schematic in `docs/`.

---

## Firmware

Flagship program: **`firmware/little_dog_walk/`** — trots via diagonal pairs,
freezes all legs when an obstacle is within `STOP_CM` (10 cm), and plays the
modem-handshake voice on the stop edge. Tunables at the top: `g_volume`,
`SWING`, `STOP_CM`, cadence `delay()`.

| Sketch | Purpose |
|--------|---------|
| `little_dog_walk/` | ⭐ main: walk + obstacle-stop + modem voice |
| `little_dog_test_1/` | full 5-part test (center → per-leg → trot → distance → chirp) |
| `little_dog_test_v2/` | flag-based staged test (`TEST_LEGS/DISTANCE/AUDIO`) |
| `obstacle_stop/` | earlier walk+stop demo with a simple two-tone chirp |
| `diagnostics/esp32_alive/` | bare MCU sanity + LED blink |
| `diagnostics/motors_only/` | PCA9685 + servo sweep in isolation |
| `diagnostics/hold_center/` | park servos, measure holding current |
| `diagnostics/distance_only/` | HC-SR04 raw pulse + cm |
| `diagnostics/audio_only/` | amp test + I2S bytes-written diagnostic |
| `diagnostics/modem_chirp/` | standalone dial-up "voice" for tuning |

The `diagnostics/` sketches exist so each subsystem can be verified alone —
invaluable when something doesn't work (see the learnings doc).

---

## Build & flash

**Board:** ESP32C3 Dev Module. **Critical:** set **`USB CDC On Boot → Enabled`**
or the USB-C serial monitor stays blank.

Arduino IDE → Tools → Board → *esp32* → **ESP32C3 Dev Module**, then
`USB CDC On Boot: Enabled`, pick the port, Upload.

CLI equivalent:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc firmware/little_dog_walk
arduino-cli upload  -p COM3 --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc firmware/little_dog_walk
```
Libraries: **Adafruit PWM Servo Driver** (+ Adafruit BusIO). I2S uses the
built-in ESP32 `driver/i2s.h`.

---

## What changed from v1

- MCU **Feather ESP32 V2 → ESP32-C3 SuperMini** (3.3 V logic, native USB-C).
- I2C moved to **GPIO6/7** (v1 used 21/22); OE left unconnected (onboard pulldown).
- Added the **MAX98357A "voice"** and the **obstacle-stop** behavior.
- Servos **MG90S → MS18**; distance echo now through a **1k/2k divider**.

## Status / next

Bring-up done. Remaining before a proper directional gait:
**verify the leg map** (which PCA9685 channel drives which physical leg, and swing
direction) — run `little_dog_test_1` Part 2 and watch. Then add per-servo
direction sign + per-leg center trim. See `docs/HANDOFF_LEARNINGS.md`.
