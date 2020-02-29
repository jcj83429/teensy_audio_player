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

// I2S wiring
// LRCLK = 23
// DOUT = 22
// BCLK = 9

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define LED_PIN 13

#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C) // Application Interrupt and Reset Control location

SdFs sd;

void low_voltage_isr(void){
  digitalWrite(LED_PIN, true);
  savePlayerState();
  // If we are still alive after 1s, reset
  delay(1000);
  Serial.end();  //clears the serial monitor  if used
  softReset();
}

void printStatus() {
  Serial.print("playing: ");
  Serial.print(playMp31.isPlaying());
  Serial.print(playAac1.isPlaying());
  Serial.print(playFlac1.isPlaying());
  Serial.print(", pos: ");
  Serial.print(myCodecFile.fposition());
  Serial.print("/");
  Serial.print(myCodecFile.fsize());

  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    Serial.print(" time: ");
    Serial.print(playingCodec->positionMillis());
    Serial.print("/");
    Serial.print(playingCodec->lengthMillis());
  }

#if USE_F32
  Serial.print(" effective replay gain: ");
  Serial.print(effectiveReplayGain());
#endif

  Serial.println();
}

void testSeek() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    uint32_t seekToTime = random(0, (int)playingCodec->lengthMillis() * 0.9 / 1000);
    seekAbsolute(seekToTime);
  }
}

void cycleReplayGain(){
  setReplayGainMode((ReplayGainMode)((replayGainMode + 1) % REPLAY_GAIN_MODES));
}

bool doSerialControl(){
  char strbuf[4] = {0};
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '\n':
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
          if (d == '\n') {
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
      case 'R':
        cycleReplayGain();
        break;
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
  // clear low voltage detection
  PMC_LVDSC2 |= PMC_LVDSC2_LVWACK;
  // choose high detect threshold, enable LV interrupt
  PMC_LVDSC2 = PMC_LVDSC2_LVWV(3) | PMC_LVDSC2_LVWIE;
  NVIC_ENABLE_IRQ(IRQ_LOW_VOLTAGE);

  Serial.begin(115200);
  //while(!Serial);

  pinMode(LED_PIN, OUTPUT);

  pinMode(PIN_KEY_RWD, INPUT_PULLUP);
  pinMode(PIN_KEY_PREV, INPUT_PULLUP);
  pinMode(PIN_KEY_PLAY, INPUT_PULLUP);
  pinMode(PIN_KEY_FF, INPUT_PULLUP);
  pinMode(PIN_KEY_NEXT, INPUT_PULLUP);
  pinMode(PIN_KEY_FN1, INPUT_PULLUP);

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
  pinMode(PIN_VFD_FRP, INPUT);
  digitalWrite(PIN_VFD_RST, LOW);
  delay(1);
  digitalWrite(PIN_VFD_RST, HIGH);

  SPI.begin();

  // set SPI mode once and never touch it again
  // The VFD supports up to 5MHz. Use half of it for safety
  SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE3));
  SPI.endTransaction();

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

  vfdInit();
  // Set brightness to min. VFD draws too much power for USB
  vfdSend(0x4f, true);
  vfdSetAutoInc(1, 0);
  Serial.println("VFD init done");

///// END VFD


///// SD CARD

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

  // sdEx.ls(LS_R | LS_DATE | LS_SIZE);

  startPlayback();

  initKeyStates();
}

void loop() {
  if(sd.sdErrorCode()){
    displayError("SD CARD ERROR       ", "REBOOTING...        ", 10000);
    delay(2000);
    softReset();
  }

  if(doSerialControl()){
    return;
  }

  if (dirNav.curDirFiles() && !getPlayingCodec()) {
    playNext();
  }

  uiUpdate();

  uiWriteFb();
}
