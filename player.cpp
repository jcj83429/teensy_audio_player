#include "player.h"
#include "ui.h"
#include "myxmpcallbacks.h"
#include <EEPROM.h>
#include <AudioStream_F32.h>
#include <AudioConvert_F32.h>
#include <output_i2s_f32.h>
#include <AudioEffectGain_F32.h>

//////////////////// Teensy Audio library
// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;     //xy=100.25,89.25
AudioPlaySdAac           playAac1;     //xy=110.25,130.25
AudioPlaySdFlac          playFlac1;     //xy=119.25,173.25
AudioPlaySdOpus          playOpus1;
//AudioSynthWaveformSine   sine1;          //xy=150.25,217.25
AudioMixer4              mixer1;         //xy=371.25,109.25
AudioMixer4              mixer2;         //xy=371.25,221.25
AudioMixer4              mixer4;
AudioMixer4              mixer5;
AudioConnection          patchCord1(playMp31, 0, mixer1, 0);
AudioConnection          patchCord2(playMp31, 1, mixer2, 0);
AudioConnection          patchCord3(playAac1, 0, mixer1, 1);
AudioConnection          patchCord4(playAac1, 1, mixer2, 1);
AudioConnection          patchCord5(playFlac1, 0, mixer1, 2);
AudioConnection          patchCord6(playFlac1, 1, mixer2, 2);
AudioConnection          patchCord7(playOpus1, 0, mixer4, 0);
AudioConnection          patchCord8(playOpus1, 1, mixer5, 0);
AudioConnection          patchCord16(mixer4, 0, mixer1, 3);
AudioConnection          patchCord17(mixer5, 0, mixer2, 3);

#if defined(__IMXRT1062__)
// Only T4.1 has enough memory for modules
TeensyXmp                playModule1;
AudioConnection          patchCord14(playModule1, 0, mixer4, 0);
AudioConnection          patchCord15(playModule1, 1, mixer5, 0);

EXTMEM uint8_t psram_heap[8*1024*1024];
tlsf_t psram_alloc;
int psram_used = 0;
#endif

//AudioConnection          patchCord7(sine1, 0, mixer4, 3);
//AudioConnection          patchCord8(sine1, 0, mixer5, 3);
// GUItool: end automatically generated code 

AudioPeakHold         analyzePeak1;
AudioPeakHold         analyzePeak2;
AudioConnection          patchCord101(mixer1, analyzePeak1);
AudioConnection          patchCord102(mixer2, analyzePeak2);

#if !USE_F32

// 16 bit out
#if USE_I2S_SLAVE
AudioOutputI2Sslave      i2s1;
//////////////////// I2S clock generator
Si5351 si5351;
#else
AudioOutputI2S           i2s1;
#endif
// 16 bit fft
AudioAnalyzeFFT256       fft256;
AudioMixer4              mixer3;
AudioConnection          patchCord9(mixer1, 0, i2s1, 0);
AudioConnection          patchCord11(mixer2, 0, i2s1, 1);
AudioConnection          patchCord10(mixer1, 0, mixer3, 0);
AudioConnection          patchCord12(mixer2, 0, mixer3, 3);
AudioConnection          patchCord13(mixer3, fft256);

#else

// 32 bit out
AudioConvert_I16toF32    i16tof32l, i16tof32r;
AudioEffectGain_F32      gain1, gain2;
AudioOutputI2S_F32       i2s32;
// float fft
AudioAnalyzeFFT256_F32   fft256;
AudioMixer4_F32          mixer3;
AudioConnection          patchCordi01(mixer1, 0, i16tof32l, 0);
AudioConnection          patchCordi02(mixer2, 0, i16tof32r, 0);
AudioConnection_F32      patchCordf01(i16tof32l, 0, gain1, 0);
AudioConnection_F32      patchCordf02(i16tof32r, 0, gain2, 0);
AudioConnection_F32      patchCordf03(gain1, 0, i2s32, 0);
AudioConnection_F32      patchCordf04(gain2, 0, i2s32, 1);
AudioConnection_F32      patchCordf05(i16tof32l, 0, mixer3, 0);
AudioConnection_F32      patchCordf06(i16tof32r, 0, mixer3, 3);
AudioConnection_F32      patchCordf07(mixer3, 0, fft256, 0);

#endif

FsFile currentFile;
char currentFileName[256] = {0};
MyCodecFile myCodecFile(NULL);

#if defined(__IMXRT1062__)
struct xmp_io_callbacks myXmpIoCb = {
  .read = my_xmp_read,
  .seek = my_xmp_seek,
  .tell = my_xmp_tell,
  .eof = my_xmp_eof,
  .size = my_xmp_size,
  .user_data = NULL,
};
#endif

bool isPaused = false;

DirectoryNavigator dirNav;

#if defined(__IMXRT1062__)
void *xmp_malloc(size_t bytes){
  void *ptr = tlsf_malloc(psram_alloc, bytes);
  if(!ptr){
    Serial.print("xmp_malloc failed to allocate ");
    Serial.print(bytes);
    Serial.println("bytes");
  }else{
    psram_used += tlsf_block_size(ptr);
  }
  return ptr;
}

void xmp_free(void *ptr){
  psram_used -= tlsf_block_size(ptr);
  tlsf_free(psram_alloc, ptr);
}

void *xmp_realloc(void *ptr, size_t bytes){
  int old_block_size =  tlsf_block_size(ptr);
  ptr = tlsf_realloc(psram_alloc, ptr, bytes);
  if(!ptr){
    Serial.print("xmp_realloc failed to allocate ");
    Serial.print(bytes);
    Serial.println("bytes");
  }else{
    psram_used += tlsf_block_size(ptr) - old_block_size;
  }
  return ptr;
}
#endif

void startPlayback(){
#if USE_F32
  int8_t gain8 = EEPROM.read(EEPROM_OFFSET_VOLUME);
  setGain(gain8);
  int8_t rgMode = EEPROM.read(EEPROM_OFFSET_REPLAYGAIN);
  setReplayGainMode((ReplayGainMode)rgMode);
  int8_t fbGain8 = EEPROM.read(EEPROM_OFFSET_REPLAYGAIN + 1);
  setReplayGainFallbackGain(fbGain8);
#endif

  dirNav.openRoot(&sd);

  currentFile = dirNav.restoreCurrentFile();
  if(currentFile.isOpen()){
    Serial.println("continue playing last played file");
    playFile(&currentFile);
    uint16_t playPosSec = 0;
    for(int i = sizeof(playPosSec) - 1; i >= 0; i--){
      playPosSec <<= 8;
      playPosSec |= EEPROM.read(EEPROM_OFFSET_PLAYTIME + i);
    }
    Serial.print("resume to ");
    Serial.println(playPosSec);
    seekAbsolute(playPosSec);
    return;
  }

  // resume failed. go back to root.
  while(dirNav.upDir());

  if (!playNext()) {
    displayError("NO FILES            ", "                    ", 10000);
    while (true) {
      Serial.println("NO FILES");
      delay(1000);
    }
  }
}

void savePlayerState(){
  uint16_t playPosSec = positionMs() / 1000;
  for(unsigned int i = 0; i < sizeof(playPosSec); i++){
    EEPROM.write(EEPROM_OFFSET_PLAYTIME + i, playPosSec & 0xff);
    playPosSec >>= 8;
  }

  dirNav.saveCurrentFile();

#if USE_F32
  int8_t gain8 = min(max(currentGain, -127), 127);
  EEPROM.write(EEPROM_OFFSET_VOLUME, gain8);
  EEPROM.write(EEPROM_OFFSET_REPLAYGAIN, replayGainMode);
  int8_t fbGain8 = min(max(rgFallbackGain, -127), 127);
  EEPROM.write(EEPROM_OFFSET_REPLAYGAIN + 1, fbGain8);
#endif
}

// we can't disable the audio interrupt because otherwise the audio output will glitch.
// so we suspend the decoders' decoding while keeping their outputs running
void suspendDecoding() {
  // for MP3 and AAC, just disable the ISR
  if (playMp31.isPlaying() || playAac1.isPlaying() || playOpus1.isPlaying()) {
    NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the suspend/resumeDecoding interface
  if (playFlac1.isPlaying()) {
    playFlac1.suspendDecoding();
  }
  // There is no need to suspend decoding for modules. 
  // libxmp loads the whole module into memory and plays from there 
  // so it doesn't conflict with sd card access
}

void resumeDecoding() {
  // for MP3 and AAC, just enable and trigger the ISR
  if (playMp31.isPlaying() || playAac1.isPlaying() || playOpus1.isPlaying()) {
    NVIC_ENABLE_IRQ(IRQ_AUDIOCODEC);
    NVIC_TRIGGER_INTERRUPT(IRQ_AUDIOCODEC);
  }
  // for FLAC, use the resumeDecoding interface to safely fill the buffer
  if (playFlac1.isPlaying()) {
    playFlac1.resumeDecoding();
  }
}

void setSampleRate(unsigned long long sampleRate) {
  Serial.print("changing sample rate to ");
  Serial.println((int)sampleRate);
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
#else
  float result = setI2SFreq(sampleRate);
  if(!result){
    Serial.println("sample rate not supported");
  }
#endif
  if(sampleRate <= 24000){
    fft256.averageTogether(1);
  }else if(sampleRate <= 48000){
    fft256.averageTogether(2);
  }else{
    fft256.averageTogether(4);
  }
}

AudioCodec *getPlayingCodec() {
  if (playMp31.isPlaying()) {
    return &playMp31;
  } else if (playAac1.isPlaying()) {
    return &playAac1;
  } else if (playFlac1.isPlaying()) {
    return &playFlac1;
  } else if (playOpus1.isPlaying()) {
    return &playOpus1;
  }
  return NULL;
}

void stop() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    playingCodec->stop();
  }
#if defined(__IMXRT1062__)
  if (playModule1.isPlaying()) {
    playModule1.stop();
  }
#endif
}

void playFile(FsFile *file) {
  Serial.print("play file: ");
  file->getName(currentFileName, sizeof(currentFileName));
  Serial.println(currentFileName);

  stop();

  file->rewind();
  myCodecFile = MyCodecFile(file);
  int error = 0;
  switch (getFileType(file)) {
    case FileType::MP3:
      error = playMp31.play(&myCodecFile);
      break;
    case FileType::AAC:
      error = playAac1.play(&myCodecFile);
      break;
    case FileType::FLAC:
      error = playFlac1.play(&myCodecFile);
      break;
    case FileType::OPUS:
      error = playOpus1.play(&myCodecFile);
      break;
#if defined(__IMXRT1062__)
    case FileType::MODULE:
      myXmpIoCb.user_data = file;
      error = !playModule1.playModuleWithCallbacks(&myXmpIoCb);
      break;
#endif
    default:
      Serial.println("WTF attempting to play dir or unsupported file?");
      break;
  }

  Serial.print("error: 0x");
  Serial.println(error, HEX);
  isPaused = false;

  if(!error){
    AudioCodec *playingCodec = getPlayingCodec();
    if(playingCodec){
      uint32_t sr = playingCodec->sampleRate();
      if(sr > DAC_MAX_SAMPLE_RATE){
        char srStr[21];
        snprintf(srStr, 21, "%-20lu", sr);
        displayError("UNSUP. SAMPLE RATE: ", srStr, 3000);
        stop();
      }else{
        setSampleRate(sr);
      }
      Serial.print("RG track peak: ");
      Serial.println(playingCodec->replaygainPeak(false));
      Serial.print("RG track gain: ");
      Serial.println(playingCodec->replaygainGainDb(false));
      Serial.print("RG album peak: ");
      Serial.println(playingCodec->replaygainPeak(true));
      Serial.print("RG album gain: ");
      Serial.println(playingCodec->replaygainGainDb(true));
#if defined(__IMXRT1062__)
    }else{
      // XMP
      setSampleRate(44100);
      Serial.print("psram_used: ");
      Serial.println(psram_used);
#endif
    }

#if USE_F32
    setGain(currentGain); // apply new replaygain
#endif
  }else{
    displayError("UNSUP. FILE SKIPPED:", currentFileName, 3000);
  }
}

bool playNext() {
  stop();
  currentFile = dirNav.nextFile();
  if(currentFile.isOpen()){
    playFile(&currentFile);
    return true;
  }else{
    return false;
  }
}

bool playPrev() {
  stop();
  currentFile = dirNav.prevFile();
  if(currentFile.isOpen()){
    playFile(&currentFile);
    return true;
  }else{
    return false;
  }
  
}

bool isPlaying() {
  if (getPlayingCodec()) {
    return true;
  }
#if defined(__IMXRT1062__)
  if (playModule1.isPlaying()) {
    return true;
  }
#endif
  return false;
}

void togglePause() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    isPaused = !isPaused;
    playingCodec->pause(isPaused);
#if defined(__IMXRT1062__)
  } else if(playModule1.isPlaying()) {
    isPaused = !isPaused;
    playModule1.pause(isPaused);
#endif
  }
}

uint32_t lengthMs() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    return playingCodec->lengthMillis();
#if defined(__IMXRT1062__)
  } else if (playModule1.isPlaying()) {
    return playModule1.lengthMs();
#endif
  }
  return 0;
}

uint32_t positionMs() {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    return playingCodec->positionMillis();
#if defined(__IMXRT1062__)
  } else if (playModule1.isPlaying()) {
    return playModule1.positionMs();
#endif
  }
  return 0;
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
#if defined(__IMXRT1062__)
  } else if (playModule1.isPlaying()) {
    if (timesec > (uint32_t)playModule1.lengthMs() / 1000) {
      return;
    }
    Serial.print("seeking to ");
    Serial.println(timesec);
    bool result = playModule1.seekSec(timesec);
    Serial.print("result: ");
    Serial.println(result);
    Serial.print("positionMs: ");
    Serial.println(playModule1.positionMs());
#endif
  }
}

void seekRelative(int dtsec) {
  AudioCodec *playingCodec = getPlayingCodec();
  if (playingCodec) {
    int timesec = playingCodec->positionMillis() / 1000 + dtsec;
    if (timesec < 0) {
      timesec = 0;
    }
    seekAbsolute(timesec);
#if defined(__IMXRT1062__)
  } else if (playModule1.isPlaying()) {
    // module seeking is not accurate. just seek to next/prev pattern
    if (dtsec > 0) {
      playModule1.seekNextPos();
    } else if (dtsec < 0) {
      playModule1.seekPrevPos();
    }
#endif
  }
}

#if USE_F32
float currentGain = 0;
ReplayGainMode replayGainMode = REPLAY_GAIN_OFF;
float rgFallbackGain = 0;

void setGain(float dB) {
  Serial.print("setting gain to ");
  Serial.println(dB);
  currentGain = dB;

  dB += effectiveReplayGain();
  Serial.print("effective gain (with RG): ");
  Serial.println(dB);
  
  gain1.setGain_dB(dB);
  gain2.setGain_dB(dB);
}

void setReplayGainMode(ReplayGainMode rgMode){
  replayGainMode = (ReplayGainMode)(rgMode % REPLAY_GAIN_MODES);
  setGain(currentGain);
}

void setReplayGainFallbackGain(float dB){
  rgFallbackGain = dB;
  setGain(currentGain);
}

float effectiveReplayGain(){
  if(replayGainMode == REPLAY_GAIN_OFF){
    return 0;
  }

  AudioCodec *playingCodec = getPlayingCodec();
  if(!playingCodec){
    return 0;
  }

  float replayGain = NAN;
  float peak = 1;
  
  if(replayGainMode == REPLAY_GAIN_TRACK || replayGainMode == REPLAY_GAIN_ALBUM){
    bool preferAlbumGain = replayGainMode == REPLAY_GAIN_ALBUM;
    replayGain = playingCodec->replaygainGainDb(preferAlbumGain);
    peak = playingCodec->replaygainPeak(preferAlbumGain);
    if(isnan(replayGain)){
      // try the other one
      replayGain = playingCodec->replaygainGainDb(!preferAlbumGain);
      peak = playingCodec->replaygainPeak(!preferAlbumGain);
    }
  }else{
    // REPLAY_GAIN_AVERAGE
    float rgAlbum = playingCodec->replaygainGainDb(true);
    float rgTrack = playingCodec->replaygainGainDb(false);
    float peakAlbum = playingCodec->replaygainPeak(true);
    float peakTrack = playingCodec->replaygainPeak(false);
    if(!isnan(rgAlbum) && !isnan(rgTrack)){
      // for the gain, use the average
      replayGain = (rgAlbum + rgTrack)/2;
      // the album peak should alwayw be >= the track peak
      peak = (peakAlbum + peakTrack)/2;
    }else if(!isnan(rgAlbum)){
      replayGain = rgAlbum;
      peak = peakAlbum;
    }else if(!isnan(rgTrack)){
      replayGain = rgTrack;
      peak = peakTrack;
    }
  }
  
  if(isnan(replayGain)){
    return rgFallbackGain;
  }
  
  float peakDb = log10(peak) * 20;
  float peakAfterGain = peakDb + currentGain + replayGain;
  if(peakAfterGain > 0){
    replayGain -= peakAfterGain;
  }
  return replayGain;
}
#endif
