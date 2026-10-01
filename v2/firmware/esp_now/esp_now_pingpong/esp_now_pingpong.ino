// ESP-NOW ping/pong between two ESP32s, with a servo that moves on each message.
// Flash the SAME sketch to both boards (COM09 and COM10). No MAC addresses needed (broadcast).
// Arduino-ESP32 core 3.x. Serial Monitor: 115200 baud, "USB CDC On Boot: Enabled" if your board uses native USB.

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
  #define SERVO_PIN 5        // change to the GPIO your servo signal wire is on
#endif

#define SERVO_FREQ    50
#define PULSE_MIN_US  600    // pulse width at 0 deg
#define PULSE_MAX_US  2400   // pulse width at 180 deg
#define ANGLE_CENTER  90
#define ANGLE_PING    60     // servo goes here when a PING arrives
#define ANGLE_PONG    120    // servo goes here when a PONG arrives
#define RETURN_MS     600    // go back to center after this long

void setAngle(int deg) {
  deg = constrain(deg, 0, 180);
  uint32_t us = map(deg, 0, 180, PULSE_MIN_US, PULSE_MAX_US);
#if USE_PCA9685
  pca.writeMicroseconds(SERVO_CH, us);
#else
  ledcWrite(SERVO_PIN, (uint32_t)((uint64_t)us * 16383 / 20000));  // 14-bit duty at 50 Hz (20 ms period)
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

// Small ring buffer so nothing is lost if two messages arrive close together.
// The receive callback only stores data; all printing and servo work happens in loop().
#define QSIZE 8
struct Rx { Msg m; uint8_t mac[6]; int rssi; };
volatile Rx rxQ[QSIZE];
volatile uint8_t qHead = 0, qTail = 0;

uint32_t seqNo = 0;
uint32_t nextPing = 0;
uint32_t servoReturnAt = 0;

volatile uint32_t rxRaw = 0;                 // every frame the callback sees (debug)
volatile int8_t   lastTxStatus = -1;         // -1 = nothing new, 0 = OK, 1 = FAIL

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t s) {
  lastTxStatus = (s == ESP_NOW_SEND_SUCCESS) ? 0 : 1;
}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  rxRaw++;
  if (len != sizeof(Msg)) return;
  uint8_t next = (qHead + 1) % QSIZE;
  if (next == qTail) return;  // queue full, drop
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

void moveServo(int deg) {
  setAngle(deg);
  servoReturnAt = millis() + RETURN_MS;
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

  WiFi.mode(WIFI_STA);          // no WiFi.begin(): no router needed
  WiFi.disconnect();
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // many ESP32-C3 boards (SuperMini etc.) are flaky at full power
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // both boards must share a channel
  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.printf("Channel: %d\n", ch);
  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;             // 0 = use current channel
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  randomSeed(esp_random());
  nextPing = millis() + 2000 + random(0, 1000);
  Serial.println("Ready. Waiting for the other board...");
}

void loop() {
  // Periodic PING (random jitter so both boards don't always transmit at the same instant)
  if ((int32_t)(millis() - nextPing) >= 0) {
    nextPing = millis() + 3000 + random(0, 1000);
    sendMsg(MSG_PING, ++seqNo, millis());
    Serial.printf("-> PING #%lu\n", (unsigned long)seqNo);
    delay(50);  // give the send callback time to fire
    Serial.printf("   tx status: %s | raw frames received so far: %lu\n",
                  lastTxStatus == 0 ? "OK" : (lastTxStatus == 1 ? "FAIL" : "no callback"),
                  (unsigned long)rxRaw);
    lastTxStatus = -1;
  }

  // Handle received messages
  while (qTail != qHead) {
    Msg m;
    uint8_t mac[6];
    int rssi = rxQ[qTail].rssi;
    memcpy(&m, (const void *)&rxQ[qTail].m, sizeof(Msg));
    memcpy(mac, (const void *)rxQ[qTail].mac, 6);
    qTail = (qTail + 1) % QSIZE;

    if (m.type == MSG_PING) {
      Serial.printf("<- PING #%lu from %02X:%02X:%02X:%02X:%02X:%02X (RSSI %d) -> servo %d, sending PONG\n",
                    (unsigned long)m.seq, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], rssi, ANGLE_PING);
      moveServo(ANGLE_PING);
      sendMsg(MSG_PONG, m.seq, m.sentMs);   // echo the sender's timestamp so it can compute RTT
    } else if (m.type == MSG_PONG) {
      Serial.printf("<- PONG #%lu from %02X:%02X:%02X:%02X:%02X:%02X (RSSI %d) RTT %lu ms -> servo %d\n",
                    (unsigned long)m.seq, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], rssi,
                    (unsigned long)(millis() - m.sentMs), ANGLE_PONG);
      moveServo(ANGLE_PONG);
    }
  }

  // Non-blocking return to center
  if (servoReturnAt && (int32_t)(millis() - servoReturnAt) >= 0) {
    setAngle(ANGLE_CENTER);
    servoReturnAt = 0;
  }
}
