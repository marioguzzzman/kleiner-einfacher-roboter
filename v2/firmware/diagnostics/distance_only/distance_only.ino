/*
 * DISTANCE-ONLY isolation test
 * ============================
 * No PCA9685, no I2S. Just the HC-SR04.
 * Prints raw echo pulse (us) AND cm every ~300 ms so we can tell
 * "no pulse at all" (wiring/power) from "bad reading" (divider).
 *
 * Wiring:
 *   HC-SR04 VCC  <- 5V (ESP32 5V pin / USB)
 *   HC-SR04 GND  <- COMMON ground (same net as ESP32 GND)
 *   HC-SR04 TRIG <- GPIO0            (direct)
 *   HC-SR04 ECHO -> [R1 1k] -> GPIO1 junction -> [R2 2k] -> GND
 */

#define PIN_TRIG 0
#define PIN_ECHO 1

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== DISTANCE-ONLY test ===");
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  delay(50);
}

void loop() {
  // 10 us trigger pulse
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long dur = pulseIn(PIN_ECHO, HIGH, 30000UL);  // wait up to 30 ms

  Serial.print("echo pulse = ");
  if (dur == 0) {
    Serial.println("0 us  -> NO ECHO (check: 5V on VCC? common GND? TRIG/ECHO swapped? divider?)");
  } else {
    Serial.print(dur); Serial.print(" us  ->  ");
    Serial.print(dur / 58); Serial.println(" cm");
  }
  delay(300);
}
