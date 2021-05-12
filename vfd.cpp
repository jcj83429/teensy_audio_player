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
  // SS and CMD/DATA pin modes will be overwritten by CORE_PIN*_CONFIG below
  pinMode(PIN_VFD_SS, OUTPUT);
  pinMode(PIN_VFD_CMD_DATA, OUTPUT);
  pinMode(PIN_VFD_RST, OUTPUT);
  //pinMode(PIN_VFD_FRP, INPUT);
  digitalWrite(PIN_VFD_RST, LOW);
  delay(1);
  digitalWrite(PIN_VFD_RST, HIGH);

  SPI.begin();

  // set SPI mode once and never touch it again
  // The VFD supports up to 5MHz. Use half of it for safety
  SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE3));
  SPI.endTransaction();

#if defined(KINETISK) // Teensy 3.6
  DUMPVAL(SPI0_CTAR0);
  DUMPVAL(SPI0_PUSHR);
  SPI0_CTAR0 &= ~(SPI_CTAR_ASC(0xf));
  // set up SPI delay params to meet timing requirements of the VFD
  // the minimum delays required to meet requirements
  // SPI0_CTAR0 |= SPI_CTAR_CSSCK(1) | SPI_CTAR_PASC(1) | SPI_CTAR_ASC(1) | SPI_CTAR_PDT(1) | SPI_CTAR_DT(0);
  // double the delays for safety
  SPI0_CTAR0 |= SPI_CTAR_CSSCK(2) | SPI_CTAR_PASC(1) | SPI_CTAR_ASC(2) | SPI_CTAR_PDT(1) | SPI_CTAR_DT(1);
  DUMPVAL(SPI0_CTAR0);
  DUMPVAL(SPI0_PUSHR);

#if USE_HW_CS
  // The drive strength (DSE) and slew rate (SRE) settings are a hacky way to make C/D slower than SS
  // Otherwise I can't use HW SPI CS and SPI DMA.
  // The display has trouble understanding the signal when the SS and C/D switch at exactly the same time
  // PIN 10 = SS
  DUMPVAL(CORE_PIN10_CONFIG);
  CORE_PIN10_CONFIG = PORT_PCR_MUX(2) | PORT_PCR_DSE; // pin 10 = PTC4 = SPI0_PCS0
  DUMPVAL(CORE_PIN10_CONFIG);
  // PIN 6 = CMD/DATA
  DUMPVAL(CORE_PIN6_CONFIG);
  CORE_PIN6_CONFIG = PORT_PCR_MUX(2) | PORT_PCR_SRE; // pin 6 = PTD4 = SPI0_PCS1
  DUMPVAL(CORE_PIN6_CONFIG);
#endif

#else // Teensy 4.1

  DUMPVAL(LPSPI4_CFGR0);
  DUMPVAL(LPSPI4_CFGR1);
  DUMPVAL(LPSPI4_CCR);
  DUMPVAL(LPSPI4_TCR);

#if USE_HW_CS
  // The T4.1 SPI can only toggle one CS pin, so the HW CS is used for the SS pin and the CMD/DATA pin is toggled manually
  SPI.setCS(PIN_VFD_SS);

  int sckdiv = LPSPI_CCR_SCKDIV(LPSPI4_CCR);
  // set SCKPCS, PCSSCK and DBT to SCKDIV/2.
  // SCKDIV (SCK clock period) is at least 200ns by the SPI speed setting
  // PCSSCK needs to be at least 40ns
  // SCKPCS needs to be at least 150ns
  // DBT needs to be at least 80ns.
  // Some of these may already be set by the SPI library but it doesn't hurt to set them again.
  LPSPI4_CCR = LPSPI_CCR_SCKPCS(sckdiv*3/4) | LPSPI_CCR_PCSSCK(sckdiv/5) | LPSPI_CCR_DBT(sckdiv*2/5) | LPSPI_CCR_SCKDIV(sckdiv);
  DUMPVAL(LPSPI4_CCR);

  // set SPI to transfer 16 bits and use 2-bit mode. MISO becomes DATA[1] and it will carry the CMD/DATA.
  LPSPI4_TCR = (LPSPI4_TCR & ~LPSPI_TCR_FRAMESZ(0xfff)) | LPSPI_TCR_FRAMESZ(15) | LPSPI_TCR_RXMSK | LPSPI_TCR_WIDTH(1);
  DUMPVAL(LPSPI4_TCR);
#else
  // take pin 12 (MISO) back from the SPI module
  pinMode(PIN_VFD_CMD_DATA, OUTPUT);
#endif

#endif

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
