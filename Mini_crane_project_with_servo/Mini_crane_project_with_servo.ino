#include <Servo.h>

// Pin Definitions
const int LIFT_BTN_PIN = 2;    
const int ROTATE_BTN_PIN = 3;  
const int TOGGLE_BTN_PIN = 4;  
const int LED_PIN = 5;         
const int LIFT_SERVO_PIN = 9;  
const int ROTATE_SERVO_PIN = 10;

// Servo Objects
Servo liftServo;
Servo rotateServo;

// Servo Positions
int liftPosition = 90;
int rotatePosition = 90;

// SPEED CONTROL (The Golden Middle)
// This is the pause (in milliseconds) between each 1-degree step.
// Higher number = Slower/Smoother. Lower number = Faster.
const int CRANE_SPEED_DELAY = 15; 

// Direction State Variable
bool isReversed = false; 

// Button Debounce Helper
bool lastToggleState = HIGH;

void setup() {
  pinMode(LIFT_BTN_PIN, INPUT_PULLUP);
  pinMode(ROTATE_BTN_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  liftServo.attach(LIFT_SERVO_PIN);
  rotateServo.attach(ROTATE_SERVO_PIN);
  
  liftServo.write(liftPosition);
  rotateServo.write(rotatePosition);
}

void loop() {
  handleDirectionToggle();
  handleLiftingServo();
  handleRotationServo();
  
  // This delay controls how fast the steps happen
  delay(CRANE_SPEED_DELAY); 
}

void handleDirectionToggle() {
  bool currentToggleState = digitalRead(TOGGLE_BTN_PIN);
  
  if (currentToggleState == LOW && lastToggleState == HIGH) {
    isReversed = !isReversed;  
    digitalWrite(LED_PIN, isReversed ? HIGH : LOW); 
    delay(200); // Debounce delay for toggle switch only
  }
  lastToggleState = currentToggleState;
}

void handleLiftingServo() {
  if (digitalRead(LIFT_BTN_PIN) == LOW) {
    if (!isReversed) {
      liftPosition += 1; // Smooth 1-degree increments
      if (liftPosition > 180) liftPosition = 180;
    } else {
      liftPosition -= 1; // Smooth 1-degree decrements
      if (liftPosition < 0) liftPosition = 0;
    }
    liftServo.write(liftPosition);
  }
}

void handleRotationServo() {
  if (digitalRead(ROTATE_BTN_PIN) == LOW) {
    if (!isReversed) {
      rotatePosition += 1; // Smooth 1-degree increments
      if (rotatePosition > 180) rotatePosition = 180;
    } else {
      rotatePosition -= 1; // Smooth 1-degree decrements
      if (rotatePosition < 0) rotatePosition = 0;
    }
    rotateServo.write(rotatePosition);
  }
}
