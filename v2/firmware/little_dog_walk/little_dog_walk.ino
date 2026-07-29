/*
 * LITTLE DOG — walk + obstacle-stop + modem voice
 * ===============================================
 * - Trots continuously (diagonal-pair swing).
 * - If something is within 10 cm of the HC-SR04, ALL legs freeze
 *   and it plays a dial-up modem "handshake" as its voice.
 * - Remove the obstacle and it resumes walking.
 *
 * Pins (ESP32-C3 SuperMini):
 *   GPIO0 TRIG   GPIO6 SDA   GPIO3 BCLK
 *   GPIO1 ECHO   GPIO7 SCL   GPIO4 LRC
 *                            GPIO5 DIN
 *   PCA9685 OE : unconnected (onboard pulldown enables outputs)
 *   MAX98357A  : SD->3.3V, GAIN->GND(15dB), Vin->5V, speaker on OUT+/-
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

// ---------- servos ----------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50
#define CENTER 307
#define SWING   60
#define STOP_CM 10          // obstacle threshold (cm)

// ---------- audio ----------
#define SAMPLE_RATE 16000
#define I2S_PORT I2S_NUM_0
float g_volume = 1.0f;      // master software volume (0..1)

// =====================================================
//  AUDIO ENGINE
// =====================================================
static inline void writeSample(int16_t s) {
  int32_t v = (int32_t)(s * g_volume);
  if (v > 32767) v = 32767; else if (v < -32767) v = -32767;
  int16_t o = (int16_t)v;
  size_t bw;
  i2s_write(I2S_PORT, &o, sizeof(o), &bw, pdMS_TO_TICKS(50));  // finite: never hang
}

void silence(int ms) {
  int n = SAMPLE_RATE * ms / 1000;
  for (int i = 0; i < n; i++) writeSample(0);
}

void toneMix(float f1, float f2, int ms, int a1, int a2) {
  int n = SAMPLE_RATE * ms / 1000;
  float p1 = 0, p2 = 0;
  float i1 = 2.0f * PI * f1 / SAMPLE_RATE, i2 = 2.0f * PI * f2 / SAMPLE_RATE;
  for (int i = 0; i < n; i++) {
    int32_t s = (int32_t)(a1 * sinf(p1)) + (int32_t)(a2 * sinf(p2));
    if (s > 32767) s = 32767; else if (s < -32767) s = -32767;
    p1 += i1; if (p1 > TWO_PI) p1 -= TWO_PI;
    p2 += i2; if (p2 > TWO_PI) p2 -= TWO_PI;
    writeSample((int16_t)s);
  }
}
void tone1(float f, int ms, int a) { toneMix(f, 0, ms, a, 0); }

void sweepTone(float f0, float f1, int ms, int a) {
  int n = SAMPLE_RATE * ms / 1000;
  float ph = 0;
  for (int i = 0; i < n; i++) {
    float f = f0 + (f1 - f0) * ((float)i / n);
    ph += 2.0f * PI * f / SAMPLE_RATE; if (ph > TWO_PI) ph -= TWO_PI;
    writeSample((int16_t)(a * sinf(ph)));
  }
}
void noiseBurst(int ms, int a) {
  int n = SAMPLE_RATE * ms / 1000;
  for (int i = 0; i < n; i++) writeSample((int16_t)random(-a, a));
}

// The authentic dial-up handshake (~5 s): dial tone -> dialing ->
// answer tone -> probing tones -> long warble -> scrambled data -> connect.
void modemHandshake() {
  // 1) dial tone
  toneMix(350, 440, 400, 9000, 9000);
  silence(150);

  // 2) DTMF "dialing" — 7 digits
  const float dtmf[7][2] = {
    {697,1336},{770,1477},{852,1209},{941,1336},
    {697,1477},{770,1209},{852,1477}
  };
  for (int d = 0; d < 7; d++) { toneMix(dtmf[d][0], dtmf[d][1], 110, 9000, 9000); silence(55); }
  silence(220);

  // 3) answer tone (~2100 Hz "bong")
  tone1(2100, 480, 15000);
  silence(60);

  // 4) probing tones — the scanning "doo-doo-doo"
  const float probe[7] = {1200, 1800, 1500, 2100, 1650, 2400, 1350};
  for (int i = 0; i < 7; i++) tone1(probe[i], 70, 12000);
  silence(90);

  // 5) the iconic warble — long "reeee-ooo" glides + trilling
  for (int k = 0; k < 3; k++) {
    sweepTone(1500, 2450, 210, 12000);
    sweepTone(2450, 1500, 210, 12000);
  }
  for (int k = 0; k < 6; k++) {
    toneMix(1650, 2250, 55, 8000, 8000);
    toneMix(1850, 2100, 55, 8000, 8000);
  }

  // 6) scrambled data — the loud hiss, building
  noiseBurst(170, 7000);
  for (int k = 0; k < 4; k++) {
    noiseBurst(120, 11000);
    toneMix(1900, 2400, 65, 6000, 6000);
  }
  noiseBurst(210, 12000);

  // 7) carrier connect + settle
  toneMix(1070, 2400, 300, 8000, 8000);
  sweepTone(2400, 1000, 240, 10000);
  silence(40);
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
    .bck_io_num = I2S_BCLK, .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN, .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

// =====================================================
//  DISTANCE
// =====================================================
long pingCM() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  if (dur == 0) return -1;
  return dur / 58;
}
long distanceCM() {                 // median of 3 pings
  long v[3]; int m = 0;
  for (int i = 0; i < 3; i++) { long d = pingCM(); if (d > 0) v[m++] = d; delay(6); }
  if (m == 0) return -1;
  for (int i = 1; i < m; i++) { long k=v[i]; int j=i-1; while(j>=0&&v[j]>k){v[j+1]=v[j];j--;} v[j+1]=k; }
  return v[m/2];
}

// =====================================================
//  SERVOS
// =====================================================
void centerAll() { for (int c = 0; c < 4; c++) pwm.setPWM(c, 0, CENTER); }

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== LITTLE DOG — walk + stop + voice ===");

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  centerAll();

  setupI2S();

  Serial.println("Walking. Put a hand <10 cm in front -> freeze + modem voice.\n");
  delay(1000);
}

int  phase = 0;
bool wasBlocked = false;

void loop() {
  long d = distanceCM();
  bool blocked = (d > 0 && d <= STOP_CM);

  Serial.print("distance = ");
  if (d < 0) Serial.print("(clear) "); else { Serial.print(d); Serial.print(" cm  "); }

  if (blocked) {
    Serial.println("-> STOP + voice");
    centerAll();                       // freeze all legs
    if (!wasBlocked) modemHandshake(); // play the voice once, on the stop edge
    wasBlocked = true;
    delay(100);
    return;
  }

  // ---- clear: trot (diagonal pairs) ----
  Serial.println("-> WALKING");
  wasBlocked = false;
  if (phase == 0) {
    pwm.setPWM(0, 0, CENTER + SWING); pwm.setPWM(3, 0, CENTER + SWING);
    pwm.setPWM(1, 0, CENTER - SWING); pwm.setPWM(2, 0, CENTER - SWING);
  } else {
    pwm.setPWM(0, 0, CENTER - SWING); pwm.setPWM(3, 0, CENTER - SWING);
    pwm.setPWM(1, 0, CENTER + SWING); pwm.setPWM(2, 0, CENTER + SWING);
  }
  phase ^= 1;
  delay(350);
}
