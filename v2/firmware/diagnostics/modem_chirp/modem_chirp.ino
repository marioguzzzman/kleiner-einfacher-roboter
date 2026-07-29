/*
 * MODEM-CHIRP test
 * ================
 * A stylized dial-up modem "handshake" as the robot's voice.
 * Plays the whole sequence on a loop so you can tune it by ear.
 *
 * MAX98357A: BCLK=GPIO3  LRC=GPIO4  DIN=GPIO5  SD=3.3V  GAIN->GND(15dB)
 */

#include "driver/i2s.h"

#define I2S_BCLK 3
#define I2S_LRC  4
#define I2S_DIN  5
#define SAMPLE_RATE 16000
#define I2S_PORT I2S_NUM_0

// ---------- master software volume (0.0 .. 1.0) ----------
// This is your software volume knob — scales every sample before output.
// Change it anytime; independent of the hardware GAIN pin.
float g_volume = 0.85f;

// ---------- low-level ----------
static inline void writeSample(int16_t s) {
  int32_t v = (int32_t)(s * g_volume);
  if (v > 32767) v = 32767; else if (v < -32767) v = -32767;
  int16_t o = (int16_t)v;
  size_t bw;
  i2s_write(I2S_PORT, &o, sizeof(o), &bw, pdMS_TO_TICKS(50));
}

void silence(int ms) {
  int n = SAMPLE_RATE * ms / 1000;
  for (int i = 0; i < n; i++) writeSample(0);
}

// two summed sine tones (a modem/DTMF pair). amp per tone; keep sum < 32767.
void toneMix(float f1, float f2, int ms, int a1, int a2) {
  int n = SAMPLE_RATE * ms / 1000;
  float ph1 = 0, ph2 = 0;
  float inc1 = 2.0f * PI * f1 / SAMPLE_RATE;
  float inc2 = 2.0f * PI * f2 / SAMPLE_RATE;
  for (int i = 0; i < n; i++) {
    int32_t s = (int32_t)(a1 * sinf(ph1)) + (int32_t)(a2 * sinf(ph2));
    if (s > 32767) s = 32767; else if (s < -32767) s = -32767;
    ph1 += inc1; if (ph1 > TWO_PI) ph1 -= TWO_PI;
    ph2 += inc2; if (ph2 > TWO_PI) ph2 -= TWO_PI;
    writeSample((int16_t)s);
  }
}

// single tone helper
void tone1(float f, int ms, int a) { toneMix(f, 0, ms, a, 0); }

// gliding tone (the warble)
void sweepTone(float f0, float f1, int ms, int a) {
  int n = SAMPLE_RATE * ms / 1000;
  float ph = 0;
  for (int i = 0; i < n; i++) {
    float f = f0 + (f1 - f0) * ((float)i / n);
    ph += 2.0f * PI * f / SAMPLE_RATE; if (ph > TWO_PI) ph -= TWO_PI;
    writeSample((int16_t)(a * sinf(ph)));
  }
}

// scrambled-data hiss (pseudo-random noise)
void noiseBurst(int ms, int a) {
  int n = SAMPLE_RATE * ms / 1000;
  for (int i = 0; i < n; i++) writeSample((int16_t)random(-a, a));
}

// ---------- the handshake ----------
void modemHandshake() {
  // 1) dial tone (350 + 440 Hz)
  toneMix(350, 440, 450, 9000, 9000);
  silence(120);

  // 2) touch-tone "dialing" — four DTMF digits
  const float dtmf[4][2] = {
    {697, 1336},  // 2
    {770, 1477},  // 6
    {941, 1209},  // *
    {852, 1336}   // 8
  };
  for (int d = 0; d < 4; d++) {
    toneMix(dtmf[d][0], dtmf[d][1], 130, 9000, 9000);
    silence(70);
  }
  silence(160);

  // 3) answer tone (~2100 Hz)
  tone1(2100, 380, 16000);
  silence(70);

  // 4) the iconic warble — alternating dual tones, climbing
  for (int k = 0; k < 4; k++) {
    toneMix(1200 + k * 140, 2250, 85, 8000, 8000);
    toneMix(1650, 1750 + k * 90, 85, 8000, 8000);
  }
  sweepTone(1800, 2400, 180, 13000);
  sweepTone(2400, 1500, 180, 13000);

  // 5) scrambled data — noise interleaved with carrier
  for (int k = 0; k < 3; k++) {
    noiseBurst(130, 9000);
    toneMix(1800, 2400, 90, 7000, 7000);
  }

  // 6) carrier "connected", then settle
  toneMix(1070, 2400, 320, 8000, 8000);
  silence(40);
}

// ---------- I2S ----------
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MODEM-CHIRP test ===");
  setupI2S();
  Serial.println("Playing dial-up handshake on loop...\n");
}

void loop() {
  Serial.println(">> handshake");
  modemHandshake();
  delay(1500);
}
