/*
 * MOTORS-ONLY isolation test
 * ==========================
 * No HC-SR04, no I2S. Just PCA9685 + 4 servos.
 * Big, slow, obvious sweep so any motion is visible.
 *
 * Wiring assumptions:
 *   PCA9685 VCC  <- ESP32 3.3V   (logic)
 *   PCA9685 V+   <- bench 5V      (servo power)
 *   PCA9685 GND  <- COMMON ground (ESP32 GND *and* bench 5V minus, same net!)
 *   PCA9685 SDA  -> GPIO6
 *   PCA9685 SCL  -> GPIO7
 *   PCA9685 OE   -> GPIO10 (active-LOW enable)
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define PIN_SDA  6
#define PIN_SCL  7
#define PIN_OE   10

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50

// Bigger, obvious travel so we can't miss motion.
#define CENTER 307     // ~1.5 ms
#define WIDE   120     // large swing for visibility (187 <-> 427)

const char* LEG_NAME[4] = {"ch0", "ch1", "ch2", "ch3"};

void scanI2C() {
  Serial.println("[I2C] scanning...");
  bool found = false;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("  found 0x"); Serial.println(a, HEX);
      found = true;
    }
  }
  if (!found) Serial.println("  NONE found!");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== MOTORS-ONLY test ===");

  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, HIGH);   // outputs OFF during init

  Wire.begin(PIN_SDA, PIN_SCL);
  scanI2C();

  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

  digitalWrite(PIN_OE, LOW);    // outputs ON (enable)
  Serial.println("OE driven LOW -> outputs ENABLED");

  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);
  Serial.println("All centered. Sweeping in 2 s...\n");
  delay(2000);
}

void loop() {
  // ---- One leg at a time, big slow travel ----
  for (int ch = 0; ch < 4; ch++) {
    Serial.print(">> Sweeping "); Serial.println(LEG_NAME[ch]);
    pwm.setPWM(ch, 0, CENTER - WIDE);
    delay(700);
    pwm.setPWM(ch, 0, CENTER + WIDE);
    delay(700);
    pwm.setPWM(ch, 0, CENTER);
    delay(500);
  }

  // ---- All four together (max visible motion) ----
  Serial.println(">> ALL FOUR together");
  for (int r = 0; r < 3; r++) {
    for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER - WIDE);
    delay(600);
    for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER + WIDE);
    delay(600);
  }
  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);
  Serial.println("(loop restart in 1 s)\n");
  delay(1000);
}
