/*
 * "Little Dog" Quadruped — Functionality Test
 * ============================================
 * 4 legs, one MS18 servo each, fore/aft swing only (no lift).
 * This sketch does NOT walk yet — it verifies every part works and
 * helps you label which PCA9685 channel drives which physical leg.
 *
 * LEG MAP (as told — confirm by watching Part 2):
 *   ch0 = back-left   (BL)
 *   ch1 = back-right  (BR)
 *   ch2 = front-left  (FL)
 *   ch3 = front-right (FR)
 *
 * PINS (ESP32-C3 SuperMini):
 *   GPIO0  HC-SR04 TRIG      GPIO6  PCA9685 SDA
 *   GPIO1  HC-SR04 ECHO(div) GPIO7  PCA9685 SCL
 *   GPIO3  amp BCLK          GPIO10 PCA9685 OE
 *   GPIO4  amp LRC
 *   GPIO5  amp DIN
 *
 * Needs: Adafruit PWM Servo Driver Library
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "driver/i2s.h"

// ---------- pins ----------
#define PIN_TRIG 0
#define PIN_ECHO 1
#define PIN_SDA  6
#define PIN_SCL  7
#define I2S_BCLK 3
#define I2S_LRC  4
#define I2S_DIN  5

// ---------- servo driver ----------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50

// PWM tick values at 50 Hz (4096 ticks = 20 ms).
// ~1.5 ms = center. Keep the swing GENTLE while testing so
// cheap gears don't slam into end-stops.
#define CENTER 307          // ~1.5 ms  -> leg straight
#define SWING   60          // how far fore/aft (start small, raise later)

// leg channels + names (index = channel number)
const char* LEG_NAME[4] = {"BACK-LEFT (ch0)", "BACK-RIGHT (ch1)",
                           "FRONT-LEFT (ch2)", "FRONT-RIGHT (ch3)"};

// ---------- audio ----------
#define SAMPLE_RATE 16000
#define I2S_PORT I2S_NUM_0

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Little Dog — Test ===");

  // OE is left UNCONNECTED — the PCA9685's onboard pulldown holds it low,
  // so outputs are enabled by default. No OE pin to drive.

  Wire.begin(PIN_SDA, PIN_SCL);
  scanI2C();

  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  setupI2S();

  // Center every leg
  centerAll();
  Serial.println("All legs centered. Starting tests in 2 s...\n");
  delay(2000);
}

// =====================================================
void loop() {
  // ---- Part 1: center, so you see the neutral pose ----
  Serial.println(">> Part 1: center all legs");
  centerAll();
  delay(1000);

  // ---- Part 2: move ONE leg at a time (label them!) ----
  Serial.println(">> Part 2: one leg at a time — watch which leg moves");
  for (int ch = 0; ch < 4; ch++) {
    Serial.print("   Moving "); Serial.println(LEG_NAME[ch]);
    pwm.setPWM(ch, 0, CENTER + SWING);  // swing one way
    delay(500);
    pwm.setPWM(ch, 0, CENTER - SWING);  // swing the other
    delay(500);
    pwm.setPWM(ch, 0, CENTER);          // back to center
    delay(400);
  }
  delay(800);

  // ---- Part 3: trot preview (diagonal pairs) ----
  // BL+FR move together, BR+FL move together — the basis of walking.
  Serial.println(">> Part 3: diagonal-pair swing (trot preview)");
  for (int rep = 0; rep < 4; rep++) {
    // pair A forward, pair B back
    pwm.setPWM(0, 0, CENTER + SWING); // BL
    pwm.setPWM(3, 0, CENTER + SWING); // FR
    pwm.setPWM(1, 0, CENTER - SWING); // BR
    pwm.setPWM(2, 0, CENTER - SWING); // FL
    delay(350);
    // swap
    pwm.setPWM(0, 0, CENTER - SWING);
    pwm.setPWM(3, 0, CENTER - SWING);
    pwm.setPWM(1, 0, CENTER + SWING);
    pwm.setPWM(2, 0, CENTER + SWING);
    delay(350);
  }
  centerAll();
  delay(800);

  // ---- Part 4: eyes (distance) ----
  Serial.print(">> Part 4: distance = ");     // label first, so it always shows
  long d = readDistanceCM();
  if (d < 0) Serial.println("(no echo — check divider/wiring)");
  else { Serial.print(d); Serial.println(" cm"); }

  // ---- Part 5: voice (chirp) ----
  Serial.print(">> Part 5: chirp ... ");
  playTone(700, 120);
  delay(80);
  playTone(1000, 120);
  Serial.println("done\n");                    // proves Part 5 finished (no hang)

  delay(1500);
}

// =====================================================
void centerAll() {
  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);
}

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
  if (!found) Serial.println("  NONE found — check SDA/SCL/power before continuing!");
}

long readDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (dur == 0) return -1;
  return dur / 58;
}

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void playTone(float freq, int ms) {
  const int amp = 27000;
  int n = (SAMPLE_RATE * ms) / 1000;
  size_t bw;
  static float ph = 0.0f;
  float inc = 2.0f * 3.14159265f * freq / SAMPLE_RATE;
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(amp * sinf(ph));
    ph += inc; if (ph > 6.2831853f) ph -= 6.2831853f;
    // Finite timeout instead of portMAX_DELAY: if the I2S/amp isn't
    // draining the buffer, bail out rather than hang the whole sketch.
    esp_err_t r = i2s_write(I2S_PORT, &s, sizeof(s), &bw, pdMS_TO_TICKS(50));
    if (r != ESP_OK || bw == 0) break;
  }
  i2s_zero_dma_buffer(I2S_PORT);
}
