// Support for SSD1325 OLED screens

#include <SPI.h>
#include "oled.h"
#include "common.h"

#if USE_OLED

#if OLED_SPI_DMA
DMAChannel *oledSpiDma;

#define FB_FULL_WRITE_BYTES (8 * 8 * 64)
DMAMEM uint8_t oledcmdbuf[FB_FULL_WRITE_BYTES];
#endif

void oledSend(uint8_t value, bool isCommand){
  digitalWriteFast(OLED_DC, !isCommand);
  //digitalWriteFast(OLED_CS, LOW);
  //SPI.transfer(value);
  LPSPI4_TDR = value;
  while((LPSPI4_FSR & 0xff) || (LPSPI4_SR & LPSPI_SR_MBF));
  //digitalWriteFast(OLED_CS, HIGH);
  digitalWriteFast(OLED_DC, HIGH);
}

void oledInit(){
  SPI.begin();
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  //pinMode(OLED_CS, OUTPUT);
  SPI.setCS(OLED_CS);
  pinMode(OLED_DC, OUTPUT);
  pinMode(OLED_RESET, OUTPUT);
  // We never read anything from the receive FIFO, so enable NOSTALL to ignore receive FIFO full.
  LPSPI4_CFGR1 |= LPSPI_CFGR1_NOSTALL;

  digitalWrite(OLED_RESET, 0);
  delay(1);
  digitalWrite(OLED_RESET, 1);
  delay(10);
  // These parameters are mostly copied from the Adafruit SSD1325 library
  // Any display that works with that library will work
  oledSend(0xAE, true); // Display Off
  oledSend(0xB3, true); // Set Display Clock Divide Ratio
  oledSend(0xF1, true); //  max clock, min divider
  oledSend(0xA8, true); // Set Multiplex Ratio
  oledSend(0x3F, true); //  64
  oledSend(0xA2, true); // Set Display Offset
  oledSend(76, true);
  oledSend(0xA1, true); // Set Display Start Line
  oledSend(0, true);
  oledSend(0xAD, true); // Set Master Configuration
  oledSend(0x02, true);
  oledSend(0xA0, true); // Set Re-map
  oledSend(0x52, true);
  oledSend(0x86, true); // Set Current Range full
  oledSend(0x81, true); // Set Contrast Current
  oledSend(0x7F, true);
  oledSend(0xA4, true); // Set Display Mode normal
  oledSend(0xAF, true); // Set Display Mode on

#if OLED_SPI_DMA
  oledSpiDma = new DMAChannel();
#endif
}

void oledWriteFb(uint8_t *fb) {
  oledSend(0x15, true); // Set Column Address
  oledSend(0, true);
  oledSend(63, true);
  oledSend(0x75, true); // Set Row Address
  oledSend(0, true);
  oledSend(63, true);

#if OLED_SPI_DMA
  int cmdIdx = 0;
#endif

  for(int i = 0; i < 8; i++){
    for(int j = 0; j < 8; j++){
      uint8_t mask = 1 << j;
      uint8_t *fbptr = fb + 128 * i;
      for(int k = 0; k < 64; k++){
        uint8_t pixpair = 0;
        if(*fbptr++ & mask){
          pixpair |= 0xF0;
        }
        if(*fbptr++ & mask){
          pixpair |= 0x0F;
        }
#if OLED_SPI_DMA
        oledcmdbuf[cmdIdx++] = pixpair;
#else
        oledSend(pixpair, false);
#endif
      }
    }
  }

#if OLED_SPI_DMA
  arm_dcache_flush_delete(oledcmdbuf, sizeof(oledcmdbuf));
  LPSPI4_DER = LPSPI_DER_TDDE; // enable transmit DMA request
  oledSpiDma->disable();
  oledSpiDma->destination((volatile uint8_t&)LPSPI4_TDR);
  oledSpiDma->disableOnCompletion();
  oledSpiDma->triggerAtHardwareEvent(DMAMUX_SOURCE_LPSPI4_TX);
  oledSpiDma->sourceBuffer(oledcmdbuf, sizeof(oledcmdbuf));
  oledSpiDma->enable();
  unsigned long startTime = millis();

  while(!oledSpiDma->complete()){
    if(millis() - startTime > 1000){
      Serial.println("SPI DMA stuck");
      DUMPVAL(LPSPI4_SR);
      DUMPVAL(LPSPI4_TCR);
      DUMPVAL(LPSPI4_DER);
      DUMPVAL(LPSPI4_FSR);
      startTime += 1000;
    }
  }
  oledSpiDma->clearComplete();
  oledSpiDma->disable();
#endif
}

#endif // USE_OLED
