/*
 * AUDIO-ONLY diagnostic
 * =====================
 * Plays a loud, continuous 880 Hz tone and reports how many bytes the
 * I2S actually shipped per burst. This separates the two failure modes:
 *
 *   wrote ~16000 bytes, 0 timeouts  -> I2S IS sending data to the amp.
 *        If still silent => amp hardware: SD pin (must be 3.3V),
 *        Vin (5V), speaker connected, DIN/BCLK/LRC wiring.
 *
 *   wrote ~0 bytes, many timeouts   -> I2S peripheral itself not running
 *        (driver install / pin / clock problem).
 *
 * MAX98357A pins:  BCLK=GPIO3  LRC=GPIO4  DIN=GPIO5
 *                  Vin=5V   GND=common   SD=3.3V (ENABLE!)   speaker on +/-
 */

#include "driver/i2s.h"

#define I2S_BCLK 3
#define I2S_LRC  4
#define I2S_DIN  5

#define SAMPLE_RATE 16000
#define I2S_PORT I2S_NUM_0

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
  esp_err_t e1 = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  esp_err_t e2 = i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.printf("i2s_driver_install = %d, i2s_set_pin = %d (0 = OK)\n", e1, e2);
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== AUDIO-ONLY diagnostic ===");
  setupI2S();
  Serial.println("Playing repeated 880 Hz bursts. Listen + watch byte counts.\n");
}

void loop() {
  const int amp = 27000;     // loud (of 32767) — near full-scale, small headroom
  const int ms  = 500;
  int n = (SAMPLE_RATE * ms) / 1000;
  size_t total = 0, bw;
  int timeouts = 0;
  static float ph = 0.0f;
  float inc = 2.0f * 3.14159265f * 880.0f / SAMPLE_RATE;

  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(amp * sinf(ph));
    ph += inc; if (ph > 6.2831853f) ph -= 6.2831853f;
    esp_err_t r = i2s_write(I2S_PORT, &s, sizeof(s), &bw, pdMS_TO_TICKS(50));
    if (r != ESP_OK || bw == 0) timeouts++;
    else total += bw;
  }

  Serial.printf("burst done: wrote %u bytes, %d timeouts (expected ~%d)\n",
                (unsigned)total, timeouts, n * 2);
  delay(500);
}
