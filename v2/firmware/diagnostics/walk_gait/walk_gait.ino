/*
 * WALK GAIT  —  smooth, live-tunable quadruped gait
 * ==================================================
 * Same hardware as motors_only.ino: PCA9685 @ 0x40, OE on GPIO10, I2C 6/7.
 *
 * ROBOT: quadruped, ONE servo per leg (1 DOF each).  Channel map:
 *        ch0 = BL  (back-left)
 *        ch1 = BR  (back-right)
 *        ch2 = FL  (front-left)    <- j3
 *        ch3 = FR  (front-right)   <- j2
 *        front = the end with the distance sensor. (channel 4 unused / not wired)
 *
 * HOW IT MOVES
 *   Each leg follows a smooth sine wave:
 *       pulse = center + DIR * AMP * sin( 2*pi*t/PERIOD + PHASE )
 *   With only 1 DOF per leg you can't pick a foot up, so "walking" is a
 *   phase-offset swing: the legs sweep fore/aft in a coordinated pattern and
 *   the robot shuffles forward.  The whole trick is in two per-leg knobs:
 *       DIR   = which way is "forward" for THAT servo (+1 or -1)   <-- mirror L/R
 *       PHASE = when in the stride that leg swings (degrees)       <-- the gait
 *
 * >>> YOU DO NOT NEED TO RECOMPILE TO TUNE. <<<
 *   Open Serial Monitor @115200, newline ending, and type commands (send '?').
 *   Change amplitude / phase / direction / period / trim live while it walks,
 *   then copy the final numbers into the DEFAULTS block below to keep them.
 *
 * SAFETY: every command is clamped to [SAFE_MIN, SAFE_MAX] so a servo can
 *   never be driven into a mechanical stall.  Widen these only once you know
 *   the real end-stops of your legs.
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ---------- hardware ----------
#define PIN_SDA 6
#define PIN_SCL 7
#define PIN_OE  10          // active-LOW output enable
#define SERVO_FREQ 50
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ---------- pulse calibration (PCA9685 counts, 0..4095 @ 50 Hz) ----------
#define CENTER   307        // ~1.5 ms neutral  (same as motors_only)
#define SAFE_MIN 160        // hard floor: never command below this
#define SAFE_MAX 455        // hard ceiling: never command above this

// ---------- legs ----------
#define NLEGS 4
enum { BL = 0, BR = 1, FR = 2, FL = 3 };            // array index (NOT channel)
const char*   LEG_NAME[NLEGS] = { "BL", "BR", "FR", "FL" };
const uint8_t LEG_CH[NLEGS]   = {  0,    1,    3,    2  };  // idx2->ch3(FR), idx3->ch2(FL)

// ================= DEFAULTS — edit these to make tuning permanent =========
int   legCenter[NLEGS] = {  357,    392,    387,    372  }; // measured per-leg neutral (BL,BR,FR,FL)
int   legAmp[NLEGS]    = {   55,     55,     55,     55   }; // swing size, counts
float legPhase[NLEGS]  = {    0,    180,      0,    180   }; // diagonal trot: (BL,FR)=0  (BR,FL)=180
int   legDir[NLEGS]    = {   +1,     -1,     -1,     +1   }; // left(BL,FL)=+1  right(BR,FR)=-1  (guess)
float periodMs         = 1200;                              // ms per full stride
// ==========================================================================

bool  running  = false;     // gait on/off
float ampScale = 0.0;       // 0..1 ease-in/out so starts & stops are gentle

// ---------- waveform mode ----------
//  0 = SINE  : smooth continuous trot, every leg always moving (default)
//  1 = CREEP : one diagonal STROKES while the other HOLDS planted, then they swap.
//              Each active leg does a raised-cosine "flick" (center -> amp -> center)
//              during its window, and sits at center the rest of the time.
int   waveMode  = 0;
float swingFrac = 0.5;      // CREEP: fraction of the cycle a leg spends stroking
                            //   0.5 = pairs alternate with no overlap (what you asked for)
                            //   <0.5 = a pause with all legs planted between strokes
                            //   >0.5 = the two strokes overlap a little

// ---------- I2C scan (handy sanity check) ----------
void scanI2C() {
  Serial.println("[I2C] scanning...");
  bool found = false;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print("  found 0x"); Serial.println(a, HEX); found = true; }
  }
  if (!found) Serial.println("  NONE found! (check wiring/power)");
}

// ---------- gait presets: just phase tables (degrees), enum order LB,RB,LF,RF ----------
void preset(const char* name) {
  if      (!strcmp(name, "trot"))  { float p[]={0,180,  0,180}; memcpy(legPhase,p,sizeof(p)); waveMode=0; } // diagonal, all legs moving
  else if (!strcmp(name, "creep")) { float p[]={0,180,  0,180}; memcpy(legPhase,p,sizeof(p)); waveMode=1; } // diagonal, ONE pair strokes while other holds
  else if (!strcmp(name, "walk"))  { float p[]={0, 90,180,270}; memcpy(legPhase,p,sizeof(p)); waveMode=0; } // one leg at a time
  else if (!strcmp(name, "pace"))  { float p[]={0,180,180,  0}; memcpy(legPhase,p,sizeof(p)); waveMode=0; } // left pair / right pair
  else if (!strcmp(name, "bound")) { float p[]={0,  0,180,180}; memcpy(legPhase,p,sizeof(p)); waveMode=0; } // back pair / front pair
  else { Serial.println("presets: trot | creep | walk | pace | bound"); return; }
  Serial.print("preset -> "); Serial.println(name);
}

void printStatus() {
  Serial.println("---- status ----");
  Serial.print("running="); Serial.print(running ? "YES" : "no");
  Serial.print("  period="); Serial.print(periodMs); Serial.println(" ms");
  for (int i = 0; i < NLEGS; i++) {
    Serial.print("  ["); Serial.print(i); Serial.print("] ");
    Serial.print(LEG_NAME[i]); Serial.print(" ch"); Serial.print(LEG_CH[i]);
    Serial.print("  center="); Serial.print(legCenter[i]);
    Serial.print("  amp=");    Serial.print(legAmp[i]);
    Serial.print("  phase=");  Serial.print(legPhase[i]);
    Serial.print("  dir=");    Serial.println(legDir[i]);
  }
}

void help() {
  Serial.println();
  Serial.println("========== WALK GAIT commands (leg index: 0=LB 1=RB 2=LF 3=RF) ==========");
  Serial.println("  g            start walking (eases in)");
  Serial.println("  s            stop  (eases legs back to center)");
  Serial.println("  c            center all legs immediately");
  Serial.println("  v            reverse travel (flips all 4 directions in one key)");
  Serial.println("  ?            this help + status");
  Serial.println("  P <ms>       stride period, e.g.  P 900   (smaller = faster)");
  Serial.println("  f <preset>   gait: f trot | f creep | f walk | f pace | f bound");
  Serial.println("               (creep = one diagonal strokes while the other holds planted)");
  Serial.println("  D <0.2..0.8> creep duty: fraction of cycle a leg strokes (0.5 = strict alternate)");
  Serial.println("  A <counts>   set amplitude on ALL legs, e.g.  A 60");
  Serial.println("  a <i> <cts>  set amplitude of one leg,   e.g.  a 2 70");
  Serial.println("  h <i> <deg>  set phase of one leg,       e.g.  h 1 180");
  Serial.println("  d <i> <+/-1> flip a leg's forward dir,   e.g.  d 0 -1");
  Serial.println("  t <i> <cts>  set a leg's center (absolute), e.g. t 3 315");
  Serial.println("  n <i> <d>    NUDGE a leg's center by d counts, e.g. n 3 -5  (watch it move)");
  Serial.println("  w            print current settings as a paste-ready DEFAULTS block");
  Serial.println("  j <i>        JOG one leg fwd/back once (find its DIR & range)");
  Serial.println("  r <ch>       RAW-probe any PCA9685 channel 0..15 (find a dead/mis-wired leg)");
  Serial.println("=========================================================================");
  printStatus();
}

// ---------- one-shot jog: sweep a single leg so you can see its motion ----------
void jog(int i) {
  if (i < 0 || i >= NLEGS) return;
  Serial.print("jog "); Serial.print(LEG_NAME[i]);
  Serial.println("  (watch: does the FOOT go toward the HEAD or the TAIL as value rises?)");
  int lo = constrain(legCenter[i] - legAmp[i], SAFE_MIN, SAFE_MAX);
  int hi = constrain(legCenter[i] + legAmp[i], SAFE_MIN, SAFE_MAX);
  pwm.setPWM(LEG_CH[i], 0, lo); delay(600);
  pwm.setPWM(LEG_CH[i], 0, hi); delay(600);
  pwm.setPWM(LEG_CH[i], 0, legCenter[i]);
}

// ---------- serial line parser ----------
void handleLine(char* s) {
  while (*s == ' ') s++;
  char cmd = *s;
  int i, v; float fv;
  switch (cmd) {
    case 'g': running = true;  Serial.println("GO");   break;
    case 's': running = false; Serial.println("STOP"); break;
    case 'v': for (int k=0;k<NLEGS;k++) legDir[k] = -legDir[k];   // reverse travel (flip all dirs)
              Serial.println("reversed travel direction"); break;
    case 'c':
      running = false; ampScale = 0;
      for (int k = 0; k < NLEGS; k++) pwm.setPWM(LEG_CH[k], 0, legCenter[k]);
      Serial.println("centered"); break;
    case '?': help(); break;
    case 'P': if (sscanf(s + 1, "%f", &fv) == 1 && fv > 100) { periodMs = fv; Serial.print("period="); Serial.println(periodMs); } break;
    case 'f': { char name[16]; if (sscanf(s + 1, "%15s", name) == 1) preset(name); } break;
    case 'D': if (sscanf(s + 1, "%f", &fv) == 1 && fv > 0.05 && fv <= 1.0) { swingFrac = fv; Serial.print("swingFrac="); Serial.println(swingFrac); } break;
    case 'A': if (sscanf(s + 1, "%d", &v) == 1) { for (int k = 0; k < NLEGS; k++) legAmp[k] = v; Serial.print("all amp="); Serial.println(v); } break;
    case 'a': if (sscanf(s + 1, "%d %d",  &i, &v)  == 2 && i >= 0 && i < NLEGS) { legAmp[i]    = v;  Serial.println("ok"); } break;
    case 'h': if (sscanf(s + 1, "%d %f",  &i, &fv) == 2 && i >= 0 && i < NLEGS) { legPhase[i]  = fv; Serial.println("ok"); } break;
    case 'd': if (sscanf(s + 1, "%d %d",  &i, &v)  == 2 && i >= 0 && i < NLEGS) { legDir[i]    = (v < 0 ? -1 : +1); Serial.println("ok"); } break;
    case 't': if (sscanf(s + 1, "%d %d",  &i, &v)  == 2 && i >= 0 && i < NLEGS) {
                legCenter[i] = constrain(v, SAFE_MIN, SAFE_MAX);
                if (!running) pwm.setPWM(LEG_CH[i], 0, legCenter[i]);   // show it live
                Serial.print(LEG_NAME[i]); Serial.print(" center="); Serial.println(legCenter[i]);
              } break;
    case 'n': if (sscanf(s + 1, "%d %d",  &i, &v)  == 2 && i >= 0 && i < NLEGS) {
                legCenter[i] = constrain(legCenter[i] + v, SAFE_MIN, SAFE_MAX);
                if (!running) pwm.setPWM(LEG_CH[i], 0, legCenter[i]);   // nudge live
                Serial.print(LEG_NAME[i]); Serial.print(" center="); Serial.println(legCenter[i]);
              } break;
    case 'w': {                                                         // dump paste-ready defaults
                Serial.println("---- copy into the DEFAULTS block ----");
                Serial.print("int   legCenter[NLEGS] = { ");
                for (int k=0;k<NLEGS;k++){Serial.print(legCenter[k]); Serial.print(k<NLEGS-1?", ":" };\n");}
                Serial.print("int   legAmp[NLEGS]    = { ");
                for (int k=0;k<NLEGS;k++){Serial.print(legAmp[k]);    Serial.print(k<NLEGS-1?", ":" };\n");}
                Serial.print("float legPhase[NLEGS]  = { ");
                for (int k=0;k<NLEGS;k++){Serial.print(legPhase[k]);  Serial.print(k<NLEGS-1?", ":" };\n");}
                Serial.print("int   legDir[NLEGS]    = { ");
                for (int k=0;k<NLEGS;k++){Serial.print(legDir[k]);    Serial.print(k<NLEGS-1?", ":" };\n");}
                Serial.print("float periodMs         = "); Serial.print(periodMs); Serial.println(";");
                Serial.print("int   waveMode         = "); Serial.print(waveMode);  Serial.println(";  // 0=sine 1=creep");
                Serial.print("float swingFrac        = "); Serial.print(swingFrac); Serial.println(";");
              } break;
    case 'j': if (sscanf(s + 1, "%d", &i) == 1) jog(i); break;
    case 'r': if (sscanf(s + 1, "%d", &i) == 1 && i >= 0 && i < 16) {   // raw channel probe
                Serial.print("raw probe ch"); Serial.println(i);
                pwm.setPWM(i, 0, CENTER - 90); delay(600);
                pwm.setPWM(i, 0, CENTER + 90); delay(600);
                pwm.setPWM(i, 0, CENTER);
              } break;
    default: if (cmd) Serial.println("? (send ? for help)"); break;
  }
}

void readSerial() {
  static char buf[48];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (n) { buf[n] = 0; handleLine(buf); n = 0; } }
    else if (n < sizeof(buf) - 1) buf[n++] = c;
  }
}

// ---------- gait engine (non-blocking, ~50 Hz updates = smooth) ----------
void updateGait() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 20) return;              // 50 Hz; matches servo refresh
  float dt = (now - last) / 1000.0f;
  last = now;

  // ease amplitude toward target so starts/stops are never jerky
  float target = running ? 1.0f : 0.0f;
  ampScale += (target - ampScale) * min(1.0f, dt * 3.0f);   // ~0.3 s ramp
  if (!running && ampScale < 0.01f) ampScale = 0;

  float cyc = now / periodMs;                               // stride counter (turns)
  for (int k = 0; k < NLEGS; k++) {
    float w;                                                // -1..+1 leg drive
    if (waveMode == 1) {
      // CREEP: local phase 0..1; stroke only inside [0, swingFrac], else hold at center
      float u = cyc + legPhase[k] / 360.0f;
      u -= floorf(u);                                       // fractional part
      w = (u < swingFrac) ? sinf(PI * u / swingFrac) : 0.0f;// raised-cosine flick, then 0
    } else {
      // SINE: continuous trot
      w = sinf(2.0f * PI * cyc + legPhase[k] * (PI / 180.0f));
    }
    int val = legCenter[k] + (int)(legDir[k] * legAmp[k] * ampScale * w);
    val = constrain(val, SAFE_MIN, SAFE_MAX);
    pwm.setPWM(LEG_CH[k], 0, val);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== WALK GAIT ===");

  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, HIGH);               // outputs OFF during init
  Wire.begin(PIN_SDA, PIN_SCL);
  scanI2C();
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  digitalWrite(PIN_OE, LOW);                // outputs ON

  for (int k = 0; k < NLEGS; k++) pwm.setPWM(LEG_CH[k], 0, legCenter[k]);
  Serial.println("centered. Send 'g' to walk, '?' for help.");
  help();
}

void loop() {
  readSerial();
  updateGait();
}
