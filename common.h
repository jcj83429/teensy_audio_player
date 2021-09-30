#ifndef COMMON_H
#define COMMON_H

#include <SdFat.h>

#define USE_F32 1

// running multiple FFTs on teensy 3.6 causes serious performance problems when playing opus or high sample rate flac
#define USE_MRFFT (USE_F32 && defined(__IMXRT1062__))

#define DAC_MAX_SAMPLE_RATE 192000

#define USE_VFD 1
#define USE_OLED 0

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
// 1 byte
#define EEPROM_OFFSET_FFT_MODE (EEPROM_OFFSET_RESUME_ATTEMPT + 1)

#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C) // Application Interrupt and Reset Control location

#define DUMPVAL(x) do{ Serial.print(#x); Serial.print(": "); Serial.println(x, HEX); }while(0);

static inline void softReset(){
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

int freeMemory();

extern SdFs sd;

#endif
