#include <SPI.h>
#include "vfd.h"

#if defined(KINETISK)

#if USE_HW_CS

#if USE_SPI_DMA
#define FB_FULL_WRITE_CMDS (8 * (128 + 4))
uint32_t vfdcmdbuf[FB_FULL_WRITE_CMDS];
DMAChannel *vfdSpiDma;;
#endif

uint32_t spiCmd(uint8_t value, bool isCommand){
  // teensy: use hardware CS to control SS and CMD/DATA
  // For command, pull the SS down. For data, pull the SS and CMD/DATA down.
  uint8_t cs_pins = isCommand ? 1 : 3;
  return value | SPI_PUSHR_PCS(cs_pins);
}
#endif // HW CS

#endif // KINETISK

void vfdSend(uint8_t value, bool isCommand) {
#if defined(KINETISK) && USE_HW_CS
  SPI0_PUSHR = spiCmd(value, isCommand);
  while (!(SPI0_SR & SPI_SR_TCF));
  SPI0_SR = SPI_SR_TCF;
#elif defined(__IMXRT1062__) && USE_HW_CS
  digitalWriteFast(PIN_VFD_CMD_DATA, isCommand);
  SPI.transfer(value);
  digitalWriteFast(PIN_VFD_CMD_DATA, HIGH);
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
#if defined(KINETISK) && USE_HW_CS && USE_SPI_DMA
  vfdSpiDma = new DMAChannel();
#endif

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

void vfdWriteFb(uint8_t *fb, bool isGram1) {
  uint8_t baseY = isGram1 ? 8 : 0;
#if (defined(KINETISK) && USE_HW_CS && USE_SPI_DMA)
  int cmdIdx = 0;
  for(int i=0; i<8; i++){
    int y = baseY + i;
    vfdcmdbuf[cmdIdx++] = spiCmd(0x64, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(0, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(0x60, true);
    vfdcmdbuf[cmdIdx++] = spiCmd(y, true);
    for(int j = 128 * i; j < 128 * (i + 1); j++){
      vfdcmdbuf[cmdIdx++] = spiCmd(fb[j], false);
    }
  }
  /*
  cmdIdx = 0;
  for(int i=0; i<FB_FULL_WRITE_CMDS; i++){
    SPI0_PUSHR = vfdcmdbuf[i];
    while (!(SPI0_SR & SPI_SR_TCF));
    SPI0_SR = SPI_SR_TCF;
  }
  */
  SPI0_RSER = SPI_RSER_TFFF_RE | SPI_RSER_TFFF_DIRS;
  vfdSpiDma->disable();
  vfdSpiDma->destination(SPI0_PUSHR);
  vfdSpiDma->disableOnCompletion();
  vfdSpiDma->triggerAtHardwareEvent(DMAMUX_SOURCE_SPI0_TX);
  vfdSpiDma->sourceBuffer(vfdcmdbuf, FB_FULL_WRITE_CMDS * 4);
  unsigned long startTime = millis();
  vfdSpiDma->enable();
  while(!vfdSpiDma->complete()){
    if(millis() - startTime > 1000){
      Serial.println("SPI DMA stuck");
      DUMPVAL(SPI0_SR);
      startTime += 1000;
    }
  }
  SPI0_SR = 0xFF0F0000;
  SPI0_RSER = 0;

#elif (defined(__IMXRT1062__) && USE_HW_CS && USE_SPI_DMA)
  arm_dcache_flush_delete(fb, 128 * 8);
  // Due to the limited HW CS capabilities, 8 separate SPI bulk transfers have to be used
  // use a dummy buffer for receive so the framebuffer doesn't get destroyed
  uint8_t receiveBuf[128];
  for(int i=0; i<8; i++){
    vfdSetCursor(0, baseY + i);
    digitalWriteFast(PIN_VFD_CMD_DATA, LOW);
    SPI.transfer(fb + 128 * i, receiveBuf, 128);
  }

#else
  for(int i=0; i<8; i++){
    vfdSetCursor(0, baseY + i);
    for(int j = 128 * i; j < 128 * (i + 1); j++){
      vfdSend(fb[j], false);
    }
  }
#endif
}
