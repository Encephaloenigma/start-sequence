// represents the state the machine is in
enum SequenceState {
  IDLE,
  IN_SEQUENCE,
  POST_SEQUENCE,
};

// the duration of the start sequence
// number cooresponds to the index of `timings` it starts at
enum Duration {
  FIVE = 0,
  THREE = 1,
  ONE = 4,

};

// the button is on pin 2
const int BUTTON = 2;
// the horn in on pin 13 (actually the buildin led for testing)
const int HORN = LED_BUILTIN;
// the digital input for the top pole of the DPDT on-off-on switch
const int TOP_PIN = 3;
// the input for bottom pole of said switch
const int BOTTOM_PIN = 4;
// the time the long horn will fire
const int longHornDelay = 1000;
// the length the short horn will fire
const int shortHornDelay = 200;
// the time inbetween subsequent horn firings
const int pause = 400;

// the state of the system
// can be IDLE, in which case the system just waits for a sequence to start,
// IN_SEQUENCE, which runs a Duration::THREE or Duration::ONE length sequence while still checking for button presses as an interrupt,
// and POST_SEQUENCE, which is active for some time after a sequence finishes, and if the button is pressed during this time a general recall will be sounded.
SequenceState state = SequenceState::IDLE;

// the timings of the Duration::THREE sequence,
// with the first number being the seconds at which to fire
// and the second and third being number of long and short horn lengths to fire
const int timings[][3] = {
  // 5 min
  { 5 * 60, 5, 0 },
  // 3 min
  { 3 * 60, 3, 0 },
  { 2 * 60, 2, 0 },
  { 90, 1, 3 },
  // one min
  { 60, 1, 0 },
  { 30, 0, 3 },
  { 20, 0, 2 },
  { 10, 0, 1 },
  { 5, 0, 1 },
  { 4, 0, 1 },
  { 3, 0, 1 },
  { 2, 0, 1 },
  { 1, 0, 1 }
};

// length of `timings` array
const int timingsLength = sizeof(timings) / sizeof(timings[0]);

void setup() {
  pinMode(HORN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  checkButton();
}

int lastButtonState = LOW;
int buttonState = LOW;

// main state machine
void checkButton() {
  lastButtonState = buttonState;
  buttonState = digitalRead(BUTTON);
  if (lastButtonState == LOW && buttonState == HIGH) {
    switch (state) {
      case SequenceState::IDLE:
        state = SequenceState::IN_SEQUENCE;
        startSequence(getDuration());
        break;
      case SequenceState::IN_SEQUENCE:
        state = SequenceState::IDLE;
        break;
    }
  }
}

// preferable to delay as it still checks the button instead of doing no-ops
void wait(long ms) {
  long start = (long) millis();
  while (getDelta(start) < ms) {
    checkButton();
  }
}

// self-explanitory
void startSequence(Duration duration) {
  soundHorn(0, 5);
  wait(pause);
  // must be a long because three mins is 180000 ms, which is larger than 2^16
  long timer;
  // set up iteration through the timings array, only going to the next item when milliseconds to the current item is reached
  int i = duration;
  switch (duration) {
    case Duration::FIVE:
      timer = 5L * 60L * 1000L;
      break;
    case Duration::THREE:
      timer = 3L * 60L * 1000L;
      break;
    case Duration::ONE:
      timer = 60L * 1000L;
      break;
  }
  long start = (long) millis();
    // can go negative, we just don't want it via overflow
  long current = timer - getDelta(start);
  // loops until current is neg (three mins have passed) or the state gets changed by a button press
  while (current >= 0 && state == SequenceState::IN_SEQUENCE) {
    current = timer - getDelta(start);
    // timings also must be a long, additionally this makes sure that i stays within the bound of `timings`
    if (current <= (long)timings[i][0] * 1000L && i < timingsLength - duration) {
      soundHorn(timings[i][1], timings[i][2]);
      i++;
    }
    checkButton();
  }
  if (state == SequenceState::IN_SEQUENCE) {
    soundHorn(1, 0);
    // post sequence not implemented yet, would just softlock
    // state = POST_SEQUENCE;
  } else {
    Serial.print("cancelled sequence\n");
  }
}

// get milliseconds since `start`
long getDelta(long start) {
  long delta = (long) millis() - start;
  return delta;
}

void longHorn() {
  digitalWrite(HORN, HIGH);
  wait(longHornDelay);
  digitalWrite(HORN, LOW);
  wait(pause);
}

void shortHorn() {
  digitalWrite(HORN, HIGH);
  wait(shortHornDelay);
  digitalWrite(HORN, LOW);
  wait(pause);
}

void soundHorn(int longs, int shorts) {
  Serial.print("horn pattern: ");
  Serial.print(longs);
  Serial.print(", ");
  Serial.print(shorts);
  Serial.print("\n");
  for (int i = 0; i < longs; i++) {
    if (state == SequenceState::IDLE) {
      return;
    }
    longHorn();
    checkButton();
  }
  for (int i = 0; i < shorts; i++) {
    if (state == SequenceState::IDLE) {
      return;
    }
    shortHorn();
    checkButton();
  }
}

Duration getDuration() {
  bool bottomPole = digitalRead(TOP_PIN) == HIGH;
  bool topPole = digitalRead(BOTTOM_PIN) == HIGH;
  return !topPole && !bottomPole ? Duration::THREE : topPole ? Duration::FIVE : Duration::ONE;
}
