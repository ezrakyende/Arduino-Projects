// Pin Definitions
const int BUZZER_PIN = 8;
const int BUTTON_PIN = 2;

// Musical Notes Frequencies (Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_REST 0

// Variables for state tracking
int currentSong = 0;
const int TOTAL_SONGS = 4;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  // Using INPUT_PULLUP means the pin reads HIGH when open, LOW when pressed
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
}

void loop() {
  // Check if the button is pressed (LOW means pressed because of INPUT_PULLUP)
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Simple debounce delay to prevent double-triggering
    
    // Play the song corresponding to the current count
    switch(currentSong) {
      case 0: playTwinkle(); break;
      case 1: playBabyShark(); break;
      case 2: playMaryLamb(); break;
      case 3: playBaaBaaBlackSheep(); break;
    }
    
    // Switch to the next song next time the button is pressed
    currentSong = (currentSong + 1) % TOTAL_SONGS;
    
    // Wait until the child releases the button before listening again
    while(digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }
  }
}

// Function to play arrays of notes and durations smoothly
void playMelody(int notes[], int durations[], int totalNotes) {
  for (int i = 0; i < totalNotes; i++) {
    // If the button is pressed again WHILE a song is playing, you could interrupt it. 
    // For preschoolers, it's safer to let the short 5-second snippet finish.
    
    int duration = 1000 / durations[i]; // Calculate note duration in milliseconds
    
    if (notes[i] == NOTE_REST) {
      noTone(BUZZER_PIN);
    } else {
      tone(BUZZER_PIN, notes[i], duration);
    }
    
    // To distinguish notes, give them a tiny pause between each other (30% extra time)
    int pauseBetweenNotes = duration * 1.30;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN);
  }
}

// SONG 1: Twinkle Twinkle Little Star
void playTwinkle() {
  int melody[] = {
    NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,
    NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4
  };
  int durations[] = {
    4, 4, 4, 4, 4, 4, 2,
    4, 4, 4, 4, 4, 4, 2
  };
  playMelody(melody, durations, sizeof(melody) / sizeof(int));
}

// SONG 2: Baby Shark (Intro snippet)
void playBabyShark() {
  int melody[] = {
    NOTE_D4, NOTE_E4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4,
    NOTE_D4, NOTE_E4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4,
    NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4
  };
  int durations[] = {
    4, 4, 8, 8, 8, 4, 8, 8, 8,
    4, 4, 8, 8, 8, 4, 8, 8, 8,
    4, 4, 4, 4, 2
  };
  playMelody(melody, durations, sizeof(melody) / sizeof(int));
}

// SONG 3: Mary Had a Little Lamb
void playMaryLamb() {
  int melody[] = {
    NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_D4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_G4,
    NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_D4, NOTE_C4
  };
  int durations[] = {
    4, 4, 4, 4, 4, 4, 2,
    4, 4, 2, 4, 4, 2,
    4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 2
  };
  playMelody(melody, durations, sizeof(melody) / sizeof(int));
}

// SONG 4: Baa Baa Black Sheep
void playBaaBaaBlackSheep() {
  int melody[] = {
    NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5, NOTE_A4, NOTE_G4,
    NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4
  };
  int durations[] = {
    4, 4, 4, 4, 8, 8, 8, 8, 2,
    4, 4, 4, 4, 4, 4, 2
  };
  playMelody(melody, durations, sizeof(melody) / sizeof(int));
}
