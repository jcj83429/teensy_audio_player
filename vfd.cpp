#include <SPI.h>
#include "vfd.h"

#if USE_HW_CS

#if USE_SPI_DMA

DMAChannel *vfdSpiDma;

#define FB_FULL_WRITE_CMDS (8 * (128 + 4))
#if defined(__IMXRT1062__)
DMAMEM uint16_t vfdcmdbuf[FB_FULL_WRITE_CMDS];
#else
uint32_t vfdcmdbuf[FB_FULL_WRITE_CMDS];
#endif
#endif // SPI DMA

uint32_t spiCmd(uint8_t value, bool isCommand){
#if defined(KINETISK)
  // teensy3.6: use hardware CS to control SS and CMD/DATA
  // For command, pull the SS down. For data, pull the SS and CMD/DATA down.
  uint8_t cs_pins = isCommand ? 1 : 3;
  return value | SPI_PUSHR_PCS(cs_pins);
#else
  // teensy4.1: use SPI in 2-bit mode and transmit CMD/DATA in the second bit
  uint16_t value2 = 0;
  uint16_t mask = 0x80;
  for(int i = 0; i < 8; i++){
    value2 <<= 1;
    value2 |= value & mask;
    mask >>= 1;
  }
  value2 |= 0xaaaa * isCommand;
  return value2;
#endif
}

#endif // HW CS

void vfdSend(uint8_t value, bool isCommand) {
#if defined(KINETISK) && USE_HW_CS
  SPI0_PUSHR = spiCmd(value, isCommand);
  while (!(SPI0_SR & SPI_SR_TCF));
  SPI0_SR = SPI_SR_TCF;
#elif defined(__IMXRT1062__) && USE_HW_CS
  LPSPI4_TDR = spiCmd(value, isCommand);
  while(LPSPI4_FSR);
  while(LPSPI4_SR & LPSPI_SR_MBF);
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
#if USE_HW_CS && USE_SPI_DMA
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
#if (USE_HW_CS && USE_SPI_DMA)
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

#if defined(KINETISK)
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

#elif defined(__IMXRT1062__)
  /*
  for(int i=0; i<FB_FULL_WRITE_CMDS; i++){
    LPSPI4_TDR = vfdcmdbuf[i];
    while(LPSPI4_FSR);
    while((LPSPI4_SR & LPSPI_SR_MBF));
  }
  */
  arm_dcache_flush_delete(vfdcmdbuf, sizeof(vfdcmdbuf));
  LPSPI4_DER = LPSPI_DER_TDDE; // enable transmit DMA request
  vfdSpiDma->disable();
  vfdSpiDma->destination((volatile uint16_t&)LPSPI4_TDR);
  vfdSpiDma->disableOnCompletion();
  vfdSpiDma->triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
  vfdSpiDma->sourceBuffer(vfdcmdbuf, sizeof(vfdcmdbuf));
  vfdSpiDma->enable();
  unsigned long startTime = millis();

  while(!vfdSpiDma->complete()){
    if(millis() - startTime > 1000){
      Serial.println("SPI DMA stuck");
      DUMPVAL(LPSPI4_SR);
      DUMPVAL(LPSPI4_TCR);
      DUMPVAL(LPSPI4_DER);
      DUMPVAL(LPSPI4_FSR);
      startTime += 1000;
    }
  }
  vfdSpiDma->clearComplete();
  vfdSpiDma->disable();

#endif // KINETISK or __IMXRT1062__

#else
  for(int i=0; i<8; i++){
    vfdSetCursor(0, baseY + i);
    for(int j = 128 * i; j < 128 * (i + 1); j++){
      vfdSend(fb[j], false);
    }
  }
#endif
}
