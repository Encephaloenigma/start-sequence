#include "functions.h"

const int HORN = LED_BUILTIN;
const int BUTTON = 2;
bool stopRequested = false;
SequenceState.IDLE state = false;

void setup() {
  pinMode(HORN, OUTPUT)
  pinMode(BUTTON, INPUT)
}

void checkButton() {
  int buttonState = digitalRead(BUTTON)
  if (buttonState = HIGH) {
    switch (state):
      case SequenceState.IDLE:
        runStart(3);
      case SequenceState.IN_SEQUENCE:
        state = SequenceState.IDLE;
  } 
}

void loop() {
  checkButton();
}

// duration is in minutes
void startSequence(int duration) {
  // 5 short
  long start = millis();
  while (millis() - start < duration * 1000 * 30  && state == SequenceState.IN_SEQUENCE) {
    // 3 long
    // 2 long
    // 1 long 3 short
    // 1 long
    // 3 short
    // 2 short
    // 1 short
    // 5 short
    checkButton();
  }
  startRunning = false;
}
void soundLong(){
  digitalWrite(HORN, HIGH);
  delay(LongLeng);
  digitalWrite(HORN, LOW);
}
void soundShort(){
  digitalWrite(HORN, HIGH);
  delay(ShortLeng);
  digitalWrite(HORN, LOW);
}
 void soundHorn(int longs, int shorts){
   for (i=0, i<longs, i++){
     if (SequenceState.IDLE){
       return;
     }
     soundLongs();
     checkButton();
   }
   for (i=0, i<shorts, i++){
     if (SequenceState.IDLE){
       return;
     }
     soundShorts();
     checkButton();
     }
   }
  
 }