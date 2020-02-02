#define PIN_KEY_PLAY 25
#define PIN_KEY_PREV 24
#define PIN_KEY_NEXT 26

enum KeyId {
  KEY_PLAY,
  KEY_PREV,
  KEY_NEXT,
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
