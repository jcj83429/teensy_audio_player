// code for Noritake GU128x64-800B VFD

#include <stdint.h>
// teensy 3.6
// SS and CMD/DATA are controlled by HW SPI CS
#define PIN_VFD_SS 10
#if defined(KINETISK) // Teensy 3.6
#define PIN_VFD_CMD_DATA 6 // one of the HW CS pins
#else
#define PIN_VFD_CMD_DATA 12 // MISO, or DATA[1] when the SPI is running in 2 bit mode
#endif
#define PIN_VFD_RST 14
#define PIN_VFD_FRP 38

#if defined(KINETISK) // Teensy 3.6
#define USE_HW_CS 1
#define USE_SPI_DMA 1
#else
// HW CS is not supported on T4.1. The SPI hardware can't toggle multiple CS pins at once
#define USE_HW_CS 1
#define USE_SPI_DMA 1
#endif 

void vfdSend(uint8_t value, bool isCommand);
void vfdSetGram(bool isGram1);
void vfdInit();
void vfdSetCursor(int x, int y);
void vfdSetAutoInc(bool dx, bool dy);
// each byte in the fb is a 1x8 column of pixels. the fb is organized in rows of these columns of pixels.
void vfdWriteFb(uint8_t *fb, bool isGram1);
