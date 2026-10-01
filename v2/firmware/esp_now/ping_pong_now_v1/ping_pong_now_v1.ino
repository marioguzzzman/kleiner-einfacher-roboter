#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ---- Servo config (tune to your MS18) ----
#define SDA_PIN     6
#define SCL_PIN     7
#define TEST_CH     0       // PCA9685 channel to move (check your leg map)
#define SERVO_FREQ  50
#define PULSE_MIN   150     // ~ticks at 50 Hz (0.7 ms)
#define PULSE_MAX   600     // ~ticks (2.4 ms)
#define ANGLE_CENTER 90
#define ANGLE_PING   60     // move on PING
#define ANGLE_PONG   120    // move on PONG

Adafruit_PWMServoDriver pca(0x40);

void setAngle(uint8_t ch, int deg) {
  deg = constrain(deg, 0, 180);
  pca.setPWM(ch, 0, map(deg, 0, 180, PULSE_MIN, PULSE_MAX));
}

// ---- ESP-NOW ----
enum MsgType : uint8_t { MSG_PING = 1, MSG_PONG = 2 };

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint32_t seq;
  uint32_t sentMs;
} Msg;

uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

volatile bool rxFlag = false;
Msg rxMsg;
uint32_t seqNo = 0, lastPing = 0;

// Core 3.x signatures. For core 2.x use (const uint8_t*mac, ...)
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t s) {}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Msg) || rxFlag) return;
  memcpy(&rxMsg, data, sizeof(Msg));
  rxFlag = true;
}

void sendMsg(uint8_t type, uint32_t seq, uint32_t t) {
  Msg m = { type, seq, t };
  esp_now_send(BCAST, (uint8_t*)&m, sizeof(m));
}

void setup() {
  Serial.begin(115200);
  delay(1000);                       // needs USB CDC On Boot: Enabled

  Wire.begin(SDA_PIN, SCL_PIN);
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);
  delay(10);
  setAngle(TEST_CH, ANGLE_CENTER);

  WiFi.mode(WIFI_STA);               // no WiFi.begin(): no router needed
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) delay(1000); }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void loop() {
  if (millis() - lastPing > 3000) {
    lastPing = millis();
    sendMsg(MSG_PING, ++seqNo, millis());
    Serial.printf("-> PING #%lu\n", seqNo);
  }

  if (rxFlag) {
    Msg m = rxMsg;
    rxFlag = false;
    if (m.type == MSG_PING) {
      Serial.printf("<- PING #%lu: servo to %d, sending PONG\n", m.seq, ANGLE_PING);
      setAngle(TEST_CH, ANGLE_PING);
      sendMsg(MSG_PONG, m.seq, m.sentMs);
    } else if (m.type == MSG_PONG) {
      Serial.printf("<- PONG #%lu: RTT %lu ms, servo to %d\n",
                    m.seq, millis() - m.sentMs, ANGLE_PONG);
      setAngle(TEST_CH, ANGLE_PONG);
    }
  }
}