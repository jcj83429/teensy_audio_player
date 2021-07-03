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

extern float32_t downsampleFilter[DOWNSAMPLE_FILTER_LEN];

class AudioAnalyzeFFT256MR_F32 : public AudioStream_F32
{
public:
  AudioAnalyzeFFT256MR_F32() : AudioStream_F32(1, inputQueueArray),
    window(AudioWindowHanning256), count(0), outputflag(false) {
    arm_cfft_radix4_init_f32(&fft_inst, 256, 0, 1);
    memset(input4, 0, sizeof(input4));
    memset(input16, 0, sizeof(input16));
    naverage = 1;
    arm_fir_decimate_init_f32(&decimate4, DOWNSAMPLE_FILTER_LEN, 4, downsampleFilter, state4, AUDIO_BLOCK_SAMPLES);
    arm_fir_decimate_init_f32(&decimate16, DOWNSAMPLE_FILTER_LEN, 4, downsampleFilter, state16, AUDIO_BLOCK_SAMPLES / 4);
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
  float32_t output4[FFT_SIZE / 2] __attribute__ ((aligned (4)));
  float32_t output16[FFT_SIZE / 2] __attribute__ ((aligned (4)));
private:
  // input downsampled 4 times
  float32_t input4[FFT_SIZE] __attribute__ ((aligned (4)));
  // input downsampled 16 times
  float32_t input16[FFT_SIZE] __attribute__ ((aligned (4)));

  // buffers required by arm fir decimate
  float32_t state4[DOWNSAMPLE_FILTER_LEN + AUDIO_BLOCK_SAMPLES] __attribute__ ((aligned (4)));
  float32_t state16[DOWNSAMPLE_FILTER_LEN + AUDIO_BLOCK_SAMPLES / 4] __attribute__ ((aligned (4)));

  arm_fir_decimate_instance_f32 decimate4;
  arm_fir_decimate_instance_f32 decimate16;

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
