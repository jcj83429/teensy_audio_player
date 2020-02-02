#include "player.h"

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

#if USE_I2S_SLAVE
//////////////////// I2S clock generator
Si5351 si5351;
#endif

SdBaseFile currentFile;
char currentFileName[256];
MyCodecFile myCodecFile(NULL);
bool isPaused = false;

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
  file->getName(currentFileName, sizeof(currentFileName));
  Serial.print("play file: ");
  Serial.println(currentFileName);

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
  isPaused = false;
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

void togglePause() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    isPaused = !isPaused;
    playingCodec->pause(isPaused);
  }
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

void seekRelative(int dtsec) {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    seekAbsolute(playingCodec->positionMillis() / 1000 + dtsec);
  }
}
