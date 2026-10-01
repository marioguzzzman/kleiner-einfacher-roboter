// =====================================================================================
//  esp_now_vvvv  —  ESP-NOW proximity -> servo motion -> serial stream for vvvv
// =====================================================================================
//
//  WHAT IT DOES
//  ------------
//  Two ESP32-C3 boards run this SAME sketch. Every ~20 ms each board broadcasts a tiny
//  "PING" packet over ESP-NOW. Each board measures how strong the other board's packets
//  arrive (RSSI, in dBm), turns that into an estimated distance in centimeters, and uses
//  the distance to set how fast a servo sweeps back and forth:
//
//        closer  -> stronger signal -> smaller distance -> FASTER sweep
//        farther -> weaker signal   -> larger distance  -> SLOWER sweep
//
//  Every 20 ms the board also prints ONE data line per motor on the Serial port, in a
//  fixed comma-separated format that a vvvv patch can split and parse directly.
//
//  Builds on esp_now_pingpong (same radio setup) and diagnostics/esp_now_proximity_servo
//  (same sweep idea). Arduino-ESP32 core 3.x. Board: ESP32C3 Dev Module,
//  "USB CDC On Boot: Enabled". Serial: 115200 baud.
//
// -------------------------------------------------------------------------------------
//  SERIAL OUTPUT FORMAT  (this is the part vvvv reads)
// -------------------------------------------------------------------------------------
//  There are only two kinds of lines:
//
//   1) DATA lines — always start with "D," and always have exactly 10 fields:
//
//        D,time_ms,motor,angle_deg,speed_dps,rssi_dbm,rssi_avg_dbm,dist_cm,link,packets
//
//        field  name          meaning
//        -----  ------------  -------------------------------------------------------
//          0    D             line tag: "this is a data line"
//          1    time_ms       millis() on this board when the line was printed
//          2    motor         motor index (0, 1, 2 ...) -> which motor this line is about
//          3    angle_deg     angle the motor was just commanded to (0..180)
//          4    speed_dps     how fast that motor is moving, in degrees per second
//                             (0 = standing still / parked)
//          5    rssi_dbm      raw signal strength of the LAST packet received (e.g. -52)
//          6    rssi_avg_dbm  smoothed signal strength (what the distance is computed from)
//          7    dist_cm       estimated distance to the other board, in centimeters
//          8    link          1 = hearing the other board, 0 = signal lost
//          9    packets       how many packets arrived since the previous data line
//                             (normally 0 or 1 at 20 ms; useful to see packet loss)
//
//        example:  D,48213,0,112.4,135.0,-58,-57.3,142.6,1,1
//
//   2) INFO lines — always start with "#". Human-readable messages (boot info, "signal
//      lost", calibration hints). In vvvv simply IGNORE every line starting with "#".
//
//  Parsing in vvvv (gamma or beta, same idea):
//      SerialPort (115200)  ->  split the incoming text on "\n"  ->  keep only lines that
//      start with "D,"  ->  split each line on ","  ->  convert fields 1..9 to numbers.
//      With several motors you get severala D-lines per 20 ms frame; use field 2 (motor)
//      to route each one to the right place in the patch.
//
// -------------------------------------------------------------------------------------
//  IMPORTANT: ABOUT "SIGNAL STRENGTH -> CENTIMETERS"
// -------------------------------------------------------------------------------------
//  Yes, it is possible, but it is an ESTIMATE, not a measurement. We use the standard
//  "log-distance path loss" model:
//
//        distance_m = 10 ^ ( (RSSI_AT_1M - rssi) / (10 * PATH_LOSS_N) )
//
//    RSSI_AT_1M   = signal strength you measure when the boards are exactly 1 m apart
//    PATH_LOSS_N  = how fast the signal fades with distance (2.0 = open air,
//                   2.5..3.5 = typical room, up to 4 with walls / people in between)
//
//  Realistic expectations: indoors it is roughly right on average but can be off by
//  30-50 % or more. Hands, bodies, the USB cable, antenna orientation and reflections
//  all change RSSI. It works best as "near / medium / far" and for smooth changes, not
//  for precise centimeters. Below ~20 cm the signal saturates, so very close
//  distances all look about the same.
//
//  HOW TO CALIBRATE (5 minutes, makes a big difference):
//    1. Put the boards exactly 1 m apart, same height, antennas facing the same way.
//       Watch field 6 (rssi_avg_dbm) for ~10 s and write down the typical value.
//       -> put it in RSSI_AT_1M.
//    2. Move them to 3 m apart and write down rssi_avg_dbm again (call it R3).
//       -> PATH_LOSS_N = (RSSI_AT_1M - R3) / 4.77      (4.77 = 10 * log10(3))
//    3. If you change TX_POWER later, redo step 1 (the whole curve shifts).
//
// =====================================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

// =====================================================================================
//  1. SETTINGS YOU WILL WANT TO TWEAK
// =====================================================================================

// ---------- Timing ----------
#define SEND_INTERVAL_MS   20     // send one PING every 20 ms (= 50 packets per second)
#define SEND_JITTER_MS     3      // + random 0..3 ms, so both boards don't always transmit
                                  //   at the exact same instant and collide
#define FRAME_MS           20     // update the motors AND print data lines every 20 ms
                                  //   (20 ms = 50 Hz, which is also the servo's own refresh rate)
#define LOST_MS            1000   // no packet for this long -> link = 0, motors park

// ---------- Radio ----------
#define WIFI_CHANNEL       1      // both boards MUST use the same channel
#define TX_POWER           WIFI_POWER_8_5dBm   // what worked in esp_now_pingpong.
                                               // Changing this changes RSSI_AT_1M!

// ---------- Distance estimate (calibrate, see header) ----------
#define RSSI_AT_1M        -88.0f  // dBm measured at 1 m  (CALIBRATE)
#define PATH_LOSS_N        2.5f   // environment factor   (CALIBRATE)
#define DIST_MIN_CM        5.0f   // clamp: never report less than this
#define DIST_MAX_CM        1000.0f// clamp: never report more than this
#define RSSI_SMOOTH        0.10f  // 0..1. Smoothing of RSSI before converting to cm.
                                  //   Lower = steadier but slower to react.
                                  //   At 50 packets/s, 0.10 settles in roughly 0.5 s.
#define R3                 -94.0f

// ---------- Distance -> motor speed ----------
#define NEAR_CM            20.0f  // at this distance (or closer) motors run at SPEED_MAX_DPS
#define FAR_CM             300.0f // at this distance (or farther) motors run at SPEED_MIN_DPS
#define SPEED_MAX_DPS      250.0f // degrees per second when near
#define SPEED_MIN_DPS      15.0f  // degrees per second when far

// ---------- Servo hardware ----------
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#define SDA_PIN            6      // PCA9685 SDA (see v2/README pin map)
#define SCL_PIN            7      // PCA9685 SCL
#define SERVO_FREQ         50     // standard analog servo frequency
#define PULSE_MIN_US       600    // pulse width at 0 deg
#define PULSE_MAX_US       2400   // pulse width at 180 deg
Adafruit_PWMServoDriver pca(0x40);

// =====================================================================================
//  2. MOTORS
// =====================================================================================
//  Each motor is described by one entry in the MOTORS[] table. Right now there is only
//  ONE motor. To add more later, just add lines to the table — the rest of the code
//  (sweeping, printing one D-line per motor) already loops over the whole table.
//
//  Later, for sequences/gaits, you would replace the simple back-and-forth sweep in
//  updateMotors() with your own pattern, but keep the same printing.

struct Motor {
  uint8_t channel;     // PCA9685 output channel (0..15) the servo is plugged into
  float   angleMin;    // sweep limit A (degrees)
  float   angleMax;    // sweep limit B (degrees)
  float   angleRest;   // where the motor parks when the link is lost
  // --- runtime state (filled in by the code, leave at 0 in the table) ---
  float   angle;       // current commanded angle
  int8_t  dir;         // +1 = moving toward angleMax, -1 = toward angleMin
  float   speedDps;    // current speed in degrees/second
};

Motor MOTORS[] = {
  //  ch  min   max   rest
  {   0,  45,  135,   90 },
  // {   1,  45,  135,   90 },   // <- example: uncomment to add a second motor on channel 1
};
const int NUM_MOTORS = sizeof(MOTORS) / sizeof(MOTORS[0]);

// Convert an angle in degrees to a pulse and send it to the PCA9685.
void writeServo(uint8_t channel, float deg) {
  deg = constrain(deg, 0.0f, 180.0f);
  uint32_t us = PULSE_MIN_US + (uint32_t)((PULSE_MAX_US - PULSE_MIN_US) * (deg / 180.0f));
  pca.writeMicroseconds(channel, us);
}

// =====================================================================================
//  3. ESP-NOW MESSAGES
// =====================================================================================
//  We only need one message type now: PING. (No PONG reply like in esp_now_pingpong:
//  each board measures the RSSI of the OTHER board's pings, which is all we need, and
//  skipping replies halves the radio traffic at this fast rate.)

enum MsgType : uint8_t { MSG_PING = 1 };

typedef struct __attribute__((packed)) {
  uint8_t  type;     // MSG_PING
  uint32_t seq;      // counts up by 1 each send
} Msg;

uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   // broadcast: no MACs needed

// ---- Receive queue ----
// The receive callback runs in the WiFi task, NOT in loop(). It must be quick, so it only
// copies the RSSI into this small ring buffer. loop() reads the buffer and does the real work.
#define QSIZE 16
volatile int8_t  rxRssi[QSIZE];
volatile uint8_t qHead = 0, qTail = 0;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Msg) || data[0] != MSG_PING) return;   // ignore anything unexpected
  uint8_t next = (qHead + 1) % QSIZE;
  if (next == qTail) return;                               // queue full -> drop this one
  rxRssi[qHead] = info->rx_ctrl->rssi;
  qHead = next;
}

uint32_t seqNo = 0;

void sendPing() {
  Msg m = { MSG_PING, ++seqNo };
  esp_now_send(BCAST, (uint8_t *)&m, sizeof(m));
  // We don't print send errors here: at 50 sends/s that would flood the serial port
  // and break the clean data stream for vvvv.
}

// =====================================================================================
//  4. PROXIMITY STATE  (signal strength -> distance -> speed)
// =====================================================================================

int      rssiLast   = 0;          // raw RSSI of the newest packet
float    rssiAvg    = 0;          // smoothed RSSI
float    distCm     = DIST_MAX_CM;
bool     everHeard  = false;
bool     linkUp     = false;
uint32_t lastHeard  = 0;          // millis() of the newest packet
uint16_t packetsThisFrame = 0;    // packets received since the last D-line

// Signal strength (dBm) -> estimated distance (cm), using the log-distance model.
float rssiToCm(float rssi) {
  float meters = powf(10.0f, (RSSI_AT_1M - R3) / (10.0f * PATH_LOSS_N));
  return constrain(meters * 100.0f, DIST_MIN_CM, DIST_MAX_CM);
}

// Distance (cm) -> motor speed (deg/s). Linear between NEAR_CM and FAR_CM.
float cmToSpeed(float cm) {
  float t = (cm - NEAR_CM) / (FAR_CM - NEAR_CM);   // 0 at NEAR_CM, 1 at FAR_CM
  t = constrain(t, 0.0f, 1.0f);
  return SPEED_MAX_DPS + t * (SPEED_MIN_DPS - SPEED_MAX_DPS);
}

// Called once per received packet (from loop(), not from the callback).
void handlePacket(int rssi) {
  rssiLast = rssi;
  // Exponential moving average: move rssiAvg a fraction of the way toward the new value.
  // We smooth in dBm (before converting to cm) because RSSI noise is roughly even in dBm.
  if (!everHeard) { rssiAvg = rssi; everHeard = true; }
  else            { rssiAvg += RSSI_SMOOTH * (rssi - rssiAvg); }

  distCm    = rssiToCm(rssiAvg);
  lastHeard = millis();
  packetsThisFrame++;

  if (!linkUp) {
    linkUp = true;
    Serial.println("# link up");
  }
}

// =====================================================================================
//  5. MOTOR UPDATE + DATA OUTPUT  (runs every FRAME_MS)
// =====================================================================================

uint32_t lastFrame = 0;

// Print one D-line for motor i. See the format table at the top of the file.
void printDataLine(int i) {
  Serial.printf("D,%lu,%d,%.1f,%.1f,%d,%.1f,%.1f,%d,%u\n",
                (unsigned long)millis(),
                i,
                MOTORS[i].angle,
                MOTORS[i].speedDps,
                rssiLast,
                rssiAvg,
                distCm,
                linkUp ? 1 : 0,
                packetsThisFrame);
}

void updateMotors() {
  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;          // not time yet
  float dt = (now - lastFrame) / 1000.0f;          // seconds since last frame (~0.020)
  lastFrame = now;

  // Link lost? Park every motor once and report speed 0.
  if (linkUp && now - lastHeard > LOST_MS) {
    linkUp = false;
    Serial.println("# link lost: motors parked");
    for (int i = 0; i < NUM_MOTORS; i++) {
      MOTORS[i].angle    = MOTORS[i].angleRest;
      MOTORS[i].speedDps = 0;
      writeServo(MOTORS[i].channel, MOTORS[i].angle);
    }
  }

  float speed = linkUp ? cmToSpeed(distCm) : 0;

  for (int i = 0; i < NUM_MOTORS; i++) {
    Motor &m = MOTORS[i];
    m.speedDps = speed;

    if (linkUp) {
      // Move "speed * time" degrees in the current direction, bounce at the limits.
      m.angle += m.dir * m.speedDps * dt;
      if (m.angle >= m.angleMax) { m.angle = m.angleMax; m.dir = -1; }
      if (m.angle <= m.angleMin) { m.angle = m.angleMin; m.dir = +1; }
      writeServo(m.channel, m.angle);
    }

    printDataLine(i);      // print every frame, even when parked, so vvvv gets a steady stream
  }

  packetsThisFrame = 0;
}

// =====================================================================================
//  6. SETUP + LOOP
// =====================================================================================

uint32_t nextPing = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- Servos ---
  Wire.begin(SDA_PIN, SCL_PIN);
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);
  delay(10);
  for (int i = 0; i < NUM_MOTORS; i++) {
    MOTORS[i].angle    = MOTORS[i].angleRest;
    MOTORS[i].dir      = +1;
    MOTORS[i].speedDps = 0;
    writeServo(MOTORS[i].channel, MOTORS[i].angle);
  }

  // --- Radio (same recipe as esp_now_pingpong) ---
  WiFi.mode(WIFI_STA);                 // station mode, no router needed
  WiFi.disconnect();
  WiFi.setTxPower(TX_POWER);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("# ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;                    // 0 = current channel
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  // --- Info lines (start with '#', vvvv ignores them) ---
  Serial.print("# my MAC: "); Serial.println(WiFi.macAddress());
  Serial.printf("# channel %d, send every %d ms, %d motor(s)\n", WIFI_CHANNEL, SEND_INTERVAL_MS, NUM_MOTORS);
  Serial.printf("# calibration: RSSI_AT_1M=%.1f  PATH_LOSS_N=%.2f\n", RSSI_AT_1M, PATH_LOSS_N);
  Serial.println("# format: D,time_ms,motor,angle_deg,speed_dps,rssi_dbm,rssi_avg_dbm,dist_cm,link,packets");

  randomSeed(esp_random());
  nextPing  = millis() + random(0, SEND_INTERVAL_MS);
  lastFrame = millis();
}

void loop() {
  // 1) Send our PING every ~20 ms
  if ((int32_t)(millis() - nextPing) >= 0) {
    nextPing = millis() + SEND_INTERVAL_MS + random(0, SEND_JITTER_MS + 1);
    sendPing();
  }

  // 2) Process every packet that arrived since last time
  while (qTail != qHead) {
    int rssi = rxRssi[qTail];
    qTail = (qTail + 1) % QSIZE;
    handlePacket(rssi);
  }

  // 3) Every 20 ms: move the motors and print the D-lines
  updateMotors();
}
