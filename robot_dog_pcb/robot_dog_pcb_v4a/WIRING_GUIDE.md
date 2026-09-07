# v3 Carrier Schematic — Wiring Guide (matches robot v2)

The carrier holds four **pluggable module headers** (modules stay removable/repairable):
U1 ESP32-C3 SuperMini · U2 MAX98357A amp · U3 PCA9685 servo driver · J3 HC-SR04 sensor.
Servos plug into the PCA9685 module directly; the speaker wires to the amp module
directly — so the carrier only carries the interconnect + divider + power.

Connect by **net label** (two same-named labels = one net) for signals, and
**power symbols** (+5V / +3V3 / GND) for the rails.

## Signal nets
| Net label | Pin A          | Pin B                |
|-----------|----------------|----------------------|
| TRIG      | U1 GPIO0       | J3 TRIG pin          |
| ECHO      | U1 GPIO1       | divider node (below) |
| BCLK      | U1 GPIO3       | U2 BCLK              |
| LRCLK     | U1 GPIO4       | U2 LRCLK             |
| DIN       | U1 GPIO5       | U2 DIN               |
| SDA       | U1 GPIO6       | U3 SDA               |
| SCL       | U1 GPIO7       | U3 SCL               |

## ECHO divider (5V echo -> 3.3V)
```
J3 ECHO(5V) --[ R1 = 1k ]--+-- node (label ECHO -> U1 GPIO1)
                           |
                        [ R2 = 2k ]
                           |
                          GND
```

## Power
- +3V3 (from U1 3V3) -> U3 VCC  and  U2 SD          (SD high = amp enabled)
- +5V  (screw-terminal rail) -> U3 V+, U2 VDD, J3 VCC
- GND  (common) -> U1 GND, U2 GND, U3 GND, J3 GND, R2 bottom, U2 GAIN (GAIN->GND = 15 dB)
- U3 ~OE  -> leave unconnected (place a no-connect flag; onboard pulldown enables outputs)
- U2 VO+/VO- -> no-connect on carrier (speaker attaches at the amp module)

## Power source (upstream, external to carrier)
2S LiPo 7.4V (~1500-2200 mAh) -> main switch (SW1) -> XL4005 buck @ 5.0V -> +5V rail.
ESP32 onboard LDO makes 3.3V. 470-1000 uF cap on the 5V rail near the servos.
Do NOT feed USB and battery into the ESP32 5V pin at once (isolate battery when
programming, or add a Schottky diode on the battery->5V path).

## After wiring
Inspect -> Electrical Rules Checker (ERC). Fix any "unconnected pin" (a label that
didn't land exactly on the pin end). OE, VO+/VO- should be marked no-connect.

## Corrections vs the current V2 sheet (align to the working firmware)
The existing sheet's I2C/I2S pins and the amp enable don't match the firmware
verified on the bench. Apply these:

U1 ESP32-C3:
  GPIO0  -> label TRIG          (was nc)
  GPIO1  -> label ECHO          (was nc; from divider node)
  GPIO3  -> I2S_BCLK            (keep)
  GPIO4  -> I2S_LRCLK           (keep)
  GPIO5  -> label I2S_DOUT      (was nc)
  GPIO6  -> SDA                 (was I2S_DOUT)
  GPIO7  -> SCL                 (was SDA)
  GPIO8  -> nc                  (was SCL)
  3V3 -> +3V3 ,  5V -> +5V ,  GND -> GND

U2 MAX98357A:
  SD   -> +3V3   (was nc)   *** critical: floating SD = silent amp ***
  GAIN -> GND    (was nc)   (15 dB)
  VDD +5V, GND GND, DIN I2S_DOUT, BCLK I2S_BCLK, LRCLK I2S_LRCLK   (keep)
  VO+/VO- -> nc  (speaker attaches at the module)

U3 PCA9685:
  VCC -> +3V3   (was "3.5V ESP"; make sure it is 3.3 V, not 5 V)
  V+  -> +5V    (servo power)
  ~OE -> nc     (remove OE label; onboard pulldown enables outputs)
  SDA / SCL / GND keep ;  PWM_0..15 -> nc

J3 HC-SR04:  pin4 VCC -> +5V ,  pin3 TRIG -> TRIG ,  pin2 ECHO -> divider ,  pin1 GND -> GND

Checks:
  - I2C pull-ups (4.7k) top -> +3V3, NOT +5V  (protect ESP32 3.3 V pins)
  - No-connect flags on: U1 GPIO2/8/9/10/20/21, U3 PWM_0..15, U3 ~OE, U2 VO+/VO-
