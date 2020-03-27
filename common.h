#ifndef COMMON_H
#define COMMON_H

#include <SdFat.h>

#define USE_F32 1
#define USE_I2S_SLAVE 0
#define DAC_MAX_SAMPLE_RATE 192000

// 2 bytes
#define EEPROM_OFFSET_PLAYTIME 0
// 4 bytes
#define EEPROM_OFFSET_DIRSTACK (EEPROM_OFFSET_PLAYTIME + 2)
// 1 byte
#define EEPROM_OFFSET_VOLUME   (EEPROM_OFFSET_DIRSTACK + 4)
// 2 bytes
#define EEPROM_OFFSET_REPLAYGAIN (EEPROM_OFFSET_VOLUME + 1)

#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C) // Application Interrupt and Reset Control location

static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

int freeMemory();

extern SdFs sd;

#endif
