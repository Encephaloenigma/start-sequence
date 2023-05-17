#include "functions.h"

const int BUTTON = 2;
const int HORN = 13;
const int longLeng = 1000;
const int shortLeng = 200;
const int pause = 500;
SequenceState state = SequenceState::IDLE;

// alternatively, {{1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {4, 0, 1}, {5, 0, 1}, {10, 0, 1}, {20, 0, 2}, {30, 0, 3}, {60, 1, 0}, {90, 1, 3}, {2 * 60, 2, 0}, {3 * 60, 3, 0}}

// seconds, long, short
const int timings[][3] = { { 3 * 60, 3, 0 }, { 2 * 60, 2, 0 }, { 90, 1, 3 }, { 60, 1, 0 }, { 30, 0, 3 }, { 20, 0, 2 }, { 10, 0, 1 }, { 5, 0, 1 }, { 4, 0, 1 }, { 3, 0, 1 }, { 2, 0, 1 }, { 1, 0, 1 } };
const int timingsLength = sizeof(timings) / sizeof(timings[0]);

void setup() {
  pinMode(HORN, OUTPUT);
  pinMode(BUTTON, INPUT);
  Serial.begin(9600);
  // testing
  delay(1000);
  state = SequenceState::IN_SEQUENCE;
  startSequence(Duration::THREE);
}

void checkButton() {
  int buttonState = digitalRead(BUTTON);
  if (buttonState == HIGH) {
    switch (state) {
      case SequenceState::IDLE:
        state = SequenceState::IN_SEQUENCE;
        startSequence(Duration::THREE);
        break;
      case SequenceState::IN_SEQUENCE:
        state = SequenceState::IDLE;
        break;
    }
  }
}

void loop() {
  // checkButton();
}

void startSequence(Duration duration) {
  soundHorn(0, 5);
  switch (duration) {
    case Duration::THREE:
      threeMinuteSequence();
      break;
    case Duration::ONE:
      oneMinuteSequence();
      break;
  }
  soundHorn(1, 0);
}

void oneMinuteSequence() {
  return;
}

void threeMinuteSequence() {
  unsigned long start = millis();
  long current = 3L * 60L * 1000L - getDelta(start);
  int i = 0;
  while (current >= 0 && state == SequenceState::IN_SEQUENCE) {
    current = 3L * 60L * 1000L - getDelta(start);
    if (current <= (long)timings[i][0] * 1000L && i < timingsLength) {
      soundHorn(timings[i][1], timings[i][2]);
      i++;
    }
    checkButton();
  }
}

unsigned long getDelta(unsigned long start) {
  return millis() - start;
}

void longHorn() {
  digitalWrite(HORN, HIGH);
  delay(longLeng);
  digitalWrite(HORN, LOW);
  delay(pause);
}

void shortHorn() {
  digitalWrite(HORN, HIGH);
  delay(shortLeng);
  digitalWrite(HORN, LOW);
  delay(pause);
}

void soundHorn(int longs, int shorts) {
  Serial.print(longs);
  Serial.print(", ");
  Serial.print(shorts);
  Serial.print("\n");
  for (int i = 0; i < longs; i++) {
    if (SequenceState::IDLE) {
      return;
    }
    longHorn();
    checkButton();
  }
  for (int i = 0; i < shorts; i++) {
    if (SequenceState::IDLE) {
      return;
    }
    shortHorn();
    checkButton();
  }
}
