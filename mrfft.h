#ifndef MRFFT_H
#define MRFFT

#include "Arduino.h"
#include "AudioStream_F32.h"
#include "arm_math.h"

// windows.c
extern "C" {
extern const int16_t AudioWindowHanning256[];
extern const int16_t AudioWindowBartlett256[];
extern const int16_t AudioWindowBlackman256[];
extern const int16_t AudioWindowFlattop256[];
extern const int16_t AudioWindowBlackmanHarris256[];
extern const int16_t AudioWindowNuttall256[];
extern const int16_t AudioWindowBlackmanNuttall256[];
extern const int16_t AudioWindowWelch256[];
extern const int16_t AudioWindowHamming256[];
extern const int16_t AudioWindowCosine256[];
extern const int16_t AudioWindowTukey256[];
}

#define FFT_SIZE 256
#define DOWNSAMPLE_FILTER_LEN 64

class AudioAnalyzeFFT256MR_F32 : public AudioStream_F32
{
public:
  AudioAnalyzeFFT256MR_F32() : AudioStream_F32(1, inputQueueArray),
    window(AudioWindowHanning256), count(0), outputflag(false) {
    arm_cfft_radix4_init_f32(&fft_inst, 256, 0, 1);
    memset(input, 0, sizeof(input));
    memset(input4, 0, sizeof(input));
    memset(input16, 0, sizeof(input));
    naverage = 1;
  }
  bool available() {
    if (outputflag == true) {
      outputflag = false;
      return true;
    }
    return false;
  }
  void averageTogether(uint8_t n) {
    if (n == 0) n = 1;
    naverage = n;
    count = 0;
  }
  void windowFunction(const int16_t *w) {
    window = w;
  }
  virtual void update(void);
  float32_t output4[128] __attribute__ ((aligned (4)));
  float32_t output16[128] __attribute__ ((aligned (4)));
private:
  // most recent 64 samples from the last block plus the samples from this block
  float32_t input[DOWNSAMPLE_FILTER_LEN + AUDIO_BLOCK_SAMPLES];
  // input downsampled 4 times
  float32_t input4[FFT_SIZE] __attribute__ ((aligned (4)));
  // input downsampled 16 times
  float32_t input16[FFT_SIZE] __attribute__ ((aligned (4)));

  const int16_t *window;  
  float32_t buffer[FFT_SIZE * 2] __attribute__ ((aligned (4)));
  float32_t sum4[FFT_SIZE / 2];
  float32_t sum16[FFT_SIZE / 2];
  uint8_t naverage;
  uint8_t count;
  volatile bool outputflag;
  audio_block_f32_t *inputQueueArray[1];
  arm_cfft_radix4_instance_f32 fft_inst;
};

#endif
