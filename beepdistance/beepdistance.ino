// Pin Definitions
const int trigPin = 9;   // Connect to HC-SR04 Trig
const int echoPin = 10;  // Connect to HC-SR04 Echo
const int buzzerPin = 11; // Connect to Buzzer (+)
const int ledPin = 12;    // Connect to LED (+) via 220 ohm resistor

// Ranging Variables
long duration;
int distance;
int safetyDistance = 10; // Starts beeping at 30cm
int stopDistance = 5;    // Continuous sound at 5cm

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600); // For monitoring distance in the Serial Monitor
}

void loop() {
  // Trigger the ultrasonic pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the return time (duration)
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate distance in cm (Speed of sound = 0.034 cm/us)
  distance = duration * 0.034 / 2;

  if (distance <= stopDistance && distance > 0) {
    // Stage 1: Continuous sound/light if very close
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
  } 
  else if (distance > stopDistance && distance <= safetyDistance) {
    // Stage 2: Variable beeping (closer = shorter delay = faster beeps)
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    delay(distance * 5); // Delay decreases with distance
    
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
    delay(distance * 5);
  } 
  else {
    // Stage 3: Turn off if no object or object is too far away
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  // Debugging output
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  delay(50); // Small delay for stability
}
