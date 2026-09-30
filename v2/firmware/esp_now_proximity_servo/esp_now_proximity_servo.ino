// ESP-NOW proximity servo: the servo sweeps back and forth, faster when the other board is
// closer and slower when it is farther. Distance is estimated from RSSI (signal strength).
// Flash the SAME sketch to both boards. Arduino-ESP32 core 3.x. Serial Monitor: 115200 baud.
//
// NOTE: RSSI is only a rough proxy for distance. It is noisy and changes with walls, bodies,
// antenna orientation and USB cables. Calibrate RSSI_NEAR / RSSI_FAR below using the values
// printed in the Serial Monitor (place the boards side by side, then at the farthest distance you want).

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ---------- Servo: pick ONE ----------
#define USE_PCA9685 1        // 1 = servo on PCA9685 (I2C), 0 = servo signal wire directly on a GPIO

#if USE_PCA9685
  #include <Wire.h>
  #include <Adafruit_PWMServoDriver.h>
  #define SDA_PIN   6
  #define SCL_PIN   7
  #define SERVO_CH  0
  Adafruit_PWMServoDriver pca(0x40);
#else
  #define SERVO_PIN 5
#endif

#define SERVO_FREQ    50
#define PULSE_MIN_US  600
#define PULSE_MAX_US  2400
#define ANGLE_CENTER  90
#define ANGLE_A       45     // sweep limits
#define ANGLE_B       135

// ---------- Proximity mapping (calibrate these) ----------
#define RSSI_NEAR     -35    // dBm when boards are practically touching -> fastest
#define RSSI_FAR      -85    // dBm at the largest distance you care about -> slowest
#define SPEED_MAX     2.5f   // one-way sweeps per second when near
#define SPEED_MIN     0.15f  // one-way sweeps per second when far
#define RSSI_SMOOTH   0.25f  // 0..1, higher = reacts faster but jitters more
#define LOST_MS       4000   // no message for this long -> servo returns to center and stops

void setAngle(float deg) {
  deg = constrain(deg, 0.0f, 180.0f);
  uint32_t us = PULSE_MIN_US + (uint32_t)((PULSE_MAX_US - PULSE_MIN_US) * (deg / 180.0f));
#if USE_PCA9685
  pca.writeMicroseconds(SERVO_CH, us);
#else
  ledcWrite(SERVO_PIN, (uint32_t)((uint64_t)us * 16383 / 20000));
#endif
}

// ---------- ESP-NOW ----------
enum MsgType : uint8_t { MSG_PING = 1, MSG_PONG = 2 };

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint32_t seq;
  uint32_t sentMs;
} Msg;

uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define QSIZE 8
struct Rx { Msg m; uint8_t mac[6]; int rssi; };
volatile Rx rxQ[QSIZE];
volatile uint8_t qHead = 0, qTail = 0;

uint32_t seqNo = 0;
uint32_t nextPing = 0;

// Proximity / motion state
float    rssiSmooth = RSSI_FAR;
float    sweepSpeed = 0;        // one-way sweeps per second
uint32_t lastHeard = 0;
bool     everHeard = false;
bool     parked = true;
float    pos = 0.5f;            // 0..1 position along the sweep
int8_t   dir = 1;
uint32_t lastServoUpdate = 0;

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t s) {}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Msg)) return;
  uint8_t next = (qHead + 1) % QSIZE;
  if (next == qTail) return;
  memcpy((void *)&rxQ[qHead].m, data, sizeof(Msg));
  memcpy((void *)rxQ[qHead].mac, info->src_addr, 6);
  rxQ[qHead].rssi = info->rx_ctrl->rssi;
  qHead = next;
}

void sendMsg(uint8_t type, uint32_t seq, uint32_t t) {
  Msg m = { type, seq, t };
  esp_err_t r = esp_now_send(BCAST, (uint8_t *)&m, sizeof(m));
  if (r != ESP_OK) Serial.printf("!! esp_now_send error %d\n", r);
}

// Update smoothed RSSI and the resulting sweep speed
void updateProximity(int rssi) {
  if (!everHeard) { rssiSmooth = rssi; everHeard = true; }
  else            { rssiSmooth += RSSI_SMOOTH * (rssi - rssiSmooth); }

  float t = (rssiSmooth - RSSI_FAR) / (float)(RSSI_NEAR - RSSI_FAR);
  t = constrain(t, 0.0f, 1.0f);
  sweepSpeed = SPEED_MIN + t * (SPEED_MAX - SPEED_MIN);
  lastHeard = millis();
  parked = false;
}

// Advance the sweep without blocking (called every loop, servo written ~50 Hz)
void updateServo() {
  uint32_t now = millis();
  if (now - lastServoUpdate < 20) return;
  float dt = (now - lastServoUpdate) / 1000.0f;
  lastServoUpdate = now;

  if (!parked && now - lastHeard > LOST_MS) {
    parked = true;
    setAngle(ANGLE_CENTER);
    Serial.println("-- no signal: servo parked at center");
    return;
  }
  if (parked) return;

  pos += dir * sweepSpeed * dt;
  if (pos >= 1.0f) { pos = 1.0f; dir = -1; }
  if (pos <= 0.0f) { pos = 0.0f; dir = 1; }
  setAngle(ANGLE_A + pos * (ANGLE_B - ANGLE_A));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

#if USE_PCA9685
  Wire.begin(SDA_PIN, SCL_PIN);
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);
  delay(10);
#else
  ledcAttach(SERVO_PIN, SERVO_FREQ, 14);
#endif
  setAngle(ANGLE_CENTER);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // keep whatever value made your link work
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  randomSeed(esp_random());
  nextPing = millis() + 500 + random(0, 300);
  Serial.println("Ready. Move the boards closer/farther and watch the RSSI.");
}

void loop() {
  // Ping often (~2x/sec) so the RSSI, and therefore the speed, updates quickly
  if ((int32_t)(millis() - nextPing) >= 0) {
    nextPing = millis() + 400 + random(0, 200);
    sendMsg(MSG_PING, ++seqNo, millis());
  }

  while (qTail != qHead) {
    Msg m;
    int rssi = rxQ[qTail].rssi;
    memcpy(&m, (const void *)&rxQ[qTail].m, sizeof(Msg));
    qTail = (qTail + 1) % QSIZE;

    updateProximity(rssi);
    Serial.printf("<- %s #%lu  RSSI %d dBm  smoothed %.1f  speed %.2f sweeps/s\n",
                  m.type == MSG_PING ? "PING" : "PONG", (unsigned long)m.seq,
                  rssi, rssiSmooth, sweepSpeed);

    if (m.type == MSG_PING) sendMsg(MSG_PONG, m.seq, m.sentMs);
  }

  updateServo();
}
