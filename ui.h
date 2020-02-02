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

typedef enum UiMode {
  UI_MODE_INVALID,
  UI_MODE_MAIN,
  UI_MODE_FILES,
} UiMode;

struct KeyInfo {
  int pin;
  KeyEvent event;
  bool lastState;
  unsigned long lastChangeTime;
  unsigned long lastEventTime;
};

extern struct KeyInfo keys[];

void updateKeyStates();
void printChar(char c, int x, int y);
void printNum(int n, int digits, int x, int y);
void printTime(int timesec, int x, int y);
void uiUpdate();

class UiModeBase {
public:
  virtual ~UiModeBase() {};
  // process key events, update framebuffer, and return next UiMode
  virtual UiMode update() = 0;
};

class UiModeMain : public UiModeBase {
public:
  UiMode update();
};

class UiModeFiles : public UiModeBase {
public:
  UiMode update();
};
