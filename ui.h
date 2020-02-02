#define PIN_KEY_PREV 24
#define PIN_KEY_RWD  25
#define PIN_KEY_PLAY 26
#define PIN_KEY_FF   27
#define PIN_KEY_NEXT 7
#define PIN_KEY_FN1 8

enum KeyId {
  KEY_PREV,
  KEY_RWD,
  KEY_PLAY,
  KEY_FF,
  KEY_NEXT,
  KEY_FN1,
  NUM_KEYS,
};

typedef enum KeyEvent {
  KEY_EV_NONE,
  KEY_EV_DOWN,
  KEY_EV_UP,
} KeyEvent;

struct KeyInfo {
  int pin;
  KeyEvent event;
  bool lastState;
  unsigned long lastChangeTime;
  unsigned long lastEventTime;
};

extern struct KeyInfo keys[];

void updateKeyStates();
