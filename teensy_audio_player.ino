#include "SdFat.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "si5351.h"
#include "vfd.h"
#include "font.h"
#include "ui.h"
#include "player.h"
#include "common.h"
#include <AudioStream_F32.h>
#include <EEPROM.h>

#if defined(__IMXRT1062__)
#include <ADC.h>
#endif

// I2S wiring
// LRCLK = 23
// DOUT = 22
// BCLK = 9

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define LED_PIN 13

#if defined(KINETISK) // Teensy 3.6
#define SRAM_U_BASE 0x20000000
// Allocate unused memory to fill up SRAM_L so the audio decoders only get SRAM_U memory.
// If the audio decoders get memory crossing the SRAM_L and SRAM_U boundary (0x20000000),
// they may do unaligned buffer copy across it which causes a hard fault.
void *unused_malloc_padding;
#endif

SdFs sd;

void low_voltage_isr(void){
  digitalWrite(LED_PIN, true);
  savePlayerState();
  // If we are still alive after 1s, reset
  delay(1000);
  Serial.end();  //clears the serial monitor  if used
  softReset();
}

#if defined(__IMXRT1062__)
// T4.1 has a much narrower range of operating voltage. At 2.9V, the MKL20's brownout
// detection will trigger and the MKL20 will reset the main processor. The highest
// brownout detect threshold available in the T4.1 is about 2.95V, which is too close
// to 2.9V. 
// Low voltage detection on T4.1 is implemented by using a 1:1 voltage divider to read
// the VIN voltage so the main processor can see the voltage drop before it starts to
// affect the 3.3V.
ADC *adc = new ADC();
#endif

#if defined(KINETISK) // Teensy 3.6

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

#else // Teensy 4.x

// from https://github.com/FrankBoesing/T4_PowerButton/blob/master/T4_PowerButton.cpp
#if defined(ARDUINO_TEENSY40)
  static const unsigned DTCM_START = 0x20000000UL;
  static const unsigned OCRAM_START = 0x20200000UL;
  static const unsigned OCRAM_SIZE = 512;
  static const unsigned FLASH_SIZE = 1984;
#elif defined(ARDUINO_TEENSY41)
  static const unsigned DTCM_START = 0x20000000UL;
  static const unsigned OCRAM_START = 0x20200000UL;
  static const unsigned OCRAM_SIZE = 512;
  static const unsigned FLASH_SIZE = 7936;
#if TEENSYDUINO>151  
  extern "C" uint8_t external_psram_size; 
#endif  
#endif

unsigned memfree(void) {
  extern unsigned long _ebss;
  extern unsigned long _sdata;
  extern unsigned long _estack;
  const unsigned DTCM_START = 0x20000000UL;
  unsigned dtcm = (unsigned)&_estack - DTCM_START;
  unsigned stackinuse = (unsigned) &_estack -  (unsigned) __builtin_frame_address(0);
  unsigned varsinuse = (unsigned)&_ebss - (unsigned)&_sdata;
  unsigned freemem = dtcm - (stackinuse + varsinuse);
  return freemem;
}

unsigned heapfree(void) {
// https://forum.pjrc.com/threads/33443-How-to-display-free-ram?p=99128&viewfull=1#post99128
  void* hTop = malloc(1024);// current position of heap.
  unsigned heapTop = (unsigned) hTop;
  free(hTop);
  unsigned freeheap = (OCRAM_START + (OCRAM_SIZE * 1024)) - heapTop;
  return freeheap;
}

#endif

int freeMemory() {
#if defined(KINETISK)

  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__

#else

  return heapfree();

#endif
}

void printStatus() {
  Serial.print("playing: ");
  Serial.print(playMp31.isPlaying());
  Serial.print(playAac1.isPlaying());
  Serial.print(playFlac1.isPlaying());
  Serial.print(playOpus1.isPlaying());
#if defined(__IMXRT1062__)
  Serial.print(playModule1.isPlaying());
#endif
  Serial.print(", pos: ");
  Serial.print(myCodecFile.fposition());
  Serial.print("/");
  Serial.print(myCodecFile.fsize());

  Serial.print(" time: ");
  Serial.print(positionMs());
  Serial.print("/");
  Serial.print(lengthMs());

#if USE_F32
  Serial.print(" effective replay gain: ");
  Serial.print(effectiveReplayGain());
#endif

  Serial.print(" free ram: ");
  Serial.print(freeMemory());

  Serial.println();
}

void testSeek() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    uint32_t seekToTime = random(0, (int)playingCodec->lengthMillis() * 0.9 / 1000);
    seekAbsolute(seekToTime);
  }
}

#if USE_F32
void cycleReplayGain(){
  setReplayGainMode((ReplayGainMode)((replayGainMode + 1) % REPLAY_GAIN_MODES));
}
#endif

void testSampleRate(){
  static int srIdx = 0;
  const int sampleRates[] = {44100, 88200, 22050, 48000, 96000, 24000};
  srIdx = (srIdx + 1) % 6;
  setSampleRate(sampleRates[srIdx]);
}

bool doSerialControl(){
  char strbuf[4] = {0};
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '\n':
      case '\r':
        break;
      case 'A':
        testSampleRate();
        break;
      case 'E':
        displayError("TEST ERROR", "TEST ERROR", 10000);
        break;
      case 'F':
        for (int i = 0 ; i < 128; i++) {
          Serial.println(fft256.output[i]);
        }
        break;
      case 'G':
        for (int i = 0; i < 3; ) {
          if (!Serial.available()) {
            continue;
          }
          char d = Serial.read();
          if (d == '\n' || d == '\r') {
            strbuf[i] = 0;
            break;
          } else if (isdigit(d)) {
            strbuf[i] = d;
          } else {
            // invalid
            Serial.print("invalid digit ");
            Serial.println(d);
            return false;
          }
          i++;
        }
        {
          int itemNum = atoi(strbuf);
          Serial.print("select item ");
          Serial.println(itemNum);
          FsFile tmpFile = dirNav.selectItem(itemNum);
          if (tmpFile.isOpen()) {
            stop();
            currentFile = tmpFile;
            playFile(&currentFile);
          }
        }
        break;
      case 'I':
        togglePause();
        break;
      case 'L':
        dirNav.printCurDir();
        break;
      case 'N':
        playNext();
        break;
      case 'P':
        playPrev();
        break;
#if USE_F32
      case 'R':
        cycleReplayGain();
        break;
#endif
      case 'S':
        printStatus();
        break;
      case 'T':
        testSeek();
        break;
      case 'U':
        dirNav.upDir();
        break;
      case 'V':
        savePlayerState();
        break;
      case '>':
        seekRelative(+5);
        break;
      case '<':
        seekRelative(-5);
        break;
#if USE_F32
      case '+':
        setGain(currentGain + 1);
        break;
      case '-':
        setGain(currentGain - 1);
        break;
      case ']':
        setReplayGainFallbackGain(rgFallbackGain + 1);
        break;
      case '[':
        setReplayGainFallbackGain(rgFallbackGain - 1);
        break;
#endif
      default:
        Serial.print("unknown cmd ");
        Serial.println(c);
        return false;
    }
    return true;
  }
  return false;
}

void setup() {
  delay(100);
  Serial.begin(115200);
  //while(!Serial);

  pinMode(LED_PIN, OUTPUT);

  pinMode(PIN_KEY_RWD, INPUT_PULLDOWN);
  pinMode(PIN_KEY_PREV, INPUT_PULLDOWN);
  pinMode(PIN_KEY_PLAY, INPUT_PULLDOWN);
  pinMode(PIN_KEY_FF, INPUT_PULLDOWN);
  pinMode(PIN_KEY_NEXT, INPUT_PULLDOWN);
  pinMode(PIN_KEY_FN1, INPUT_PULLDOWN);

///// AUDIO
  AudioMemory(20);
#if USE_F32
  AudioMemory_F32(8);
#endif

  // mix L+R for FFT
  mixer3.gain(0, 0.5);
  mixer3.gain(3, 0.5);

  fft256.averageTogether(2);

  // FOR DAC SANITY
  // sine1.amplitude(1);
  // sine1.frequency(1000);

#if !USE_F32
#if USE_I2S_SLAVE
  // si5351 setup
  bool si5351_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if (si5351_found) {
    Serial.println("si5351 found");
  } else {
    while (1) {
      Serial.println("si5351 not found");
      delay(100);
    }
  }
#endif
#endif
///// END AUDIO

///// VFD
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
#endif

#endif
  vfdInit();
  // Set brightness to min. VFD draws too much power for USB
  vfdSend(0x4f, true);
  vfdSetAutoInc(1, 0);
  Serial.println("VFD init done");

///// END VFD

  uiInit();

///// SD CARD

  //while (!sd.begin(SdSpiConfig(SDCARD_SS_PIN, DEDICATED_SPI, SD_SCK_MHZ(50)))) {
  while (!sd.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("SdFs begin() failed");
    displayError("SdFs begin() failed ", "                    ", 10000);
  }
  clearError();
  Serial.println("SD init success");
  printStr("Loading SD card     ", 21, 0, 0, false);
  printStr("                    ", 21, 0, 1, false);
  uiWriteFb();

///// END SD CARD

  Serial.println("ALL INIT DONE!");

  // make sd the current volume.
  sd.chvol();

#if defined(KINETISK) // Teensy 3.6
  size_t heapTop = (size_t)sbrk(0);
  Serial.print("heapTop at ");
  Serial.println(heapTop, HEX);
  if(heapTop <= SRAM_U_BASE){
    size_t paddingSize = SRAM_U_BASE - heapTop + 8192;
    Serial.print("allocating ");
    Serial.print(paddingSize);
    Serial.println(" bytes of unused memory to fill up SRAM_L");
    unused_malloc_padding = malloc(paddingSize);
    heapTop = (size_t)sbrk(0);
    Serial.print("heapTop now at ");
    Serial.println(heapTop, HEX);
    Serial.print("freeMemory: ");
    Serial.println(freeMemory());
  }else{
    Serial.print("SRAM_L is already filled by global and static objects");
  }
#endif

  startPlayback();

  initKeyStates();

#if defined(KINETISK) // Teensy 3.6
  // clear low voltage detection
  PMC_LVDSC2 |= PMC_LVDSC2_LVWACK;
  // choose high detect threshold, enable LV interrupt
  PMC_LVDSC2 = PMC_LVDSC2_LVWV(3) | PMC_LVDSC2_LVWIE;
  NVIC_ENABLE_IRQ(IRQ_LOW_VOLTAGE);
#else
  pinMode(VOLTAGE_DIVIDER_PIN, INPUT);
  // trigger low_voltage_isr when VIN falls below 3.7V
  adc->adc0->enableCompare((3.7/2) / 3.3 * adc->adc0->getMaxValue(), false);
  adc->adc0->enableInterrupts(low_voltage_isr);
  adc->adc0->startContinuous(VOLTAGE_DIVIDER_PIN);
#endif
}

void loop() {
  if(sd.sdErrorCode()){
    Serial.print("SD ERROR CODE ");
    Serial.println(sd.sdErrorCode());
    displayError("SD CARD ERROR       ", "REBOOTING...        ", 10000);
    delay(2000);
    softReset();
  }

  if(doSerialControl()){
    return;
  }

  if (dirNav.curDirFiles() && !isPlaying()) {
    playNext();
  }

  uiUpdate();

  uiWriteFb();
}
