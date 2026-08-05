#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>

// ---------- CONFIGURATION ----------

// Pin definitions
#define TRIG_PIN 4  // HC-SR04 Trig
#define ECHO_PIN 3  // HC-SR04 Echo
#define CALIBRATION_BUTTON_PIN 38  // SW38 button (GPIO 0)

// Servo parameters
#define SERVOMIN 150
#define SERVOMAX 600
#define MAX_DISTANCE 200

// Servo channel mappings
#define LEFT_LEG 0
#define RIGHT_LEG 1
#define LEFT_ARM 2
#define RIGHT_ARM 3

// ---------- LIBRARY OBJECTS ----------

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// ---------- STRUCTS AND HELPERS ----------

struct Pose {
  int leftLeg;
  int rightLeg;
  int leftArm;
  int rightArm;
};

Pose currentPose = {90, 90, 90, 90};  // optional tracking

int applyAngle(uint8_t channel, int angle) {
  if (channel == RIGHT_ARM || channel == RIGHT_LEG) {
    return 180 - angle;  // These are mirrored
  }
  return angle;  // LEFT_LEG is normal
}


// Clamp angle to prevent servo overextension
int clampAngle(int angle) {
  return constrain(angle, 10, 170);
}

// Convert angle to PCA9685 pulse width
int angleToPulse(int ang) {
  return map(clampAngle(ang), 0, 180, SERVOMIN, SERVOMAX);
}

int interpolate(int from, int to, float progress) {
  return from + (to - from) * progress;
}

// Set one servo with angle inversion applied
void setServo(uint8_t channel, int angle) {
  int corrected = applyAngle(channel, angle);
  pwm.setPWM(channel, 0, angleToPulse(corrected));
}

// Apply a full-body pose
void applyPose(Pose p) {
  setServo(LEFT_LEG, p.leftLeg);
  setServo(RIGHT_LEG, p.rightLeg);
  setServo(LEFT_ARM, p.leftArm);
  setServo(RIGHT_ARM, p.rightArm);
  currentPose = p;
}

void smoothPoseTransition(Pose fromPose, Pose toPose, int steps, int stepDelay) {
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    float progress = 0.5 - 0.5 * cos(t * PI);  // sinusoidal easing


    setServo(LEFT_LEG,  interpolate(fromPose.leftLeg,  toPose.leftLeg,  progress));
    setServo(RIGHT_LEG, interpolate(fromPose.rightLeg, toPose.rightLeg, progress));
    setServo(LEFT_ARM,  interpolate(fromPose.leftArm,  toPose.leftArm,  progress));
    setServo(RIGHT_ARM, interpolate(fromPose.rightArm, toPose.rightArm, progress));

    delay(stepDelay);
  }

  currentPose = toPose;  // Update tracking
}



// ---------- POSES ----------

Pose neutralPose = {90, 90, 90, 90};

Pose sittingPose = {
  0,     // LEFT_LEG pulled in
  0,   // RIGHT_LEG pulled in (mirrored)
  90,    // LEFT_ARM neutral
  90     // RIGHT_ARM neutral
};

// ---------- CALIBRATION ----------

void calibrateServos() {
  Serial.println("Entering calibration mode...");
  for (int angle = 0; angle <= 90; angle++) {
    setServo(LEFT_LEG, angle);
    setServo(RIGHT_LEG, angle);
    setServo(LEFT_ARM, angle);
    setServo(RIGHT_ARM, angle);
    delay(40);  // smooth motion
  }
  Serial.println("Calibration complete.");
  delay(1000);  // hold pose briefly
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  Serial.println("Robot Booting...");

  pwm.begin();
  pwm.setPWMFreq(50);  // standard servo frequency
  pinMode(CALIBRATION_BUTTON_PIN, INPUT);  // SW38 with onboard pull-up

  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    Serial.println("SW38 held — running calibration.");
    calibrateServos();
  } else {
    Serial.println("No button press — applying neutral pose.");
    applyPose(neutralPose);
  }
}

// ---------- MAIN LOOP ----------

void loop() {
  int distance = sonar.ping_cm();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Press SW38 at any time to re-calibrate
  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    Serial.println("Manual calibration triggered.");
    calibrateServos();
  }

  smoothPoseTransition(currentPose, sittingPose, 50, 15);
  delay(1000);
  smoothPoseTransition(currentPose, neutralPose, 50, 15);
  delay(2000);

   dogWalkForward(3, 400);
  delay(1000);
  dogWalkBackward(3, 400);
  delay(1000);
}

void dogWalkForward(int steps, int stepDelay) {
  Serial.println("Dog walk forward...");
  for (int i = 0; i < steps; i++) {
    // Step A
    setServo(LEFT_ARM, 60);    // front left forward
    setServo(RIGHT_ARM, 120);  // front right back
    setServo(LEFT_LEG, 120);   // back left back
    setServo(RIGHT_LEG, 60);   // back right forward
    delay(stepDelay);

    // Step B
    setServo(LEFT_ARM, 120);
    setServo(RIGHT_ARM, 60);
    setServo(LEFT_LEG, 60);
    setServo(RIGHT_LEG, 120);
    delay(stepDelay);
  }

  applyPose(neutralPose);  // return to base pose
}

void dogWalkBackward(int steps, int stepDelay) {
  Serial.println("Dog walk backward...");
  for (int i = 0; i < steps; i++) {
    // Step A reversed
    setServo(LEFT_ARM, 120);
    setServo(RIGHT_ARM, 60);
    setServo(LEFT_LEG, 60);
    setServo(RIGHT_LEG, 120);
    delay(stepDelay);

    // Step B reversed
    setServo(LEFT_ARM, 60);
    setServo(RIGHT_ARM, 120);
    setServo(LEFT_LEG, 120);
    setServo(RIGHT_LEG, 60);
    delay(stepDelay);
  }

  applyPose(neutralPose);
}

