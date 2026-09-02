#include <Servo.h>

const int TRIG_PIN = 9;
const int ECHO_PIN = 10;
const int SERVO_PIN = 11;

const int TRIGGER_DISTANCE = 25; // Distance in cm
const int OPEN_ANGLE = 110;      // Increased angle to ensure full opening
const int CLOSE_ANGLE = 10;      // Slight offset to prevent mechanical strain
const int STEP_DELAY = 5;        // MUCH LOWER DELAY = WAY FASTER MOVEMENT
const long HOLD_TIME = 3000;     // 3 seconds hold

Servo binServo;
int currentAngle = CLOSE_ANGLE;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  binServo.attach(SERVO_PIN);
  binServo.write(currentAngle);
  Serial.begin(9600);
}

void loop() {
  if (getDistance() <= TRIGGER_DISTANCE) {
    openLidSlowly();
    delay(HOLD_TIME);
    closeLidWithInterrupt();
  }
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 20000); // 20ms timeout prevents freezing if sensor misses echo
  if (duration == 0) return 999;                 // Return safe high distance if sensor glitches
  
  return duration * 0.034 / 2;
}

void openLidSlowly() {
  for (int pos = currentAngle; pos <= OPEN_ANGLE; pos += 2) { // Increments by 2 for faster speed
    binServo.write(pos);
    currentAngle = pos;
    delay(STEP_DELAY);
  }
}

void closeLidWithInterrupt() {
  int checkCounter = 0;
  for (int pos = currentAngle; pos >= CLOSE_ANGLE; pos -= 2) { // Decrements by 2 for faster speed
    
    // Check distance every 4 steps to keep servo motion smooth and fast
    if (checkCounter++ % 4 == 0) {
      if (getDistance() <= TRIGGER_DISTANCE) {
        openLidSlowly();
        delay(HOLD_TIME);
        pos = OPEN_ANGLE;
        continue;
      }
    }
    
    binServo.write(pos);
    currentAngle = pos;
    delay(STEP_DELAY);
  }
}
