#ifndef COMMON_H
#define COMMON_H

#include <SdFat.h>

#define USE_F32 1
#define DAC_MAX_SAMPLE_RATE 192000

// Needed for T4.1 only
// VIN -- 100K ohm -- A8 -- 100K ohm -- GND
#define VOLTAGE_DIVIDER_PIN A8

// 2 bytes
#define EEPROM_OFFSET_PLAYTIME 0
// 4 bytes
#define EEPROM_OFFSET_DIRSTACK (EEPROM_OFFSET_PLAYTIME + 2)
// 1 byte
#define EEPROM_OFFSET_VOLUME   (EEPROM_OFFSET_DIRSTACK + 4)
// 2 bytes
#define EEPROM_OFFSET_REPLAYGAIN (EEPROM_OFFSET_VOLUME + 1)
// 1 byte
#define EEPROM_OFFSET_RESUME_ATTEMPT (EEPROM_OFFSET_REPLAYGAIN + 2)

#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C) // Application Interrupt and Reset Control location

static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

int freeMemory();

extern SdFs sd;

#endif
