# HANDOFF — Little Dog (unit #1) — WHAT WE LEARNED
Last updated: 2026-07-29 — bring-up + debugging session.
Companion: `HANDOFF_WIRING.md` (the physical wiring).

---

## HEADLINE
Full electronics bring-up is **DONE**. The robot walks, stops when something is
within 10 cm, and plays a dial-up-modem "voice" when it stops. Every subsystem
is verified working. The main remaining task is **leg-map verification** before a
real directional gait can be written.

---

## KEY LEARNINGS / GOTCHAS (in the order they bit us)

1. **PCA9685 OE has an onboard pull-down → leave OE UNCONNECTED.**
   Half-wired/floating OE was the cause of the long "servos only twitch" saga.
   Outputs are enabled by default. (Confirmed on Adafruit's pinout page.)

2. **"Servos twitch / don't move" with I2C working = signal/ground, not power.**
   Diagnosis ladder that worked:
   - I2C finds 0x40 → command path OK.
   - Servo **draws ~13 mA and sits slack** → NO valid PWM reaching it.
     A servo that's actually being driven **holds firm** and draws **100–200 mA**.
   - "Hold at center" test + feeling the horn (firm vs slack) is the fastest check.

3. **Bench-supply current is a LIMIT, not a push.**
   Raising the amp knob does not force current into the servos — they draw what
   they need. Setting it too LOW makes the supply fold back (voltage sags → twitch).
   Set the limit high (≈3 A) and keep **voltage at 5.0 V**.

4. **Common ground is mandatory between the bench supply and the ESP32.**
   Two separate power sources → the servo PWM needs a shared ground reference.
   Missing common ground = powered but slack servos.

5. **USB CDC On Boot must be ENABLED** for the SuperMini's USB-C serial.
   Default is Disabled → Serial goes to UART pins → **blank Serial Monitor**.
   FQBN: `esp32:esp32:esp32c3:CDCOnBoot=cdc`. Baud 115200.

6. **The I2S chirp hung the whole sketch.**
   `i2s_write(..., portMAX_DELAY)` blocks FOREVER if the buffer isn't draining,
   which froze the loop so Parts 4 & 5 (and even leg names) never printed.
   **Fix:** use a finite timeout `pdMS_TO_TICKS(50)` and bail if `bw==0`.
   Also: print the label BEFORE the sensor read, so a slow read can't hide it.

7. **MAX98357A silence debugging:**
   - **SD pin must be tied to 3.3V to ENABLE the amp.** It has an internal
     pull-down, so floating = shutdown = silent. A floating SD can even read a
     phantom 3.3V on a multimeter — don't trust it, wire it solidly.
   - Needs **all three** I2S lines: BCLK, LRC, DIN. Missing LRC = silent.
   - Its class-D output is **not measurable with a DMM** (balanced ~300 kHz PWM).
     Trust your ears, not the meter, on OUT+/OUT-.
   - **GAIN pin = hardware volume** (GAIN→GND = 15 dB max). Not software-settable.
   - **Software volume = scaling the sample amplitude** (`g_volume` 0..1). Both
     stages multiply: software amplitude → hardware GAIN → speaker.
   - Speaker is only 8Ω **0.25 W** — fine for brief chirps; a 4Ω 3W speaker later
     would be much louder and safer at full gain.
   - **Confirmed:** GAIN→GND (15 dB) is wired and works, `g_volume=1.0`, yet it's
     still not very loud — the **small speaker is the ceiling**, not the amp or
     the code. A slightly bigger speaker helped only marginally; a real 4Ω 3–5W
     driver in an enclosure is the fix if more volume is ever needed.

8. **I2C on GPIO6/7 works** and avoids the C3 strapping pins (GPIO2/8/9).
   One reference calls 6/7 "SPI-default," but `Wire.begin(6,7)` remaps fine and
   0x40 is found reliably. Only move it if you ever see I2C dropouts.

9. **Serial-read trick (for scripted reads):** on the C3's USB-JTAG, RTS = reset.
   Open the port with **RTS de-asserted** to avoid rebooting the board into the
   bootloader; the COM number also changes on replug (COM3 ↔ COM5).

---

## THE LITTLE PROGRAMS (sketch inventory)
All live as sibling folders in `C:\Users\mario\Downloads\`.

| Sketch | What it does (briefly) |
|--------|------------------------|
| **little_dog_walk/** | ⭐ FLAGSHIP. Trots (diagonal pairs); freezes all legs when something <10 cm; plays the modem-handshake "voice" on the stop edge. Has the full audio engine + median distance. |
| **little_dog_test_1/** | Original 5-part functionality test: center → move each leg one at a time (prints leg name — USE FOR LEG-MAP) → diagonal-pair trot preview → distance print → chirp. I2S now hang-proofed. |
| **esp32_alive/** | Bare MCU sanity check — boots, prints chip info, blinks onboard LED (GPIO8). Proves the ESP32 alone works. |
| **hold_center/** | Parks all 4 servos at center and holds; prints I2C + heartbeat. Used to measure holding current (firm/100–200 mA vs slack/13 mA). |
| **motors_only/** | PCA9685 + servos only (no sensor/audio). Big slow sweep per channel, then all together. Isolates the servo path. |
| **distance_only/** | HC-SR04 only. Prints raw echo pulse (µs) + cm. Confirmed the sensor + divider work. |
| **audio_only/** | MAX98357A only. Loud continuous tone; prints I2S bytes-written per burst to tell "I2S OK but amp silent" from "I2S not running." |
| **modem_chirp/** | Standalone dial-up "voice" so the sound can be tuned by ear on a loop. This sound is now folded into little_dog_walk. |
| **obstacle_stop/** | Earlier version of the flagship: walk + stop-at-10cm with a SIMPLE two-tone chirp (before the modem voice was integrated). Superseded by little_dog_walk. |
| **little_dog_test_v2/** | Mario's own sketch — NOT created in this session and not reviewed here. Provenance/contents unknown to the bring-up agent. |

---

## DONE (2026-07-29)
- Verified ESP32, I2C/PCA9685 (0x40), all 4 servos, HC-SR04, MAX98357A audio.
- Root-caused & fixed: OE twitch (leave OE unconnected), I2S hang (finite timeout),
  silent amp (SD→3.3V + all 3 I2S lines), blank serial (USB CDC On Boot).
- Wired bench 5V straight to PCA9685 V+ (buck bypassed for now).
- Built the modem "voice" + integrated walk + obstacle-stop + voice.

## STILL PENDING / NEXT
- **Leg-map verification (BLOCKING a real gait):** run Part 2 of `little_dog_test_1`
  and record, for ch0–ch3, which physical leg moves and which way (front/back) first.
- **Directional gait:** once mapped, add per-servo direction sign (cancel L/R mirror)
  and per-leg center-trim offsets. Current "trot" is an uncalibrated preview.
- **Battery + buck:** final untethered unit still needs a power decision (buck back in,
  or a second buck for the ESP32). Not acquired yet.
- **Bigger speaker** (4Ω 3–5W in an enclosure) is the only real way to get more
  volume — GAIN→GND + g_volume=1.0 are already maxed; the speaker is the limit.
