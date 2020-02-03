#include <SPI.h>
#include "vfd.h"

uint8_t framebuffer[8][128];

#if USE_HW_CS
#define FB_FULL_WRITE_CMDS (8 * (128 + 4))
uint32_t vfdcmdbuf[FB_FULL_WRITE_CMDS];

uint32_t spiCmd(uint8_t value, bool isCommand){
  // teensy: use hardware CS to control SS and CMD/DATA
  // For command, pull the SS down. For data, pull the SS and CMD/DATA down.
  uint8_t cs_pins = isCommand ? 1 : 3;
  return value | SPI_PUSHR_PCS(cs_pins);
}
#endif

void vfdSend(uint8_t value, bool isCommand) {
#if USE_HW_CS
  SPI0_PUSHR = spiCmd(value, isCommand);
  while (!(SPI0_SR & SPI_SR_TCF));
  SPI0_SR = SPI_SR_TCF;
#else
  digitalWriteFast(PIN_VFD_CMD_DATA, isCommand);
  digitalWriteFast(PIN_VFD_SS, LOW);
  SPI.transfer(value);
  digitalWriteFast(PIN_VFD_SS, HIGH);
  digitalWriteFast(PIN_VFD_CMD_DATA, HIGH);
#endif
}

// turn on display and set which GRAM to display
void vfdSetGram(bool isGram1) {
  vfdSend(0x20 | (4 << isGram1), true);
  vfdSend(0x40, true);
}

void vfdInit() {
  // clear
  vfdSend(0x5f, true);
  delay(1); // datasheet says wait 1ms

  // display area set: all areas set to bitmap mode
  for (int i = 0; i < 8; i++) {
    vfdSend(0x62, true);
    vfdSend(i, true);
    vfdSend(0xff, false);
  }

  vfdSetGram(0);
}

void vfdSetCursor(int x, int y) {
  vfdSend(0x64, true);
  vfdSend(x, true);
  vfdSend(0x60, true);
  vfdSend(y, true);
}

void vfdSetAutoInc(bool dx, bool dy) {
  vfdSend(0x80 | (dx << 2) | (dy << 1), true);
}

void vfdWriteFb(bool isGram1) {
  uint8_t baseY = isGram1 ? 8 : 0;
#if USE_HW_CS
  int cmdIdx = 0;
  for(int i=0; i<8; i++){
    int y = baseY + i;
    vfdcmdbuf[cmdIdx++] = spiCmd(0x64, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(0, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(0x60, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(y, true);
    for(int j=0; j<128; j++){
      vfdcmdbuf[cmdIdx++] = spiCmd(framebuffer[i][j], false);
    }
  }
  cmdIdx = 0;
  for(int i=0; i<FB_FULL_WRITE_CMDS; i++){
    SPI0_PUSHR = vfdcmdbuf[i];
    while (!(SPI0_SR & SPI_SR_TCF));
    SPI0_SR = SPI_SR_TCF;
  }
#else
  for(int i=0; i<8; i++){
    vfdSetCursor(0, baseY + i);
    for(int j=0; j<128; j++){
      vfdSend(framebuffer[i][j], false);
    }
  }
#endif
}
