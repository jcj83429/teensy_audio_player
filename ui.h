#include "directories.h"
#include "common.h"

#define PIN_KEY_PREV 37
#define PIN_KEY_RWD  38
#define PIN_KEY_PLAY 39
#define PIN_KEY_FF   27
#define PIN_KEY_NEXT 28
#define PIN_KEY_FN1 26

#define USE_VFD 1
#define USE_OLED 0

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
void initKeyStates();
void printChar(char c, int x, int y, bool highlight);
void printStr(const char *s, int maxLen, int x, int y, bool highlight);
void printNum(int n, int digits, int x, int y);
void printTime(int timesec, int x, int y);
void uiInit();
void uiWriteFb();
void uiUpdate();
void displayError(const char *line0, const char *line1, unsigned long duration);
void clearError();

class UiModeBase {
public:
  virtual void enter() {};
  // process key events, update framebuffer, and return next UiMode
  virtual UiMode update(bool redraw) = 0;
};

class UiModeMain : public UiModeBase {
public:
  UiMode update(bool redraw);

#if USE_MRFFT
  float fftBin1x(int x);
  float fftBin4x(int x);
  float fftBin16x(int x);
#endif
};

class UiModeFiles : public UiModeBase {
public:
  void enter();
  UiMode update(bool redraw);

  void draw();

  DirectoryNavigator filesModeDirNav;
  int highlightedIdx;
  unsigned int highlightedFnOffset;
  unsigned long lastUpdateTime;
};

#if USE_F32
class UiModeVolume : public UiModeBase {
public:
  void enter() {
    selectedSetting = 0;
  }
  UiMode update(bool redraw);

  int selectedSetting;
  static const int numSettings = 3;
};
#endif
