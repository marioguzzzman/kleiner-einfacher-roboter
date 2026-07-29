# Mini Robot Swarm Unit — "Little Dog"

A small 3D-printed quadruped robot, built as one unit of a larger robotic swarm.
Each unit senses its surroundings, moves on four servo legs, and (from v2 onward)
has a synthesized "voice." This repository holds **two hardware/firmware
generations** of the same physical robot plus the **shared** mechanical body and
carrier PCB.

> The 3D body and the carrier PCB are common to both generations — only the
> microcontroller board and the firmware changed between v1 and v2.

---

## Two generations

| | **v1** | **v2** |
|---|--------|--------|
| Folder | [`v1/`](v1/) | [`v2/`](v2/) |
| MCU | Adafruit Feather ESP32 V2 | ESP32-C3 SuperMini |
| Servos | 4× MG90S | 4× MS18 (SG90-class) |
| Sensing | HC-SR04 ultrasonic | HC-SR04 ultrasonic (+ 1k/2k echo divider) |
| Audio | — | MAX98357A I2S amp + speaker ("modem voice") |
| Behavior | pose-based (sit / wave / walk / lay down) | trot + obstacle-stop (<10 cm) + voice on stop |
| I2C pins | GPIO21/22 | GPIO6/7 |
| Status | Phase 1 baseline | **electronics bring-up complete** |

**v2 is a continuation of v1**, not a separate project — same robot, new brain.

---

## Repository layout

```
kleiner-einfacher-roboter/
├── README.md                       ← you are here
├── LICENSE
├── robot_dog_pcb/                   KiCad carrier PCB          (SHARED)
├── little_robot_original_3d_stl/    printable body/legs/cover  (SHARED)
├── v1/   Feather ESP32 V2, pose-based
│   ├── README.md
│   ├── docs/          hardware.md · setup.md · code_explanation.md
│   └── src/           robot_dog.ino
└── v2/   ESP32-C3 SuperMini, walk + sensor + voice
    ├── README.md
    ├── firmware/
    │   ├── little_dog_walk/     ⭐ main program (walk + stop + voice)
    │   ├── little_dog_test_1/   full 5-part functionality test
    │   ├── little_dog_test_v2/  flag-based staged test
    │   ├── obstacle_stop/       walk + stop demo (simple chirp)
    │   └── diagnostics/         per-subsystem isolation sketches
    └── docs/          HANDOFF_WIRING.md · HANDOFF_LEARNINGS.md
```

---

## Shared hardware

- **Body:** `little_robot_original_3d_stl/` — `main_body.stl`, `leg.stl`, `cover.stl`.
  Based on the [Cobot Four-Servo-Legs Robot](https://makerworld.com/en/models/523448-cobot-four-servo-legs-and-distance-sensor-robot) on MakerWorld.
- **Carrier PCB:** `robot_dog_pcb/` — KiCad project (schematic, board, `.pdf` export).
  Modules mount as pluggable headers on the carrier.

---

## Getting started

- Building the **current** robot → see **[v2/README.md](v2/)** and
  [v2/docs/HANDOFF_WIRING.md](v2/docs/HANDOFF_WIRING.md) for the full wiring schematic.
- The **original** design → see **[v1/README.md](v1/)**.

---

## License

See [LICENSE](LICENSE). (Original note: all rights reserved.)

## Credits

- 3D model: [Cobot Four Servo Legs Robot](https://makerworld.com/en/models/523448-cobot-four-servo-legs-and-distance-sensor-robot) on MakerWorld
- Built with Arduino / ESP32, PCA9685, MAX98357A, and Adafruit libraries.
