/*
 * HOLD-CENTER diagnostic
 * ======================
 * Parks all 4 servos at center and HOLDS (no movement).
 * Prints a heartbeat every 2 s with an I2C re-check + a counter.
 *
 * Read three things from this:
 *   1) Does "found 0x40" appear reliably?  (I2C solid)
 *   2) Does the banner print ONCE, then heartbeats count up 1,2,3...?
 *      If the banner REPEATS -> board is reset-looping.
 *   3) On the bench ammeter while it holds:
 *        ~100-200 mA & servos firm  -> valid signal reaching them (GOOD)
 *        ~10-15  mA & servos slack   -> no valid signal (signal/level issue)
 *
 * OE is assumed hard-wired to GND (outputs always enabled).
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define PIN_SDA  6
#define PIN_SCL  7

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50
#define CENTER 307     // ~1.5 ms

bool i2cSees0x40() {
  Wire.beginTransmission(0x40);
  return (Wire.endTransmission() == 0);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== HOLD-CENTER diagnostic ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.print("[boot] 0x40 present? ");
  Serial.println(i2cSees0x40() ? "YES" : "NO");

  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);
  Serial.println("[boot] all 4 parked at CENTER (307). Holding.\n");
}

uint32_t beat = 0;

void loop() {
  // Re-assert center every cycle so a valid pulse train is always present.
  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);

  beat++;
  Serial.print("heartbeat #"); Serial.print(beat);
  Serial.print("   0x40="); Serial.print(i2cSees0x40() ? "YES" : "NO");
  Serial.println("   (servos should be FIRM at center)");

  delay(2000);
}
