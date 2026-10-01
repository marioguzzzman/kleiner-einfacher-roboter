const int MIC_ANALOG = 2;   // KY-037 AO
const int MIC_DIGITAL = 10; // KY-037 DO

void setup() {
  Serial.begin(115200);

  pinMode(MIC_DIGITAL, INPUT);
  analogReadResolution(12); // ESP32-C3: 0–4095
}

void loop() {
  int analogValue = analogRead(MIC_ANALOG);
  int digitalValue = digitalRead(MIC_DIGITAL);

  Serial.print("AO = ");
  Serial.print(analogValue);
  Serial.print("   DO = ");
  Serial.println(digitalValue);

  delay(20);
}