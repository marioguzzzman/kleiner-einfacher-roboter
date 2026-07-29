/*
 * OBSTACLE-STOP demo
 * ==================
 * The legs trot continuously. When something is within 10 cm of the
 * HC-SR04, ALL motors freeze (and it chirps once). Remove the obstacle
 * and it resumes walking.  -> ties the distance sensor to the motors.
 *
 * Pins (ESP32-C3 SuperMini):
 *   GPIO0  TRIG    GPIO6 SDA    GPIO3 BCLK
 *   GPIO1  ECHO    GPIO7 SCL    GPIO4 LRC
 *                               GPIO5 DIN
 *   OE: left unconnected (PCA9685 onboard pulldown enables outputs)
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "driver/i2s.h"

#define PIN_TRIG 0
#define PIN_ECHO 1
#define PIN_SDA  6
#define PIN_SCL  7
#define I2S_BCLK 3
#define I2S_LRC  4
#define I2S_DIN  5

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50
#define CENTER 307
#define SWING   60

#define STOP_CM 10          // obstacle threshold (cm)

#define SAMPLE_RATE 16000
#define I2S_PORT I2S_NUM_0

// ---------- distance ----------
long pingCM() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  if (dur == 0) return -1;      // no echo -> nothing in range
  return dur / 58;
}

// median of 3 pings (kills the occasional stray reading)
long distanceCM() {
  long v[3]; int m = 0;
  for (int i = 0; i < 3; i++) {
    long d = pingCM();
    if (d > 0) v[m++] = d;
    delay(6);
  }
  if (m == 0) return -1;
  for (int i = 1; i < m; i++) { long k=v[i]; int j=i-1; while(j>=0 && v[j]>k){v[j+1]=v[j];j--;} v[j+1]=k; }
  return v[m/2];
}

// ---------- servos ----------
void centerAll() { for (int c = 0; c < 4; c++) pwm.setPWM(c, 0, CENTER); }

// ---------- audio ----------
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
  const int amp = 27000;   // near full-scale for a loud chirp
  int n = (SAMPLE_RATE * ms) / 1000;
  size_t bw;
  static float ph = 0.0f;
  float inc = 2.0f * 3.14159265f * freq / SAMPLE_RATE;
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(amp * sinf(ph));
    ph += inc; if (ph > 6.2831853f) ph -= 6.2831853f;
    esp_err_t r = i2s_write(I2S_PORT, &s, sizeof(s), &bw, pdMS_TO_TICKS(50));
    if (r != ESP_OK || bw == 0) break;   // never hang
  }
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== OBSTACLE-STOP demo ===");

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  centerAll();

  setupI2S();

  Serial.println("Legs trot until something is <10 cm in front.");
  Serial.println("Put your hand close -> legs FREEZE + chirp.\n");
  delay(1000);
}

int  phase = 0;
bool wasBlocked = false;

void loop() {
  long d = distanceCM();
  bool blocked = (d > 0 && d <= STOP_CM);

  Serial.print("distance = ");
  if (d < 0) Serial.print("(clear) ");
  else { Serial.print(d); Serial.print(" cm  "); }

  if (blocked) {
    Serial.println("-> OBSTACLE! motors STOP");
    centerAll();                       // freeze
    if (!wasBlocked) {                 // chirp once on the edge
      playTone(900, 150);
      delay(60);
      playTone(1300, 150);
    }
    wasBlocked = true;
    delay(120);
    return;
  }

  // ---- clear: keep trotting (diagonal pairs) ----
  Serial.println("-> clear, WALKING");
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
