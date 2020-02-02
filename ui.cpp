#include "ui.h"
#include "vfd.h"
#include "font.h"
#include "player.h"
#include <Arduino.h>

#define DEBOUNCE_TIME 50

UiModeBase *currentUiMode = new UiModeMain();

struct KeyInfo keys[] = {
  [KEY_PREV] = {
    .pin = PIN_KEY_PREV,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_RWD] = {
    .pin = PIN_KEY_RWD,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_PLAY] = {
    .pin = PIN_KEY_PLAY,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_FF] = {
    .pin = PIN_KEY_FF,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_NEXT] = {
    .pin = PIN_KEY_NEXT,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_FN1] = {
    .pin = PIN_KEY_FN1,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
};

void updateKeyStates(){
  unsigned long now = millis();
  for(int keyId = 0; keyId < NUM_KEYS; keyId++){
    bool newState = digitalRead(keys[keyId].pin);

    if(newState != keys[keyId].lastState){
      keys[keyId].lastState = newState;
      keys[keyId].lastChangeTime = now;
      keys[keyId].event = KEY_EV_NONE;
      continue;
    }

    if(keys[keyId].lastChangeTime - now > DEBOUNCE_TIME && keys[keyId].lastEventTime < keys[keyId].lastChangeTime){
      keys[keyId].event = newState ? KEY_EV_UP : KEY_EV_DOWN;
      keys[keyId].lastEventTime = now;
      Serial.print("KEY ");
      Serial.print(keyId);
      Serial.print(" EVENT ");
      Serial.println(keys[keyId].event);
    }else{
      keys[keyId].event = KEY_EV_NONE;
    }
  }
}

void printChar(char c, int x, int y){
  for(int i=0; i<6; i++){
    framebuffer[y][x + i] = ms_6x8_font[(int)c][i];
  }
}

void printNum(int n, int digits, int x, int y){
  x += (digits - 1) * 6;
  for(int i = 0; i < digits; i++){
    char c = '0' + n % 10;
    printChar(c, x, y);
    x -= 6;
    n /= 10;
  }
}

void printTime(int timesec, int x, int y){
  if (timesec > 5999) {
    // can only display up to 99:99
    timesec = 5999;
  }
  int mins = timesec / 60;
  int secs = timesec % 60;

  printNum(mins, 2, x, y);
  printChar(':', x + 12, y);
  printNum(secs, 2, x + 18, y);
}

void uiUpdate(){
  updateKeyStates();
  UiMode newMode = currentUiMode->update();
  if(newMode != UI_MODE_INVALID){
    Serial.print("change UI mode to ");
    Serial.println(newMode);
    delete currentUiMode;
    switch(newMode){
    case UI_MODE_MAIN:
      currentUiMode = new UiModeMain();
      break;
    case UI_MODE_FILES:
      currentUiMode = new UiModeFiles();
      break;
    default:
      break;
      currentUiMode = new UiModeMain();
    }
  }
}

UiMode UiModeMain::update(){
  if(keys[KEY_PLAY].event == KEY_EV_DOWN){
    togglePause();
    goto keysdone;
  }
  if(keys[KEY_PREV].event == KEY_EV_DOWN){
    playPrev();
    goto keysdone;
  }
  if(keys[KEY_NEXT].event == KEY_EV_DOWN){
    playNext();
    goto keysdone;
  }
  if(keys[KEY_RWD].event == KEY_EV_DOWN){
    seekRelative(-5);
    goto keysdone;
  }
  if(keys[KEY_FF].event == KEY_EV_DOWN){
    seekRelative(+5);
    goto keysdone;
  }
  if(keys[KEY_FN1].event == KEY_EV_DOWN){
    return UI_MODE_FILES;
  }
keysdone:

  // row 0-3: spectrum
  for (int i = 0 ; i < 128; i++) {
    uint16_t fftBin = fft256_1.output[i];
    uint32_t fftBinLog = fftBin ? (32 - __builtin_clz((uint32_t)fftBin)) : 0;
    uint32_t lsb = 0;
    if (fftBinLog > 1) {
      // use the bit after the leading bit to add an approximate bit to the log
      lsb = (fftBin >> (fftBinLog - 2)) & 1;
    }
    fftBinLog = (fftBinLog << 1) | lsb;
    uint32_t fftBinLogMask = fftBinLog ? (0x1 << (32 - fftBinLog)) : 0;
    framebuffer[0][i] = fftBinLogMask;
    framebuffer[1][i] = fftBinLogMask >> 8;
    framebuffer[2][i] = fftBinLogMask >> 16;
    framebuffer[3][i] = fftBinLogMask >> 24;
  }

  // clear row 4-7
  for (int i = 0; i < 128; i++) {
    for (int j = 4; j < 8; j++) {
      framebuffer[j][i] = 0;
    }
  }

  // row 4-6: file name
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 21; i++) {
      char c = currentFileName[21*j + i];
      if(c == 0){
        goto filenameend;
      }
      printChar(c, i * 6, j + 4);
    }
  }
filenameend:

  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    printTime(playingCodec->lengthMillis() / 1000, 128 - 5 * 6, 7);
    printChar('/', 128 - 6 * 6, 7);
    printTime(playingCodec->positionMillis() / 1000, 128 - 11 * 6, 7);
  }

  return UI_MODE_INVALID;
}

UiMode UiModeFiles::update() {
  if(keys[KEY_FN1].event == KEY_EV_DOWN){
    return UI_MODE_MAIN;
  }
  return UI_MODE_INVALID;
}
