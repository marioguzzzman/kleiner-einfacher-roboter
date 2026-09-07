# Recommendations — preparing to work on this project

A practical checklist for flashing and running the Little Dog (v2, ESP32-C3
SuperMini). Read this before a bench
session. Wiring details live in [HANDOFF_WIRING.md](HANDOFF_WIRING.md); the
*why* behind the gotchas is in [HANDOFF_LEARNINGS.md](HANDOFF_LEARNINGS.md).

---

## 1. Before you power on

- **Bench 5 V supply ON** into PCA9685 **V+** (servo power), limit ~3 A.
- **Common ground joined** — the bench supply's **(−) must tie into the same
  ground node** as the ESP32, PCA9685, sensor, and amp. If it doesn't, the
  servos get power but no valid signal and sit slack (looks like dead code, is
  actually wiring). See [HANDOFF_WIRING.md:93](HANDOFF_WIRING.md#L93).
- **Use a real USB *data* cable** (not charge-only), seated firmly. The
  SuperMini's USB-C jack is fragile — a flaky cable/connector is the #1 cause of
  the "port goes in and out."

## 2. Arduino IDE board setup

- **Board:** Tools → Board → **`esp32`** group (Espressif Systems) →
  **ESP32C3 Dev Module**. *Not* the "Arduino ESP32 Boards" group.
  - Confirm you're on the Espressif core: the Tools menu should show
    **USB CDC On Boot**, Flash Size, Partition Scheme, etc. If those options
    aren't there, you picked the wrong core.
- **USB CDC On Boot → Enabled.** Critical — without it the Serial Monitor stays
  blank on this board.
  - ⚠️ This is a **compile-time** option. Flipping the menu does nothing until
    you **re-upload** a sketch built with it enabled.
- **Baud:** Serial Monitor at **115200**.

## 3. If the COM port won't stay put

The SuperMini uses **native USB (USB-Serial/JTAG)**, so every reset
re-enumerates the port — it can appear to drop "in and out," and the COM number
hops on replug (we've seen COM3 / COM5 / COM9). This is expected behavior, not a
fault. Also try a **rear/motherboard USB port**, not a hub or front-panel port.

## 4. Uploading — force download mode

When a normal upload can't catch the cycling port, put the board into bootloader
mode by hand. This holds a stable port that doesn't run user code:

1. **Hold BOOT** (GPIO9) — keep holding.
2. While holding BOOT, **tap RESET** once, then release RESET.
3. **Release BOOT.** Now upload.

Order matters — BOOT must be held **across** the reset pulse; pressing both at
once does nothing. (One-button boards: hold BOOT while plugging in the USB.)

## 5. After the upload

- **"Hard resetting via RTS pin…" is normal** — it's just esptool pulsing reset
  so the chip boots your new sketch. Not an error.
- That reset **re-enumerates the USB port**, so the Serial Monitor from the
  upload is now stale. **Reselect the current COM** (re-check the port list) and
  **reopen the monitor at 115200**. Tap RESET once if nothing appears.

## 6. Verifying it actually works

- Serial Monitor should show `=== MOTORS-ONLY test ===` then
  **`found 0x40`** — that line confirms I2C + the PCA9685 are alive.
  `NONE found!` instead = SDA/SCL wiring or PCA9685 power, not the code.
- **Watch the legs, not just the log.** If servos sweep but the monitor is
  blank, the diagnostic is *passing* — only the logging is misrouted (see §2,
  USB CDC On Boot).
- Use the **`diagnostics/`** sketches to isolate one subsystem at a time
  (`motors_only`, `distance_only`, `audio_only`, …) — much faster than debugging
  the full `little_dog_walk` firmware.

## 7. Blank Serial Monitor — quick triage

1. Reopen the monitor on the **current** COM at **115200** (tap RESET first).
2. Still blank → **USB CDC On Boot = Enabled**, then **re-upload** (compile-time).
3. Still blank but **legs move** → the sketch is running; logging is on UART0
   (GPIO20/21), not USB. Fix via step 2.