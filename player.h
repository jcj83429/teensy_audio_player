#define USE_I2S_SLAVE 0

#include <play_sd_mp3.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>
#include "mycodecfile.h"
#include "directories.h"

extern AudioPlaySdMp3           playMp31;     //xy=100.25,89.25
extern AudioPlaySdAac           playAac1;     //xy=110.25,130.25
extern AudioPlaySdFlac          playFlac1;     //xy=119.25,173.25
extern AudioSynthWaveformSine   sine1;          //xy=150.25,217.25
extern AudioMixer4              mixer1;         //xy=371.25,109.25
extern AudioMixer4              mixer2;         //xy=371.25,221.25
extern AudioMixer4              mixer3;         //xy=723.25,165.25
extern AudioAnalyzeFFT256       fft256_1;       //xy=860.25,165.25

#if USE_I2S_SLAVE
//////////////////// I2S clock generator
extern Si5351 si5351;
#endif

extern SdBaseFile currentFile;
extern char currentFileName[256];
extern MyCodecFile myCodecFile;
extern bool isPaused;

extern DirectoryNavigator dirNav;

void audioInit();
void suspendDecoding();
void resumeDecoding();
void setSampleRate(unsigned long long sampleRate);
AudioCodec *getPlayingCodec();
void stop();
void playFile(SdBaseFile *file);
void playNext();
void playPrev();
void togglePause();
void seekAbsolute(uint32_t timesec);
void seekRelative(int dtsec);
