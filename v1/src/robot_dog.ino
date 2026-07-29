#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>

// ============================================================
// CONFIGURATION
// ============================================================

#define COMPLEX_MODE true  // true = all behaviors, false = simple sit/walk
#define TRANSITION_DELAY 150  // ms between pose steps for smooth motion

// Pin definitions
#define TRIG_PIN 4   // HC-SR04 Trig (A3 on Feather)
#define ECHO_PIN 3   // HC-SR04 Echo (A2 on Feather)
#define CALIBRATION_BUTTON_PIN 0  // SW38 button on Feather ESP32 V2

// Servo parameters
#define SERVOMIN 150    // Min pulse length (0 degrees)
#define SERVOMAX 600    // Max pulse length (180 degrees)
#define MAX_DISTANCE 200

// Servo channel mappings
#define LEFT_LEG 0
#define RIGHT_LEG 1
#define LEFT_ARM 2
#define RIGHT_ARM 3

// ============================================================
// LIBRARY OBJECTS
// ============================================================

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// ============================================================
// STRUCTS AND HELPERS
// ============================================================

struct Pose {
  int leftLeg;
  int rightLeg;
  int leftArm;
  int rightArm;
};

Pose currentPose = {90, 90, 90, 90};

// Apply angle inversion for mirrored servos
// Both arms and right leg are physically mounted in opposite orientation
int applyAngle(uint8_t channel, int angle) {
  if (channel == RIGHT_LEG || channel == RIGHT_ARM) {
    return 180 - angle;  // Invert right leg
  } else if (channel == LEFT_ARM) {
    return 180 - angle;  // Invert left arm
  }
  return angle;  // Left leg is normal
}

// Clamp angle to prevent servo overextension
int clampAngle(int angle) {
  return constrain(angle, 10, 170);
}

// Convert angle (0-180) to PCA9685 pulse width
int angleToPulse(int ang) {
  return map(clampAngle(ang), 0, 180, SERVOMIN, SERVOMAX);
}

// Set one servo with angle correction applied
void setServo(uint8_t channel, int angle) {
  int corrected = applyAngle(channel, angle);
  pwm.setPWM(channel, 0, angleToPulse(corrected));
  delayMicroseconds(TRANSITION_DELAY * 1000);
}

void applyPose(Pose p) {
  setServo(LEFT_LEG, p.leftLeg);
  delay(TRANSITION_DELAY);
  setServo(RIGHT_LEG, p.rightLeg);
  delay(TRANSITION_DELAY);
  setServo(LEFT_ARM, p.leftArm);
  delay(TRANSITION_DELAY);
  setServo(RIGHT_ARM, p.rightArm);
  delay(TRANSITION_DELAY);
  currentPose = p;
}

// Smooth servo movement with interpolation
// Moves servo gradually from one angle to another
void smoothServo(uint8_t channel, int fromAngle, int toAngle, int stepDelay) {
  int step = (fromAngle < toAngle) ? 1 : -1;
  for (int angle = fromAngle; angle != toAngle; angle += step) {
    setServo(channel, angle);
    delay(stepDelay);
  }
  setServo(channel, toAngle);
}

// ============================================================
// POSES
// ============================================================

// Neutral pose - all limbs at 90 degrees (upright position)
Pose neutralPose = {90, 90, 90, 90};

// Sitting pose - legs bent inward, arms neutral
Pose sittingPose = {0, 180, 90, 90};

// ============================================================
// CALIBRATION
// ============================================================

void calibrateServos() {
  Serial.println("Entering calibration mode...");
  Serial.println("Hold robot steady...");

  for (int angle = 0; angle <= 90; angle++) {
    setServo(LEFT_LEG, angle);
    setServo(RIGHT_LEG, angle);
    setServo(LEFT_ARM, angle);
    setServo(RIGHT_ARM, angle);
    delay(40);  // Smooth motion: 40ms per step
  }

  Serial.println("Calibration complete!");
  Serial.println("All servos at neutral position.");
  delay(1000);  // Hold final pose briefly
}

// ============================================================
// MOVEMENT FUNCTIONS
// ============================================================

void sit() {
  Serial.println("Action: Sit");
  smoothServo(LEFT_LEG, currentPose.leftLeg, sittingPose.leftLeg, 15);
  smoothServo(RIGHT_LEG, currentPose.rightLeg, sittingPose.rightLeg, 15);
  currentPose = sittingPose;
}

void wave() {
  Serial.println("Action: Wave");
  for (int i = 0; i < 3; i++) {
    smoothServo(LEFT_ARM, currentPose.leftArm, 60, 10);
    delay(300);
    smoothServo(LEFT_ARM, currentPose.leftArm, 120, 10);
    delay(300);
  }
  smoothServo(LEFT_ARM, currentPose.leftArm, 90, 10);
}

void walkForward() {
  Serial.println("Action: Walk Forward");
  smoothServo(LEFT_LEG, currentPose.leftLeg, 60, 15);
  delay(200);
  smoothServo(RIGHT_LEG, currentPose.rightLeg, 120, 15);
  delay(200);
  applyPose(neutralPose);
}

void walkBackward() {
  Serial.println("Action: Walk Backward");
  smoothServo(LEFT_LEG, currentPose.leftLeg, 120, 15);
  delay(200);
  smoothServo(RIGHT_LEG, currentPose.rightLeg, 60, 15);
  delay(200);
  applyPose(neutralPose);
}

void lookAround() {
  Serial.println("Action: Look Around");
  smoothServo(LEFT_ARM, currentPose.leftArm, 30, 12);
  delay(400);
  smoothServo(RIGHT_ARM, currentPose.rightArm, 30, 12);
  delay(400);
  smoothServo(LEFT_ARM, currentPose.leftArm, 150, 12);
  delay(400);
  smoothServo(RIGHT_ARM, currentPose.rightArm, 150, 12);
  delay(400);
  applyPose(neutralPose);
}

void layDown() {
  Serial.println("Action: Lay Down");
  smoothServo(LEFT_LEG, currentPose.leftLeg, 150, 15);
  smoothServo(RIGHT_LEG, currentPose.rightLeg, 30, 15);
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println("=================================");
  Serial.println("Mini Robot Swarm Unit - Booting...");
  Serial.println("=================================");

  pwm.begin();
  pwm.setPWMFreq(50);  // Standard servo frequency (50Hz)

  pinMode(CALIBRATION_BUTTON_PIN, INPUT);  // SW38 has onboard pull-up

  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    Serial.println("SW38 held - starting calibration mode.");
    calibrateServos();
  } else {
    Serial.println("Applying neutral pose.");
    applyPose(neutralPose);
  }

  Serial.println("Robot ready!");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  int distance = sonar.ping_cm();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (COMPLEX_MODE) {
    if (distance > 0 && distance < 5) {
      sit();
    } else if (distance >= 5 && distance < 20) {
      wave();
      delay(800);
      applyPose(neutralPose);
    } else if (distance >= 20 && distance < 50) {
      lookAround();
    } else if (distance >= 50 && distance < 100) {
      // Idle/neutral - just hold pose
    } else if (distance > 0) {
      walkForward();
    } else {
      sit();
    }
  } else {
    if (distance > 0 && distance < 20) {
      sit();
    } else {
      walkForward();
    }
  }
}
