#include <play_sd_mp3.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>
#include "common.h"
#include "mycodecfile.h"
#include "directories.h"
#include "analyze_fft256_f32.h"
#include <AudioMixer_F32.h>

extern AudioPlaySdMp3           playMp31;     //xy=100.25,89.25
extern AudioPlaySdAac           playAac1;     //xy=110.25,130.25
extern AudioPlaySdFlac          playFlac1;     //xy=119.25,173.25
extern AudioSynthWaveformSine   sine1;          //xy=150.25,217.25
#if !USE_F32
extern AudioMixer4              mixer3;
extern AudioAnalyzeFFT256       fft256;
#else
extern AudioMixer4_F32          mixer3;
extern AudioAnalyzeFFT256_F32   fft256;
#endif

#if !USE_F32

#if USE_I2S_SLAVE
//////////////////// I2S clock generator
extern Si5351 si5351;
#endif

#endif

extern FsFile currentFile;
extern char currentFileName[256];
extern MyCodecFile myCodecFile;
extern bool isPaused;

extern DirectoryNavigator dirNav;

void audioInit();
void startPlayback();
void savePlayerState();
void suspendDecoding();
void resumeDecoding();
void setSampleRate(unsigned long long sampleRate);
AudioCodec *getPlayingCodec();
void stop();
void playFile(FsFile *file);
bool playNext();
bool playPrev();
void togglePause();
void seekAbsolute(uint32_t timesec);
void seekRelative(int dtsec);

#if USE_F32
extern float currentGain;
void setGain(float32_t dB);
#endif
