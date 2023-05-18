#include "SevSeg.h"
SevSeg sevseg; //Initiate a seven segment controller object


// represents the state the machine is in
enum SequenceState {
  IDLE,
  IN_SEQUENCE,
  POST_SEQUENCE,
};

// the duration of the start sequence
// number cooresponds to the index of `timings` it starts at
enum Duration {
  FIVE = 5,
  THREE = 3,
  ONE = 1,
};

// one min, three min, five min
const int durationToIndex[] = {NULL, 4, NULL, 1, NULL, 0};
const int durationToColor[] = {NULL, 0b110, NULL, 0b101, NULL, 0b100};

// the button is on pin 2
const int BUTTON = 2;
// the horn in on pin 13 (actually the buildin led for testing)
const int HORN = 9;
// the digital input for the top pole of the DPDT on-off-on switch
const int TOP_PIN = 3;
// the input for bottom pole of said switch
const int BOTTOM_PIN = 4;
// the pin of input for stopping a sequence or initalizing a general recall
const int STOP_RECALL_BUTTON = 5;
// no longer using color to show state since there are no more GPIO pins to spare :P
// const int red_pin = 8;
// const int green_pin = 7;
// const int blue_pin = 6;

// the time the long horn will fire
const int longHornDelay = 1000;
// the length the short horn will fire
const int shortHornDelay = 200;
// the time inbetween subsequent horn firings
const int pause = 400;

// global for putting time on the screen
long current;
long timer;
long start;
bool timerRunning;

// the state of the system
// can be IDLE, in which case the system just waits for a sequence to start,
// IN_SEQUENCE, which runs a Duration::THREE or Duration::ONE length sequence while still checking for button presses as an interrupt,
// and POST_SEQUENCE, which is active for some time after a sequence finishes, and if the button is pressed during this time a general recall will be sounded.
SequenceState state = SequenceState::IDLE;

// the timings of the Duration::THREE sequence,
// with the first number being the seconds at which to fire
// and the second and third being number of long and short horn lengths to fire
const int timings[][3] = {
  // five min
  { 5 * 60, 5, 0 },
  // three min
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

// from SevSeg.cpp
// digitCodeMap indicate which segments must be illuminated to display
// each number.
static const uint8_t digitCodeMap[] = {
  // GFEDCBA  Segments      7-segment map:
  0b00111111, // 0   "0"          AAA
  0b00000110, // 1   "1"         F   B
  0b01011011, // 2   "2"         F   B
  0b01001111, // 3   "3"          GGG
  0b01100110, // 4   "4"         E   C
  0b01101101, // 5   "5"         E   C
  0b01111101, // 6   "6"          DDD
  0b00000111, // 7   "7"
  0b01111111, // 8   "8"
  0b01101111, // 9   "9"
};

uint8_t LCD_PERIOD = 0b10000000;


// length of `timings` array
const int timingsLength = sizeof(timings) / sizeof(timings[0]);

void setup() {
  pinMode(HORN, OUTPUT);
  pinMode(TOP_PIN, INPUT);
  pinMode(BOTTOM_PIN, INPUT);
  pinMode(BUTTON, INPUT);
  // pinMode(red_pin, OUTPUT);
  // pinMode(green_pin, OUTPUT);
  // pinMode(blue_pin, OUTPUT);
  // setColor(0b111);
  Serial.begin(9600);
  // 7 segment init
  // 
  byte numDigits = 4;  
  byte digitPins[] = {10, 11, 12, 13};
  byte segmentPins[] = {14, 15, 16, 17, 18, 19, 6, 7};
  bool resistorsOnSegments = 0; 
  // variable above indicates that 4 resistors were placed on the digit pins.
  // set variable to 1 if you want to use 8 resistors on the segment pins.
  sevseg.begin(COMMON_ANODE, numDigits, digitPins, segmentPins, resistorsOnSegments);
  sevseg.setBrightness(-100);
}

void loop() {
  checkLoop();
}

void checkLoop() {
  if (state == SequenceState::IN_SEQUENCE) {
    updateCurrent();
  }
  checkButtons();
  updateScreen();
}

void updateScreen() {
  if (timerRunning) {
    int minutes = current / (1000L * 60L);
    int seconds = current / 1000 % 60;
    int decimal = current / 100 % 10;
    uint8_t segments[4] = {
      digitCodeMap[minutes] | LCD_PERIOD,
      digitCodeMap[seconds / 10],
      digitCodeMap[seconds % 10] | LCD_PERIOD,
      digitCodeMap[decimal]
    };
    // for (int i = 0; i < 4; i++) {
    //   Serial.print(segments[i]);
    //   Serial.print(", ");
    // }
    // Serial.println();
    // Serial.println(current);
    sevseg.setSegments(segments);
  } else {
    char segments[] = {'0', '0', '0', String(getDuration())[0]};
    sevseg.setChars(segments);
  }
  sevseg.refreshDisplay(); // Must run repeatedly

}

// dangerous and bad global state management but oh well
void updateCurrent() {
  current = timer - getDelta(start);
}

int lastStartButtonState = LOW;
int lastStopButtonState = LOW;
int startButtonState = LOW;
int stopButtonState = LOW;

// main state machine
void checkButtons() {
  lastStartButtonState = startButtonState;
  startButtonState = digitalRead(BUTTON);
  lastStopButtonState = stopButtonState;
  stopButtonState = digitalRead(STOP_RECALL_BUTTON);
  if (lastStartButtonState == LOW && startButtonState == HIGH) {
    switch (state) {
      case SequenceState::IDLE:
      case SequenceState::POST_SEQUENCE:
        state = SequenceState::IN_SEQUENCE;
        startSequence(getDuration());
        break;
    }
  }
  if (lastStopButtonState == LOW && stopButtonState == HIGH) {
    switch (state) {
      case SequenceState::POST_SEQUENCE:
        startRecall();
      case SequenceState::IN_SEQUENCE:
        state = SequenceState::IDLE;
        // setColor(0b111);
        break;
    }
  }
  
}

// preferable to delay as it still checks the button instead of doing no-ops
void wait(long ms) {
  long waitStart = (long)millis();
  while (getDelta(waitStart) < ms) {
    checkLoop();
  }
}

// self-explanitory
void startSequence(Duration duration) {
  // must be a long because three mins is 180000 ms, which is larger than 2^16
  // set up iteration through the timings array, only going to the next item when milliseconds to the current item is reached
  int i = durationToIndex[duration];
  // setColor(durationToColor[duration]);
  timer = (long) duration * 60L * 1000L;
  soundHorn(0, 5);
  wait(pause);
  start = (long) millis();
  timerRunning = true;
  // can go negative, we just don't want it via overflow
  updateCurrent();
  // loops until current is neg (three mins have passed) or the state gets changed by a button press
  while (current >= 0 && state == SequenceState::IN_SEQUENCE) {
    checkLoop();
    // timings also must be a long, additionally this makes sure that i stays within the bound of `timings`
    if (current <= (long)timings[i][0] * 1000L && i < timingsLength) {
      soundHorn(timings[i][1], timings[i][2]);
      i++;
    }
  }
  timerRunning = false;
  if (state == SequenceState::IN_SEQUENCE) {
    soundHorn(1, 0);
    state = SequenceState::POST_SEQUENCE;
    Serial.println("in post sequence")
  } else {
    Serial.println("cancelled sequence");
  }
}

void startRecall() {
  // TODO: implement :P
}

// color is a three bit number, rgb
void setColor(int color) {
  return;
  // digitalWrite(red_pin, (color >> 2) & 1);
  // digitalWrite(green_pin, (color >> 1) & 1);
  // digitalWrite(blue_pin, color & 1);
}

// get milliseconds since `start`
long getDelta(long start) {
  long delta = (long)millis() - start;
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
  Serial.println(shorts);
  for (int i = 0; i < longs; i++) {
    if (state == SequenceState::IDLE) {
      return;
    }
    longHorn();
    checkLoop();
  }
  for (int i = 0; i < shorts; i++) {
    if (state == SequenceState::IDLE) {
      return;
    }
    shortHorn();
    checkLoop();
  }
}

Duration getDuration() {
  bool bottomPole = digitalRead(TOP_PIN) == HIGH;
  bool topPole = digitalRead(BOTTOM_PIN) == HIGH;
  return !topPole && !bottomPole ? Duration::THREE : topPole ? Duration::FIVE : Duration::ONE;
}
