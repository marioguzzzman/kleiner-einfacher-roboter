# HANDOFF — Little Dog (unit #1) — WIRING & CONNECTIONS
Last updated: 2026-07-29 — full electronics bring-up completed this session.
Status: **ALL SUBSYSTEMS WORKING** (servos, I2C/PCA9685, HC-SR04 distance, MAX98357A audio).
Flagship firmware: `little_dog_walk/little_dog_walk.ino` (walk + obstacle-stop + modem voice).

> This file = the physical wiring as actually built and verified on the bench.
> Companion file: `HANDOFF_LEARNINGS.md` (why things are wired this way + gotchas).

---

## MODULES
- **MCU:** ESP32-C3 SuperMini (USB-C, 3.3V logic)
- **Servo driver:** PCA9685 (HW-170, 16ch, I2C @ **0x40**)
- **Distance:** HC-SR04 (5V version) + 1k/2k ECHO divider
- **Audio:** MAX98357A (I2S mono class-D amp) + 8Ω 0.25W speaker
- **Actuators:** 4× MS18 micro servos (SG90-class), fore/aft swing
- **Servo power (bench):** lab supply set to **5.0 V, limit ~3 A**, straight into PCA9685 V+.
  (The XL4005 buck from the earlier plan is currently **BYPASSED** — not in the circuit.)

---

## SYSTEM SCHEMATIC (as built)

```
        USB-C ──► [PC]  (powers ESP32 + native-USB serial, and VBUS=5V rail)
          │
          ▼
 ┌───────────────────────────┐
 │   ESP32-C3 SuperMini      │       ── I2C ──►  ┌──────────────────────┐
 │   (3.3 V logic)           │  GPIO6 SDA ─────► │   PCA9685  (0x40)    │
 │                           │  GPIO7 SCL ─────► │                      │
 │  GPIO0 ───────────────────┼──────────► TRIG   │  VCC ◄─ 3.3V (logic) │
 │  GPIO1 ◄──[1k/2k divider]─┼─── ECHO           │  V+  ◄─ BENCH 5V     │
 │  GPIO3 ───────────────────┼──► BCLK           │  OE  ─ (unconnected) │
 │  GPIO4 ───────────────────┼──► LRC            │  ch0 ─► servo BL(?)  │
 │  GPIO5 ───────────────────┼──► DIN            │  ch1 ─► servo BR(?)  │
 │  GPIO10  (unused, ex-OE)  │                   │  ch2 ─► servo FL(?)  │
 │                           │                   │  ch3 ─► servo FR(?)  │
 │  3.3V ─┬─► PCA9685 VCC     │                   └──────────────────────┘
 │        └─► MAX98357A SD    │                    (? = leg map UNVERIFIED)
 │  5V   ─┬─► HC-SR04 VCC     │
 │        └─► MAX98357A Vin   │       ┌──────────────────────┐
 │  GND ─► COMMON GROUND ─────┼─────► │   MAX98357A          │
 └───────────────────────────┘  BCLK►│  BCLK  LRC  DIN       │
                                 LRC► │  Vin ◄─ 5V            │
                                 DIN► │  SD  ◄─ 3.3V (ENABLE) │
                                      │  GAIN ─ GND (15 dB)*  │
                                      │  OUT+ ─┐              │
                                      │  OUT- ─┴─► 8Ω speaker │
                                      └──────────────────────┘

   HC-SR04:  VCC◄5V   GND►common   TRIG◄GPIO0   ECHO►divider►GPIO1
```
\* GAIN→GND is CONNECTED (max 15 dB) and working. Note: overall loudness is
   limited by the small speaker, not the amp — even a slightly bigger speaker
   was only marginally louder. A 4Ω 3–5W driver in a small enclosure is what
   would actually get loud. `g_volume` is currently 1.0 in the firmware.

---

## PIN MAP (ESP32-C3 → everything)

| GPIO  | Net                         | Notes |
|-------|-----------------------------|-------|
| GPIO0 | HC-SR04 TRIG                | direct, 3.3V out is fine for trigger |
| GPIO1 | HC-SR04 ECHO (via divider)  | 5V echo dropped to 3.3V by 1k/2k |
| GPIO2 | —                           | strapping pin, leave empty |
| GPIO3 | MAX98357A BCLK              | I2S bit clock |
| GPIO4 | MAX98357A LRC               | I2S word select |
| GPIO5 | MAX98357A DIN               | I2S data |
| GPIO6 | PCA9685 SDA                 | `Wire.begin(6,7)` |
| GPIO7 | PCA9685 SCL                 | |
| GPIO8 | —                           | strapping + onboard LED, leave empty |
| GPIO9 | —                           | strapping + boot button, leave empty |
| GPIO10| **UNUSED** (was PCA9685 OE) | OE now left unconnected — see below |
| GPIO20| — (UART RX, free)           | |
| GPIO21| — (UART TX, free)           | |

---

## POWER RAILS

| Rail | Source | Feeds |
|------|--------|-------|
| **3.3V** | ESP32 3.3V pin | PCA9685 **VCC** (logic), MAX98357A **SD** (enable) |
| **5V (logic side)** | ESP32 5V pin (USB VBUS) | HC-SR04 **VCC**, MAX98357A **Vin** |
| **5V (servo side)** | **bench supply, 5.0V / ~3A limit** | PCA9685 **V+** (servo power ONLY) |
| **GND** | common star node | everything below |

> IMPORTANT: PCA9685 **VCC (3.3V logic)** and **V+ (5V servo power)** are SEPARATE
> on purpose. Do NOT feed VCC from 5V. Servo current lives on V+ only.

### COMMON GROUND (single net — star-joined)
All of these MUST be one electrical node:
- ESP32 GND
- PCA9685 GND (header + screw terminal)
- **Bench supply (−)**  ← the servo supply's negative MUST join here
- MAX98357A GND
- HC-SR04 GND
- bottom of ECHO divider (R2 → GND)

If the bench (−) is not tied to this node, servos get power but **no valid signal**
(they draw ~13 mA and sit slack). This bit us hard — see `HANDOFF_LEARNINGS.md`.

---

## ECHO DIVIDER (built on perfboard)

```
 HC-SR04 ECHO (5V) ──[ R1 = 1k ]──┬──► GPIO1   (junction ≈ 3.3V)
                                  │
                              [ R2 = 2k ]
                                  │
                                 GND (common)
```
TRIG has no divider (it's an output from the ESP32).

---

## OE — why it's disconnected (do NOT re-wire it)
The PCA9685 breakout has an **onboard pull-down on OE**, so OE floats to LOW =
outputs **enabled by default**. Leave OE **unconnected**. Driving it from GPIO10
is unnecessary and, half-wired/floating, caused the "servo twitch" saga this
session. Confirmed against Adafruit's PCA9685 pinout docs.
(If you ever want a software kill-switch back, wire OE to a spare GPIO and drive
it HIGH=disable / LOW=enable — but that's optional.)

---

## SERVO REFERENCE
- 50 Hz, 4096 ticks = 20 ms.  1.0ms=205 · **1.5ms=307 (CENTER)** · 2.0ms=410
- Firmware uses `CENTER=307`, `SWING=60` (gentle, protects cheap gears).
- **Leg map is UNVERIFIED** (ch0..3 → which physical leg + swing direction).
  Run Part 2 of `little_dog_test_1.ino` and watch. This blocks a real directional gait.

---

## TOOLCHAIN (for flashing)
- arduino-cli (bundled with Arduino IDE):
  `C:\Users\mario\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`
- Board: **ESP32C3 Dev Module**, FQBN with USB serial:
  `esp32:esp32:esp32c3:CDCOnBoot=cdc`
- In Arduino IDE: Tools → Board → **esp32 → ESP32C3 Dev Module**, and
  **USB CDC On Boot → Enabled** (else Serial Monitor is blank).
- Port hops between COM3/COM5 on replug — re-check `arduino-cli board list`.
```bash
"C:\Users\mario\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc "C:\Users\mario\Downloads\little_dog_walk"
```
