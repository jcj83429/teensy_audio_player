#include "SdFat.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "si5351.h"
#include "vfd.h"
#include "font.h"
#include <play_sd_mp3.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>

#include "mycodecfile.cpp"
#include "directories.h"

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define LED_PIN 13

#define MAX_FILES 256

#define USE_I2S_SLAVE 0

#if USE_I2S_SLAVE
//////////////////// I2S clock generator
Si5351 si5351;
#endif

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;     //xy=100.25,89.25
AudioPlaySdAac           playAac1;     //xy=110.25,130.25
AudioPlaySdFlac          playFlac1;     //xy=119.25,173.25
AudioSynthWaveformSine   sine1;          //xy=150.25,217.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
#if USE_I2S_SLAVE
AudioOutputI2Sslave      i2s1;           //xy=592.25,164.25
#else
AudioOutputI2S           i2s1;           //xy=592.25,164.25
#endif
AudioMixer4              mixer3;         //xy=723.25,165.25
AudioAnalyzeFFT256       fft256_1;       //xy=860.25,165.25
AudioConnection          patchCord1(playMp31, 0, mixer1, 0);
AudioConnection          patchCord2(playMp31, 1, mixer2, 0);
AudioConnection          patchCord3(playAac1, 0, mixer1, 1);
AudioConnection          patchCord4(playAac1, 1, mixer2, 1);
AudioConnection          patchCord5(playFlac1, 0, mixer1, 2);
AudioConnection          patchCord6(playFlac1, 1, mixer2, 2);
AudioConnection          patchCord7(sine1, 0, mixer1, 3);
AudioConnection          patchCord8(sine1, 0, mixer2, 3);
AudioConnection          patchCord9(mixer1, 0, i2s1, 0);
AudioConnection          patchCord10(mixer1, 0, mixer3, 0);
AudioConnection          patchCord11(mixer2, 0, i2s1, 1);
AudioConnection          patchCord12(mixer2, 0, mixer3, 3);
AudioConnection          patchCord13(mixer3, fft256_1);
// GUItool: end automatically generated code

SdFatSdioEX sdEx;

SdBaseFile curDir;
int numFiles = 0;
uint16_t curDirFileIdx[MAX_FILES];

SdBaseFile currentFile;
MyCodecFile myCodecFile(NULL);
int currentFileIndex = -1;

DirectoryNavigator dirNav;

// we can't disable the audio interrupt because otherwise the audio output will glitch.
// so we suspend the decoders' decoding while keeping their outputs running
void suspendDecoding() {
  // for MP3 and AAC, just disable the ISR
  if (playMp31.isPlaying() || playAac1.isPlaying()) {
    NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the suspend/resumeDecoding interface
  if (playFlac1.isPlaying()) {
    playFlac1.suspendDecoding();
  }
}

void resumeDecoding() {
  // for MP3 and AAC, just enable and trigger the ISR
  if (playMp31.isPlaying() || playAac1.isPlaying()) {
    NVIC_ENABLE_IRQ(IRQ_AUDIOCODEC);
    NVIC_TRIGGER_INTERRUPT(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the resumeDecoding interface to safely fill the buffer
  if (playFlac1.isPlaying()) {
    playFlac1.resumeDecoding();
  }
}

void setSampleRate(unsigned long long sampleRate) {
#if USE_I2S_SLAVE
  uint8_t error;
  // sometimes the sample rate setting doens't go through, so repeate 3 times
  for (int i = 0; i < 3; i++) {
    // clk0 = LRCLK
    error = si5351.set_freq(sampleRate * SI5351_FREQ_MULT, SI5351_CLK0);
    if (error) {
      Serial.print("error clk0 ");
      Serial.println(error);
      return;
    }
    // clk1 = BCLK
    error = si5351.set_freq(64 * sampleRate * SI5351_FREQ_MULT, SI5351_CLK1);
    if (error) {
      Serial.print("error clk1 ");
      Serial.println(error);
      return;
    }
  }
  Serial.print("sample rate changed to ");
  Serial.println((int)sampleRate);
#endif
  // sample rate change for master needs to be implemented
}

AudioCodec *getPlayingCodec() {
  if (playMp31.isPlaying()) {
    return &playMp31;
  } else if (playAac1.isPlaying()) {
    return &playAac1;
  } else if (playFlac1.isPlaying()) {
    return &playFlac1;
  }
  return NULL;
}

void stop() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    playingCodec->stop();
  }
}

void playFile(SdBaseFile *file) {
  char tmpFileName[256];
  file->getName(tmpFileName, sizeof(tmpFileName));
  Serial.print("play file: ");
  Serial.println(tmpFileName);

  stop();

  file->rewind();
  myCodecFile = MyCodecFile(file);
  int error = 0;
  switch (getFileType(file)) {
    case FileType::MP3:
      playMp31.load(&myCodecFile);
      error = playMp31.play();
      break;
    case FileType::AAC:
      playAac1.load(&myCodecFile);
      error = playAac1.play();
      break;
    case FileType::FLAC:
      playFlac1.load(&myCodecFile);
      error = playFlac1.play();
      break;
    default:
      Serial.println("WTF attempting to play dir or unsupported file?");
      break;
  }

  Serial.print("error: 0x");
  Serial.println(error, HEX);
}

void playNext() {
  stop();
  currentFile = dirNav.nextFile();
  playFile(&currentFile);
}

void playPrev() {
  stop();
  currentFile = dirNav.prevFile();
  playFile(&currentFile);
}

void flashError(int error) {
  for (int i = 0; i < error; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
  delay(1000);
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

  Serial.println();
}

void seekAbsolute(uint32_t timesec) {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    if (timesec > playingCodec->lengthMillis() / 1000) {
      return;
    }
    Serial.print("seeking to ");
    Serial.println(timesec);
    bool result = playingCodec->seek(timesec);
    Serial.print("result: ");
    Serial.println(result);
    Serial.print("positionMillis: ");
    Serial.println(playingCodec->positionMillis());
  }
}

void testSeek() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    uint32_t seekToTime = random(0, (int)playingCodec->lengthMillis() * 0.9 / 1000);
    seekAbsolute(seekToTime);
  }
}

void seekRelative(int dtsec) {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    seekAbsolute(playingCodec->positionMillis() / 1000 + dtsec);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);

///// AUDIO
  // put your setup code here, to run once:
  AudioMemory(20);

  // mix L+R for FFT
  mixer3.gain(0, 0.5);
  mixer3.gain(3, 0.5);

  // FOR DAC SANITY
  // sine1.amplitude(0.20);
  // sine1.frequency(1000);

#if USE_I2S_SLAVE
  // si5351 setup
  bool si5351_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if (si5351_found) {
    Serial.println("si5351 found");
  } else {
    while (1) {
      Serial.println("si5351 not found");
      flashError(1);
    }
  }
#endif

  setSampleRate(44100);

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

  Serial.println("VFD init done");

  vfdSetAutoInc(1, 0);

  for (int i = 0; i < 6; i++) {
    vfdSetCursor(0, i + 2);
    for (int j = 0; j < 96; j++) {
      vfdSend(ms_6x8_font[0x20 + 16 * i + j / 6][j % 6], false);
    }
  }

///// END VFD

///// SD CARD

  if (!sdEx.begin()) {
    while (1) {
      Serial.println("SdFatSdioEX begin() failed");
      flashError(2);
    }
  } else {
    Serial.println("SD init success");
  }

///// END SD CARD

  Serial.println("ALL INIT DONE!");

  // make sdEx the current volume.
  sdEx.chvol();

  // sdEx.ls(LS_R | LS_DATE | LS_SIZE);

  dirNav.openRoot(SdBaseFile::cwd()->volume());

  if (dirNav.curDirFiles()) {
    playNext();
  } else {
    while (true) {
      Serial.println("NO FILES");
      delay(1000);
    }
  }
}

int sampleRates[] = {44100, 88200, 22050};
int sampleRateIndex = 0;

char strbuf[4] = {0};
void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '\n':
        break;
      case 'F':
        for (int i = 0 ; i < 128; i++) {
          Serial.println(fft256_1.output[i]);
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
            return;
          }
          i++;
        }
        {
          int itemNum = atoi(strbuf);
          Serial.print("select item ");
          Serial.println(itemNum);
          SdBaseFile tmpFile = dirNav.selectItem(itemNum);
          if (tmpFile.isOpen()) {
            stop();
            currentFile = tmpFile;
            playFile(&currentFile);
          }
        }
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
        sampleRateIndex = (sampleRateIndex + 1) % 3;
        setSampleRate(sampleRates[sampleRateIndex]);
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
      case '>':
        seekRelative(+5);
        break;
      case '<':
        seekRelative(-5);
        break;
      default:
        Serial.print("unknown cmd ");
        Serial.println(c);
        break;
    }
    return;
  }

  if (dirNav.curDirFiles() && !getPlayingCodec()) {
    playNext();
  }

  // spectrum on top half of the screen
  for (int i = 0 ; i < 128; i++) {
    uint16_t fftBin = fft256_1.output[i];
    uint32_t fftBinLog = fftBin ? (32 - __builtin_clz((uint32_t)fftBin)) : 0;
    uint32_t lsb = 0;
    if (fftBinLog > 1) {
      // use the bit after the leading bit to add an approximate bit to the log
      lsb = (fftBin >> (fftBinLog - 2)) & 1;
    }
    fftBinLog = (fftBinLog << 1) | lsb;
    uint32_t fftBinLogMask = fftBinLog ? (0x1 << (32 - fftBinLog)) : 0;
    framebuffer[0][i] = fftBinLogMask;
    framebuffer[1][i] = fftBinLogMask >> 8;
    framebuffer[2][i] = fftBinLogMask >> 16;
    framebuffer[3][i] = fftBinLogMask >> 24;
  }
  // snow on bottom half of the screen for stress testing
  for (int i = 0; i < 128; i++) {
    for (int j = 4; j < 8; j++) {
      framebuffer[j][i] = 0;
      for (int k = 0; k < 8; k++) {
        if (random(8) == 0) {
          framebuffer[j][i] |= 1 << k;
        }
      }
    }
  }
  vfdWriteFb(0);
}
