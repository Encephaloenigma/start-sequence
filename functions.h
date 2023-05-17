enum SequenceState {
  IDLE,
  IN_SEQUENCE,
  POST_SEQUENCE,
};

enum Duration {
  THREE,
  ONE
};

void startSequence(int);
void soundLong(); 
void soundShort();
void soundHorn(int, int);