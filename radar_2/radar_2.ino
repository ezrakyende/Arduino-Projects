#include <Servo.h>

// Pin definitions (Updated as requested)
const int TRIG_PIN = 10;
const int ECHO_PIN = 11;
const int SERVO_PIN = 9;
const int LED_PIN = 8;
const int BUZZER_PIN = 7;

// Radar settings
const int MIN_ANGLE = 15;
const int MAX_ANGLE = 165;
const int STEP_DELAY = 25;    // Speed of the sweep (lower is faster)
const int MAX_DISTANCE = 40;  // Match this with your Processing code scale

Servo radarServo;
int distance;
long duration;

void setup() {
  Serial.begin(9600);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  radarServo.attach(SERVO_PIN);
}

void loop() {
  // Sweep from Left to Right
  for(int i = MIN_ANGLE; i <= MAX_ANGLE; i++) {  
    updateRadar(i);
  }
  // Sweep from Right to Left
  for(int i = MAX_ANGLE; i > MIN_ANGLE; i--) {  
    updateRadar(i);
  }
}

void updateRadar(int angle) {
  radarServo.write(angle);
  delay(STEP_DELAY);
  
  distance = calculateDistance();
  
  // SEND DATA TO PROCESSING (The Critical Format: angle,distance.)
  Serial.print(angle);
  Serial.print(",");
  Serial.print(distance);
  Serial.print("."); 
  
  // ALARM LOGIC
  if (distance > 0 && distance < 20) { // If object is closer than 20cm
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 1500, 30); // Short high-pitch beep
  } else {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

int calculateDistance() { 
  digitalWrite(TRIG_PIN, LOW); 
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); 
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH, 25000); // 25ms timeout to prevent lag
  distance = duration * 0.034 / 2;
  
  if (distance > MAX_DISTANCE || distance <= 0) {
    return MAX_DISTANCE; // Return max if nothing is found
  }
  return distance;
}
