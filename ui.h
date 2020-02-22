#include "directories.h"
#include "common.h"

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
  KEY_EV_NONE = 0,
  KEY_EV_DOWN = 1,
  KEY_EV_UP = 2,
  KEY_EV_REPEAT = 4,
} KeyEvent;

typedef enum UiMode {
  UI_MODE_INVALID,
  UI_MODE_MAIN,
  UI_MODE_FILES,
#if USE_F32
  UI_MODE_VOLUME,
#endif
} UiMode;

struct KeyInfo {
  int pin;
  int event;
  bool lastState;
  unsigned long lastChangeTime;
  unsigned long lastEventTime;
};

extern struct KeyInfo keys[];

void updateKeyStates();
void printChar(char c, int x, int y, bool highlight);
void printStr(const char *s, int maxLen, int x, int y, bool highlight);
void printNum(int n, int digits, int x, int y);
void printTime(int timesec, int x, int y);
void uiWriteFb();
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
  UiModeFiles();
  ~UiModeFiles();
  UiMode update();

  void draw();

  DirectoryNavigator filesModeDirNav;
  int highlightedIdx;
  unsigned int highlightedFnOffset;
  unsigned long lastUpdateTime;
};

#if USE_F32
class UiModeVolume : public UiModeBase {
  UiMode update();
};
#endif
