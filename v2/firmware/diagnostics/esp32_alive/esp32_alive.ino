/*
 * ESP32-C3 SuperMini — bare "am I alive?" test
 * ============================================
 * Nothing external attached. Proves: board boots, runs code,
 * blinks onboard LED, and prints over USB serial.
 *
 * SuperMini onboard blue LED is on GPIO8 (active LOW).
 */

#define LED_PIN 8   // onboard LED on the C3 SuperMini (active LOW)

uint32_t beat = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n============================");
  Serial.println("  ESP32-C3 SuperMini  ALIVE");
  Serial.println("============================");
  Serial.print("Chip model : "); Serial.println(ESP.getChipModel());
  Serial.print("Cores      : "); Serial.println(ESP.getChipCores());
  Serial.print("CPU MHz    : "); Serial.println(getCpuFrequencyMhz());
  Serial.print("Flash MB   : "); Serial.println(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.print("Free heap  : "); Serial.println(ESP.getFreeHeap());
  Serial.println("----------------------------");
  Serial.println("Onboard LED (GPIO8) will blink. Heartbeats below:\n");

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  beat++;

  digitalWrite(LED_PIN, LOW);   // LED ON (active low)
  Serial.print("heartbeat #"); Serial.print(beat);
  Serial.print("   millis="); Serial.print(millis());
  Serial.println("   LED=ON");
  delay(500);

  digitalWrite(LED_PIN, HIGH);  // LED OFF
  Serial.print("heartbeat #"); Serial.print(beat);
  Serial.print("   millis="); Serial.print(millis());
  Serial.println("   LED=off");
  delay(500);
}
