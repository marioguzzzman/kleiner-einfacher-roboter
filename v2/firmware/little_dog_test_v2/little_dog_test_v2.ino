/*
 * "Little Dog" Quadruped — Staged Functionality Test (v2)
 * =======================================================
 * Isolates each subsystem so you never debug "all problems at once."
 *
 * >>> FIRST, IN THE ARDUINO IDE: Tools -> "USB CDC On Boot" -> ENABLED <<<
 *     Without it the ESP32-C3 prints nothing over USB and looks frozen.
 *
 * Turn subsystems on/off with the flags below. Start with ONLY legs:
 *   TEST_LEGS 1, TEST_DISTANCE 0, TEST_AUDIO 0
 * Once legs work, turn on distance, then audio.
 *
 * LEG MAP (confirm by watching Stage 1):
 *   ch0 = back-left   ch1 = back-right   ch2 = front-left   ch3 = front-right
 *
 * PINS (ESP32-C3 SuperMini):
 *   GPIO0 TRIG  GPIO1 ECHO(div)  GPIO3 BCLK  GPIO4 LRC  GPIO5 DIN
 *   GPIO6 SDA   GPIO7 SCL        GPIO10 OE
 */

// ===================== FEATURE FLAGS =====================
#define TEST_LEGS      1     // set 0 to skip
#define TEST_DISTANCE  1     // set 0 to skip
#define TEST_AUDIO     1     // set 0 to skip (and skips I2S init entirely)
// =========================================================

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#if TEST_AUDIO
  #include "driver/i2s.h"
#endif

// ---------- pins ----------
#define PIN_TRIG 0
#define PIN_ECHO 1
#define PIN_SDA  6
#define PIN_SCL  7
#define PIN_OE   10
#define I2S_BCLK 3
#define I2S_LRC  4
#define I2S_DIN  5

// ---------- servo driver ----------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50
#define CENTER 307          // ~1.5 ms
#define SWING   60          // gentle; raise once safe range known

const char* LEG_NAME[4] = {"ch0 BACK-LEFT", "ch1 BACK-RIGHT",
                           "ch2 FRONT-LEFT", "ch3 FRONT-RIGHT"};

// ---------- audio ----------
#if TEST_AUDIO
  #define SAMPLE_RATE 16000
  #define I2S_PORT I2S_NUM_0
#endif

unsigned long loopCount = 0;

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n\n=== Little Dog Test v2 — staged ===");
  Serial.printf("Flags: LEGS=%d DISTANCE=%d AUDIO=%d\n",
                TEST_LEGS, TEST_DISTANCE, TEST_AUDIO);

  // OE: hold servos disabled during init (harmless if OE not wired)
  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, HIGH);

  Serial.println("[init] I2C...");
  Wire.begin(PIN_SDA, PIN_SCL);
  scanI2C();

  Serial.println("[init] PCA9685...");
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

#if TEST_DISTANCE
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
#endif

#if TEST_AUDIO
  Serial.println("[init] I2S...");
  setupI2S();
#endif

  digitalWrite(PIN_OE, LOW);   // enable servo outputs
  centerAll();
  Serial.println("[init] done. Legs centered.\n");
  delay(1500);
}

// =====================================================
void loop() {
  Serial.printf("---- heartbeat %lu ----\n", loopCount++);

#if TEST_LEGS
  Serial.println("[STAGE 1] legs — one at a time");
  centerAll();
  delay(600);
  for (int ch = 0; ch < 4; ch++) {
    Serial.printf("   moving %s\n", LEG_NAME[ch]);
    pwm.setPWM(ch, 0, CENTER + SWING); delay(500);
    pwm.setPWM(ch, 0, CENTER - SWING); delay(500);
    pwm.setPWM(ch, 0, CENTER);         delay(400);
  }
  Serial.println("[STAGE 1] done\n");
  delay(600);
#endif

#if TEST_DISTANCE
  Serial.println("[STAGE 2] distance");
  for (int i = 0; i < 3; i++) {
    long d = readDistanceCM();
    if (d < 0) Serial.println("   (no echo — check divider/wiring)");
    else       Serial.printf("   %ld cm\n", d);
    delay(300);
  }
  Serial.println("[STAGE 2] done\n");
#endif

#if TEST_AUDIO
  Serial.println("[STAGE 3] chirp");
  playTone(700, 120);
  delay(80);
  playTone(1000, 120);
  Serial.println("[STAGE 3] done\n");
#endif

  delay(1500);
}

// =====================================================
void centerAll() {
  for (int ch = 0; ch < 4; ch++) pwm.setPWM(ch, 0, CENTER);
}

void scanI2C() {
  bool found = false;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("   I2C found 0x%02X\n", a);
      found = true;
    }
  }
  if (!found)
    Serial.println("   I2C: NONE found — servos won't move. Check SDA/SCL/power.");
}

#if TEST_DISTANCE
long readDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000UL);  // 30 ms timeout
  if (dur == 0) return -1;
  return dur / 58;
}
#endif

#if TEST_AUDIO
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
  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("   I2S install FAILED");
    return;
  }
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

// Non-blocking-ish: each write has a timeout so audio can NEVER
// freeze the whole sketch (the old portMAX_DELAY could hang forever).
void playTone(float freq, int ms) {
  const int amp = 8000;
  int n = (SAMPLE_RATE * ms) / 1000;
  size_t bw;
  static float ph = 0.0f;
  float inc = 2.0f * 3.14159265f * freq / SAMPLE_RATE;
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(amp * sinf(ph));
    ph += inc; if (ph > 6.2831853f) ph -= 6.2831853f;
    esp_err_t r = i2s_write(I2S_PORT, &s, sizeof(s), &bw, pdMS_TO_TICKS(20));
    if (r != ESP_OK || bw == 0) break;  // don't hang if I2S stalls
  }
  i2s_zero_dma_buffer(I2S_PORT);
}
#endif
