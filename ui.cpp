#include "ui.h"
#include "vfd.h"
#include "font.h"
#include "player.h"
#include <Arduino.h>

#define DEBOUNCE_TIME 50
#define KEY_REPEAT_TIME 100
#define KEY_REPEAT_DELAY 250

UiModeMain uiModeMain;
UiModeFiles uiModeFiles;
#if USE_F32
UiModeVolume uiModeVolume;
#endif

UiModeBase *currentUiMode = &uiModeMain;

#if defined(__IMXRT1062__) && USE_SPI_DMA
// For T4.1, the DMA is directly from the framebuffer so it needs to be in DMAMEM
DMAMEM uint8_t framebuffer[8][128];
#else
uint8_t framebuffer[8][128];
#endif

char errorMsg[2][20] = {0};
unsigned long errorMsgEndTime = 0;

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
    bool newState = !digitalRead(keys[keyId].pin);

    if(newState != keys[keyId].lastState){
      keys[keyId].lastState = newState;
      keys[keyId].lastChangeTime = now;
      keys[keyId].event = KEY_EV_NONE;
      continue;
    }

    if(now - keys[keyId].lastChangeTime > DEBOUNCE_TIME) {
      if(keys[keyId].lastEventTime < keys[keyId].lastChangeTime) {
        keys[keyId].event = newState ? KEY_EV_UP : KEY_EV_DOWN;
        keys[keyId].lastEventTime = now;
      } else if(!newState && now - keys[keyId].lastChangeTime > KEY_REPEAT_DELAY && now - keys[keyId].lastEventTime > KEY_REPEAT_TIME) {
        keys[keyId].event = KEY_EV_DOWN | KEY_EV_REPEAT;
        keys[keyId].lastEventTime = now;
      } else {
        keys[keyId].event = KEY_EV_NONE;
      }
//      if(keys[keyId].event != KEY_EV_NONE){
//        Serial.print("KEY ");
//        Serial.print(keyId);
//        Serial.print(" EVENT ");
//        Serial.println(keys[keyId].event);
//      }
    }else{
      keys[keyId].event = KEY_EV_NONE;
    }
  }
}

void initKeyStates(){
  updateKeyStates();
  for(int i=0; i<NUM_KEYS; i++){
    keys[i].lastEventTime = keys[i].lastChangeTime;
  }
}

void printChar(char c, int x, int y, bool highlight){
  uint8_t xorVal = highlight ? 0xff : 0;
  for(int i=0; i<6; i++){
    if(x >= 0 && x < 128){
      framebuffer[y][x] = xorVal ^ ms_6x8_font[(int)c][i];
    }
    x++;
  }
}

void printStr(const char *s, int maxLen, int x, int y, bool highlight){
  while(*s && maxLen){
    printChar(*s, x, y, highlight);
    x += 6;
    s++;
    maxLen--;
  }
}

void printNum(int n, int digits, int x, int y){
  x += (digits - 1) * 6;
  for(int i = 0; i < digits; i++){
    char c = '0' + n % 10;
    printChar(c, x, y, false);
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
  printChar(':', x + 12, y, false);
  printNum(secs, 2, x + 18, y);
}

void uiInit(){
#if defined(__IMXRT1062__) && USE_HW_CS && USE_SPI_DMA
  memset(framebuffer, 0, sizeof(framebuffer));
#endif
}

void uiWriteFb(){
  vfdWriteFb(&framebuffer[0][0], 0);
}

void renderError(){
  for(int i = 0; i < 2; i++){
    printStr(errorMsg[i], 20, 0, i, true);
  }
}

void uiUpdate(){
  updateKeyStates();

  if(errorMsgEndTime){
    for(int i = 0; i < NUM_KEYS; i++){
      if(keys[i].event & KEY_EV_DOWN){
        Serial.print("key ");
        Serial.print(i);
        Serial.print(" event ");
        Serial.print(keys[i].event);
        Serial.println(" clear error");
        clearError();
        break;
      }
    }
  }

  bool forceRedraw = false;
  if(errorMsgEndTime - millis() >= 0x80000000){
    // error msg expired
    errorMsgEndTime = 0;
    forceRedraw = true;
  }

  UiMode newMode = currentUiMode->update(forceRedraw);

  if(newMode != UI_MODE_INVALID){
    Serial.print("change UI mode to ");
    Serial.println(newMode);

    switch(newMode){
    case UI_MODE_MAIN:
      currentUiMode = &uiModeMain;
      break;
    case UI_MODE_FILES:
      currentUiMode = &uiModeFiles;
      break;
#if USE_F32
    case UI_MODE_VOLUME:
      currentUiMode = &uiModeVolume;
      break;
#endif
    default:
      break;
      currentUiMode = &uiModeMain;
    }

    currentUiMode->enter();
  }

  if(errorMsgEndTime){
    renderError();
  }
}

void displayError(const char *line0, const char*line1, unsigned long duration){
  strncpy(errorMsg[0], line0, 20);
  strncpy(errorMsg[1], line1, 20);
  errorMsgEndTime = millis() + duration;
  if(!errorMsgEndTime){
    errorMsgEndTime = 1;
  }
  // immediately write to screen
  renderError();
  uiWriteFb();
}

void clearError(){
  // set it to now instead of 0 so uiUpdate() can treat it as an expiry and force redraw
  errorMsgEndTime = millis();
}

UiMode UiModeMain::update(bool redraw){
  if(keys[KEY_PLAY].event == KEY_EV_DOWN){
    togglePause();
    goto keysdone;
  }
  if(keys[KEY_PREV].event & KEY_EV_DOWN){
    playPrev();
    goto keysdone;
  }
  if(keys[KEY_NEXT].event & KEY_EV_DOWN){
    playNext();
    goto keysdone;
  }
  if(keys[KEY_RWD].event & KEY_EV_DOWN){
    seekRelative(-5);
    goto keysdone;
  }
  if(keys[KEY_FF].event & KEY_EV_DOWN){
    seekRelative(+5);
    goto keysdone;
  }
  if(keys[KEY_FN1].event == KEY_EV_DOWN){
    return UI_MODE_FILES;
  }
keysdone:

  union {
    float32_t f;
    uint32_t u;
  } fu;

  // row 0-3: spectrum
  for (int i = 0 ; i < 128; i++) {
#if !USE_F32
    uint16_t fftBin = fft256.output[i];
    int fftBinLog = fftBin ? (32 - __builtin_clz((uint32_t)fftBin)) : 0;
    int lsb = 0;
    if (fftBinLog > 1) {
      // use the bit after the leading bit to add an approximate bit to the log
      lsb = (fftBin >> (fftBinLog - 2)) & 1;
    }
    fftBinLog = (fftBinLog << 1) | lsb;
#else
    float32_t fftBin = fft256.output[i];
    fu.f = fftBin * fftBin; // 2 * log(x) = log(x^2);
    int fftBinLog = (fu.u >> 23) & 0xff; // just get exponent
    fftBinLog -= 96;
    fftBinLog = fftBinLog > 32 ? 32 : fftBinLog < 0 ? 0 : fftBinLog;
#endif
    uint32_t fftBinLogMask = fftBinLog ? (0x1 << (32 - fftBinLog)) : 0;
    framebuffer[0][i] = fftBinLogMask;
    framebuffer[1][i] = fftBinLogMask >> 8;
    framebuffer[2][i] = fftBinLogMask >> 16;
    framebuffer[3][i] = fftBinLogMask >> 24;
  }

  // row 0: overlay peak
  float peakL = (analyzePeak1.read() * 2) & 0xffff; //0 to 2^16
  peakL /= 256; // limit peak meter to 8 bits of dynamic range. below that it's mostly noise and DC offset anyway.
  peakL = peakL * peakL;
  peakL = peakL * peakL;
  peakL = peakL * peakL;
  peakL = peakL * peakL; // 0 to 2^128
  float peakR = (analyzePeak2.read() * 2) & 0xffff; //0 to 2^16
  peakR /= 256;
  peakR = peakR * peakR;
  peakR = peakR * peakR;
  peakR = peakR * peakR;
  peakR = peakR * peakR; // 0 to 2^128

  int peakLLog, peakRLog;
  if(peakL == 0){
    peakLLog = -1;
  }else{
    fu.f = peakL;
    peakLLog = ((fu.u >> 23) & 0xff) - 127;
    peakLLog = min(127, peakLLog);
  }
  if(peakR == 0){
    peakRLog = -1;
  }else{
    fu.f = peakR;
    peakRLog = ((fu.u >> 23) & 0xff) - 127;
    peakRLog = min(127, peakRLog);
  }

  if(peakLLog >= 0){
    framebuffer[0][peakLLog] |= 0x01;
  }
  if(peakRLog >= 0){
    framebuffer[0][peakRLog] |= 0x80;
  }

  // row 0: overlay rms
  float rmsL = analyzePeak1.readMeanSq() * 4 / 65536; // 0 to 2^16
  rmsL = rmsL * rmsL;
  rmsL = rmsL * rmsL;
  rmsL = rmsL * rmsL;
  fu.f = rmsL;
  int rmsLLog = ((fu.u >> 23) & 0xff) - 127;
  rmsLLog = min(127, rmsLLog);
  if(rmsLLog >= 0){
    framebuffer[0][rmsLLog] |= 0x06;
    if(rmsLLog > 0){
      framebuffer[0][rmsLLog-1] |= 0x06;
    }
  }

  float rmsR = analyzePeak2.readMeanSq() * 4 / 65536; // 0 to 2^16
  rmsR = rmsR * rmsR;
  rmsR = rmsR * rmsR;
  rmsR = rmsR * rmsR;
  fu.f = rmsR;
  int rmsRLog = ((fu.u >> 23) & 0xff) - 127;
  rmsRLog = min(127, rmsRLog);
  if(rmsRLog >= 0){
    framebuffer[0][rmsRLog] |= 0x60;
    if(rmsRLog > 0){
      framebuffer[0][rmsRLog-1] |= 0x60;
    }
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
      printChar(c, i * 6, j + 4, false);
    }
  }
filenameend:

  uint32_t posMs = positionMs();
  uint32_t lenMs = lengthMs();
  if(lenMs && posMs <= lenMs){
    int timePos = posMs * (128 - 11 * 6) / lenMs;
    printTime(posMs / 1000, timePos, 7);
    printChar('/', timePos + 30, 7, false);
    printTime(lenMs / 1000, timePos + 36, 7);
  }

  return UI_MODE_INVALID;
}

void UiModeFiles::enter(){
  // all files are opened read only, so just copy the whole dirNav
  filesModeDirNav = dirNav;
  highlightedIdx = filesModeDirNav.lastSelectedItem;
  highlightedFnOffset = 0;
  lastUpdateTime = millis();
  draw();
}

UiMode UiModeFiles::update(bool redraw) {
  bool updated = false;
  unsigned long now = millis();
  // FN1 key: go back to main mode
  if(keys[KEY_FN1].event == KEY_EV_DOWN){
#if USE_F32
    return UI_MODE_VOLUME;
#else
    return UI_MODE_MAIN;
#endif
  }
  // NEXT key: go into dir or play file
  if(keys[KEY_NEXT].event & KEY_EV_DOWN){
    FsFile selectedFile = filesModeDirNav.selectItem(highlightedIdx);
    if(selectedFile.isOpen()){
      stop();
      currentFile = selectedFile;
      playFile(&currentFile);
      // write directory stack etc. back
      dirNav = filesModeDirNav;
      return UI_MODE_MAIN;
    }else{
      highlightedIdx = 0;
      updated = true;
    }
  }else if(keys[KEY_PREV].event & KEY_EV_DOWN){
    if(filesModeDirNav.upDir()){
      highlightedIdx = filesModeDirNav.lastSelectedItem;
      updated = true;
    }
  }else if(keys[KEY_FF].event & KEY_EV_DOWN){
    highlightedIdx = (highlightedIdx + 1) % filesModeDirNav.curDirFiles();
    updated = true;
  }else if(keys[KEY_RWD].event & KEY_EV_DOWN){
    highlightedIdx = (highlightedIdx + filesModeDirNav.curDirFiles() - 1) % filesModeDirNav.curDirFiles();
    updated = true;
  }

  if(updated){
    highlightedFnOffset = 0;
  }else{
    if(highlightedIdx < filesModeDirNav.curDirFiles() &&
       ((highlightedFnOffset > 0 && now - lastUpdateTime > 20) || now - lastUpdateTime > 500)){
      int fnlen = strlen(filesModeDirNav.curDirFileName(highlightedIdx));
      if(fnlen > 21){
        highlightedFnOffset += 1;
        if(highlightedFnOffset >= strlen(filesModeDirNav.curDirFileName(highlightedIdx)) * 6){
          highlightedFnOffset = 0;
        }
        updated = true;
      }else{
        lastUpdateTime += 60000; // file name doesn't need scrolling
      }
    }
  }

  if(updated){
    draw();
    lastUpdateTime = now;
  }else if(redraw){
    draw();
  }
  return UI_MODE_INVALID;
}

void UiModeFiles::draw() {
  memset(framebuffer, 0, sizeof(framebuffer));
  int fileIdx = highlightedIdx & ~7;
  for(int i=0; i<8; i++){
    if(fileIdx >= filesModeDirNav.curDirFiles()){
      break;
    }
    char *fn = filesModeDirNav.curDirFileName(fileIdx);
    bool highlight = false;
    int x = 0;
    if(fileIdx == highlightedIdx){
      highlight = true;
      fn += highlightedFnOffset / 6;
      x = -(highlightedFnOffset % 6);
    }else{
      x = 0;
    }
    printStr(fn, 22, x, i, highlight);
    fileIdx++;
  }
}

#if USE_F32
UiMode UiModeVolume::update(bool redraw) {
  if(keys[KEY_FN1].event == KEY_EV_DOWN){
    return UI_MODE_MAIN;
  }
  if(keys[KEY_NEXT].event & KEY_EV_DOWN){
    switch(selectedSetting){
      case 0: {
        float newGain = currentGain + 1;
        newGain = min(newGain, 0);
        setGain(newGain);
        break;
      }
      case 1:
        setReplayGainMode((ReplayGainMode)((replayGainMode + 1) % REPLAY_GAIN_MODES));
        break;
      case 2: {
        float newFbGain = rgFallbackGain + 1;
        newFbGain = min(newFbGain, 0);
        setReplayGainFallbackGain(newFbGain);
        break;
      }
      default:
        break;
    }
    goto keysdone;
  }
  if(keys[KEY_PREV].event & KEY_EV_DOWN){
    switch(selectedSetting){
      case 0: {
        float newGain = currentGain - 1;
        newGain = max(newGain, -60);
        setGain(newGain);
        break;
      }
      case 1:
        setReplayGainMode((ReplayGainMode)((replayGainMode - 1 + REPLAY_GAIN_MODES) % REPLAY_GAIN_MODES));
        break;
      case 2: {
        float newFbGain = rgFallbackGain - 1;
        newFbGain = max(newFbGain, -60);
        setReplayGainFallbackGain(newFbGain);
        break;
      }
      default:
        break;
    }
    goto keysdone;
  }
  if(keys[KEY_FF].event == KEY_EV_DOWN){
    selectedSetting = (selectedSetting + 1) % numSettings;
    goto keysdone;
  }
  if(keys[KEY_RWD].event == KEY_EV_DOWN){
    selectedSetting = (selectedSetting - 1 + numSettings) % numSettings;
    goto keysdone;
  }
keysdone:

  memset(framebuffer, 0, sizeof(framebuffer));
  printStr("Volume", 21, 0, 0, false);

  char strbuf[10] = {0};
  snprintf(strbuf, 10, "%3.2ddB", (int)currentGain);
  printStr(strbuf, 5, 128-30, 0, false);

  printStr("ReplayGain", 21, 0, 2, false);
  switch(replayGainMode){
    case REPLAY_GAIN_OFF:
      printStr("Off", 3, 128-18, 2, false);
      break;
    case REPLAY_GAIN_TRACK:
      printStr("Track", 5, 128-30, 2, false);
      break;
    case REPLAY_GAIN_ALBUM:
      printStr("Album", 5, 128-30, 2, false);
      break;
    case REPLAY_GAIN_AVERAGE:
      printStr("Average", 7, 128-42, 2, false);
      break;
    default:
      break;
  }

  printStr("RG Fallback", 21, 12, 3, false);
  snprintf(strbuf, 10, "%3.2ddB", (int)rgFallbackGain);
  printStr(strbuf, 5, 128-30, 3, false);

  printStr("Effective RG", 21, 12, 4, false);
  snprintf(strbuf, 10, "%+5.1fdB", effectiveReplayGain());
  printStr(strbuf, 7, 128 - 42, 4, false);

  int highlightRow;
  switch(selectedSetting){
    case 0:
    default:
      highlightRow = 0;
      break;
    case 1:
      highlightRow = 2;
      break;
    case 2:
      highlightRow = 3;
      break;
  }
  for(int i = 0; i < 128; i++){
    framebuffer[highlightRow][i] ^= 0xff;
  }

  return UI_MODE_INVALID;
}
#endif
