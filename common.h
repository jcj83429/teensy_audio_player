#ifndef COMMON_H
#define COMMON_H

#include <SdFs.h>

#define USE_F32 1
#define USE_I2S_SLAVE 0

// 2 bytes
#define EEPROM_OFFSET_PLAYTIME 0
// 4 bytes
#define EEPROM_OFFSET_DIRSTACK (EEPROM_OFFSET_PLAYTIME + 2)
// 1 byte
#define EEPROM_OFFSET_VOLUME   (EEPROM_OFFSET_DIRSTACK + 4)

static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

extern SdFs sd;

#endif
