#ifndef COMMON_H
#define COMMON_H

#define USE_F32 1
#define USE_I2S_SLAVE 0

#define EEPROM_OFFSET_PLAYTIME 0 // 2 bytes
#define EEPROM_OFFSET_DIRSTACK 2 // 4 bytes

static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

#endif
