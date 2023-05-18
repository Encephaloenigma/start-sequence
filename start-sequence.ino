#include <AceTMI.h>
#include <AceSegment.h>

using ace_tmi::SimpleTmi1637Interface;
using ace_segment::Tm1637Module;

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

// the button is on pin 2
const int BUTTON = 4;
// the horn in on pin 13 (actually the buildin led for testing)
const int HORN = A0;
// the digital input for the top pole of the DPDT on-off-on switch
const int TOP_PIN = 3;
// the input for bottom pole of said switch
const int BOTTOM_PIN = 2;
// the pin of input for stopping a sequence or initalizing a general recall
const int STOP_RECALL_BUTTON = 5;
// no longer using color to show state since there are no more GPIO pins to spare :P
// const int red_pin = 1;
// const int green_pin = 0;
// const int blue_pin = 8;

// the length of time the long horn will fire for
const int longHornDelay = 1000;
// the length the time the short horn will fire
const int shortHornDelay = 200;
// the time inbetween subsequent horn firings
const int pause = 400;

const uint8_t CLK_PIN = A1;
const uint8_t DIO_PIN = A2;
const uint8_t NUM_DIGITS = 4;
const uint8_t DELAY_MICROS = 100;
SimpleTmi1637Interface tmiInterface(DIO_PIN, CLK_PIN, DELAY_MICROS);
Tm1637Module<SimpleTmi1637Interface, NUM_DIGITS> display(tmiInterface);

// globals for putting time on the screen

// if the timer is running (the below variables should only be accessed when this is true)
bool timerRunning;
// the current time left on the countdown during the sequence
long currentTime;
// the length of the current timer that is running
long timerLength;
// the starting time of the current sequence
long start;
//
bool warningRunning = false;
//
int warningSignal = 5;
//
bool recallRunning = false;


// the state of the system
// can be IDLE, in which case the system just waits for a sequence to start
// IN_SEQUENCE, which runs a Duration::THREE or Duration::ONE length sequence while still checking for button presses as an interrupt
// POST_SEQUENCE, which is active after a sequence finishes, and if the button is pressed during this time a general recall will be sounded
SequenceState state = SequenceState::IDLE;



// the timings for the sequences
// with the first number being the seconds at which to fire
// and the second and third being number of long and short horn lengths to fire
// the index that each Duration starts at is found with the `durationToIndex` and
// the color of each state is found with the `durationToColor` index
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


// one min, three min, five min
const int durationToIndex[] = { NULL, 4, NULL, 1, NULL, 0 };
const int durationToColor[] = { NULL, 0b110, NULL, 0b101, NULL, 0b100 };

// length of `timings` array
const int timingsLength = sizeof(timings) / sizeof(timings[0]);

// from SevSeg
// digitCodeMap indicate which segments must be illuminated to display each number
static const uint8_t digitCodeMap[] = {
  // GFEDCBA  Segments      7-segment map:
  0b00111111,  // 0   "0"          AAA
  0b00000110,  // 1   "1"         F   B
  0b01011011,  // 2   "2"         F   B
  0b01001111,  // 3   "3"          GGG
  0b01100110,  // 4   "4"         E   C
  0b01101101,  // 5   "5"         E   C
  0b01111101,  // 6   "6"          DDD
  0b00000111,  // 7   "7"
  0b01111111,  // 8   "8"
  0b01101111,  // 9   "9"
};

// a bitmask that corresponds to only the decimal place segment being lit
uint8_t LCD_PERIOD = 0b10000000;

void setup() {
  // set pin modes
  pinMode(HORN, OUTPUT);
  pinMode(TOP_PIN, INPUT);
  pinMode(BOTTOM_PIN, INPUT);
  pinMode(BUTTON, INPUT);
  pinMode(STOP_RECALL_BUTTON, INPUT);
  // pinMode(red_pin, OUTPUT);
  // pinMode(green_pin, OUTPUT);
  // pinMode(blue_pin, OUTPUT);
  // it's serial or the LEDs, and this code won't fix itself :/
  // Serial.begin(9600);
  // 7 segment init
  byte numDigits = 4;
  byte digitPins[] = { 10, 11, 12, 13 };
  byte segmentPins[] = { 14, 15, 16, 17, 18, 19, 6, 7 };
  // indicates that 4 resistors were placed on the digit pins
  // set variable to 1 if you want to use 8 resistors on the segment pins
  bool resistorsOnSegments = 0;
  display.setBrightness(100);
  tmiInterface.begin();
  display.begin();
}

// checks buttons and updates screen
// must be called constantly
// if called again after it already state change but before another state check
// everything will break and a sequence will start before the last sequence ends
// so don't do that :)
void loop() {
  checkLoop();
}

void checkLoop() {
  updateCurrent();
  updateState();
  updateScreen();
}

// screen
void updateScreen() {
  if (warningRunning) {
    for (int i = 0; i < 4; i++) {
      display.setPatternAt(i, digitCodeMap[0]);
    }
    display.setPatternAt(warningSignal < 4 ? 4 - warningSignal : 0, digitCodeMap[warningSignal]);
  } else if (recallRunning) {
    for (int i = 0; i < 4; i++) {
      display.setPatternAt(i, 0b01110110);
    }
  } else if (timerRunning) {
    int minutes = currentTime / (1000L * 60L);
    int seconds = currentTime / 1000 % 60;
    int decimal = currentTime / 100 % 10;
    uint8_t segments[4] = {
      digitCodeMap[minutes],
      digitCodeMap[seconds / 10],
      digitCodeMap[seconds % 10],
      digitCodeMap[decimal]
    };
    for (int i = 0; i < 4; i++) {
      display.setPatternAt(i, segments[i]);
    }
    // display.setDecimalPointAt(0, true);
    // display.setDecimalPointAt(3, true);
  } else {
    display.setPatternAt(0, digitCodeMap[getDuration()]);
    for (int i = 1; i < 4; i++) {
      display.setPatternAt(i, digitCodeMap[0]);
    }
  }
  // .setBrightness(2);
  display.flush();
}

// dangerous and bad global state management but oh well
// `timerRunning` *must* be `true` before calling or it won't do anything
void updateCurrent() {
  if (timerRunning) {
    currentTime = timerLength - getDelta(start);
  }
}

// storing button states for rising edge detection
// better than debouce protection
int lastStartButtonState = LOW;
int lastStopButtonState = LOW;
int startButtonState = LOW;
int stopButtonState = LOW;

// main state machine
// check start button and stop/recall button and update state accordingly
void updateState() {
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
      // for testing
      // case SequenceState::IDLE:
      // state = SequenceState::POST_SEQUENCE;
      case SequenceState::POST_SEQUENCE:
        startRecall();
      case SequenceState::IN_SEQUENCE:
        state = SequenceState::IDLE;
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

// starts a sequence of a duration
// ends if the state changes
void startSequence(Duration duration) {
  // must be a long because three mins is 180000 ms, which is larger than 2^16
  // set up iteration through the timings array, only going to the next item when milliseconds to the current item is reached
  int i = durationToIndex[duration];
  // setColor(durationToColor[duration]);
  warningRunning = true;
  timerLength = (long)duration * 60L * 1000L;
  for (warningSignal = 5; warningSignal > 0; warningSignal--) {
    checkLoop();
    if (state == SequenceState::IDLE) {
      warningRunning = false;
      return;
    }
    shortHorn();
  }
  warningRunning = false;
  wait(pause);
  start = (long)millis();
  timerRunning = true;
  // can go negative, we just don't want it via overflow
  updateCurrent();
  // loops until current is neg (three mins have passed) or the state gets changed by a button press
  while (currentTime >= 0 && state == SequenceState::IN_SEQUENCE) {
    checkLoop();
    // timings also must be a long, additionally this makes sure that i stays within the bound of `timings`
    if (currentTime <= (long)timings[i][0] * 1000L && i < timingsLength) {
      soundHorn(timings[i][1], timings[i][2]);
      i++;
    }
  }
  timerRunning = false;
  if (state == SequenceState::IN_SEQUENCE) {
    soundHorn(1, 0);
    state = SequenceState::POST_SEQUENCE;
    Serial.println("in post sequence");
  } else {
    Serial.println("cancelled sequence");
  }
}

void startRecall() {
  Serial.println("recall sequence");
  recallRunning = true;
  soundHorn(0, 6);
  recallRunning = false;
  // TODO: implement :P
}

// color is a three bit number, rgb
// currently unused since there are no more GPIO pins :(
// void setColor(int color) {
//   digitalWrite(red_pin, (color >> 2) & 1);
//   digitalWrite(green_pin, (color >> 1) & 1);
//   digitalWrite(blue_pin, color & 1);
// }

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
  return !topPole && !bottomPole ? Duration::THREE : topPole ? Duration::FIVE
                                                             : Duration::ONE;
}
