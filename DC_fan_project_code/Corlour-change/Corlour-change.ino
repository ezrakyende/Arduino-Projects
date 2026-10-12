const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;
const int buttonPin = 2;

int colorState = 0;
bool lastButtonState = HIGH;

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP); // use internal pull-up
}

void loop() {
  bool buttonState = digitalRead(buttonPin);

  // detect button press (falling edge)
  if (lastButtonState == HIGH && buttonState == LOW) {
    colorState++;
    if (colorState > 6) {
      colorState = 0;
    }
    delay(200); // simple debounce
  }

  lastButtonState = buttonState;

  setColor(colorState);
}

void setColor(int state) {
  switch(state) {
    case 0: // Off
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
      break;

    case 1: // Red
      analogWrite(redPin, 255);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
      break;

    case 2: // Green
      analogWrite(redPin, 0);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 0);
      break;

    case 3: // Blue
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 255);
      break;

    case 4: // Yellow
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 0);
      break;

    case 5: // Cyan
      analogWrite(redPin, 0);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
      break;

    case 6: // Magenta
      analogWrite(redPin, 255);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 255);
      break;
  }
}