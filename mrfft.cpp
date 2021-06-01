#include "mrfft.h"
#include "utility/dspinst.h"

// Minimum phase lowpass filter
// pass band: 0 to nyquist * 0.2, ripple 0.3dB.
// stop band: nyquiest * 0.3 to nyquist, attenuation 96dB.
// 
// The filter is created by designing a linear phase filter on t-filter.engineerjs.com
// with 200dB stop band attenuation and converting it to minimum phase with scipy's
// scipy.signal.minimum_phase.
float32_t downsampleFilter[DOWNSAMPLE_FILTER_LEN] = {
  0.00018571448384269954,
  0.0010696817283257156,
  0.003767338351592549,
  0.010097349911909342,
  0.02237684352184839,
  0.042726548002427575,
  0.07192652852553824,
  0.10814655820774877,
  0.14613679334373858,
  0.1775235026120112,
  0.19258316931788663,
  0.18328251437531817,
  0.14667703132610463,
  0.08729208992282445,
  0.017183733110449795,
  -0.04689703101642827,
  -0.08863872564861636,
  -0.09786853748033258,
  -0.07454828572830405,
  -0.02922985323401633,
  0.020537985149332836,
  0.05664970874667319,
  0.06720945946220722,
  0.050828807607402125,
  0.01663609920859967,
  -0.019988537232582683,
  -0.04406880310859916,
  -0.047163199203292844,
  -0.030353360250059504,
  -0.0029559228955885622,
  0.022215839485356895,
  0.03471075539463166,
  0.030731024198308338,
  0.014072248442750778,
  -0.006402552273297229,
  -0.021274177127499246,
  -0.02481212164150601,
  -0.017053098398579956,
  -0.0030204231325733546,
  0.010205670793796503,
  0.016947060179066623,
  0.015259957626919906,
  0.007232023669436401,
  -0.002588631212708767,
  -0.009623347666294354,
  -0.011301727824695094,
  -0.007846280634281312,
  -0.001667918822135158,
  0.00409702345988173,
  0.007082645995964171,
  0.006544818357812169,
  0.003325415869615498,
  -0.0008560757032006886,
  -0.004313452224150361,
  -0.006058382906913892,
  -0.005994823359003984,
  -0.004690040229561189,
  -0.002945725297631964,
  -0.0014155587791365013,
  -0.0004175510746568555,
  4.719868783091641e-05,
  0.0001548081558555851,
  0.00010698283155988254,
  3.921167458049065e-05,
};

static void copy_to_fft_buffer(void *destination, const void *source)
{
  const float32_t *src = (const float32_t *)source;
  float32_t *dst = (float32_t *)destination;

  for (int i=0; i < 256; i++) {
    *dst++ = *src++;  // real sample plus a zero for imaginary
    *dst++ = 0;
  }
}

static void apply_window_to_fft_buffer(void *buffer, const void *window)
{
  float32_t *buf = (float32_t *)buffer;
  const int16_t *win = (int16_t *)window;;

  for (int i=0; i < 256; i++) {
    *buf *= *win++;
    *buf /= 32768;
    buf += 2;
  }
}

void AudioAnalyzeFFT256MR_F32::update(void)
{
  audio_block_f32_t *block;

  block = receiveReadOnly_f32();
  if (!block) return;

  // shift input array
  for(int i = 0; i < DOWNSAMPLE_FILTER_LEN; i++){
    input[i] = input[AUDIO_BLOCK_SAMPLES + i];
  }
  // copy block data into input array
  for(int i = 0; i < AUDIO_BLOCK_SAMPLES; i++){
    input[DOWNSAMPLE_FILTER_LEN + i] = block->data[i];
  }

  // shift input4 array
  for(int i = 0; i < FFT_SIZE - AUDIO_BLOCK_SAMPLES / 4; i++){
    input4[i] = input4[i + AUDIO_BLOCK_SAMPLES / 4];
  }
  // fill new samples into input4 array
  for(int i = 0; i < AUDIO_BLOCK_SAMPLES / 4; i++){
    int inputIdx = DOWNSAMPLE_FILTER_LEN + i * 4 - 1;
    float64_t outputSmp = 0;
    for(int j = 0; j < DOWNSAMPLE_FILTER_LEN; j++, inputIdx--){
      outputSmp += input[inputIdx] * downsampleFilter[j];
    }
    input4[FFT_SIZE - AUDIO_BLOCK_SAMPLES / 4 + i] = outputSmp;
  }

  // shift input16 array
  for(int i = 0; i < FFT_SIZE - AUDIO_BLOCK_SAMPLES / 16; i++){
    input16[i] = input16[i + AUDIO_BLOCK_SAMPLES / 16];
  }
  // fill new samples into input16 array
  for(int i = 0; i < AUDIO_BLOCK_SAMPLES / 16; i++){
    int inputIdx = FFT_SIZE - AUDIO_BLOCK_SAMPLES / 4 + i * 4 - 1;
    float64_t outputSmp = 0;
    for(int j = 0; j < DOWNSAMPLE_FILTER_LEN; j++, inputIdx--){
      outputSmp += input[inputIdx] * downsampleFilter[j];
    }
    input16[FFT_SIZE - AUDIO_BLOCK_SAMPLES / 16 + i] = outputSmp;
  }

  copy_to_fft_buffer(buffer, input4);
  if (window) apply_window_to_fft_buffer(buffer, window);
  arm_cfft_radix4_f32(&fft_inst, buffer);
  // G. Heinzel's paper says we're supposed to average the magnitude
  // squared, then do the square root at the end.
  if (count == 0) {
    for (int i=0; i < 128; i++) {
      sum4[i] = (buffer[i * 2] * buffer[i * 2] + buffer[i * 2 + 1] * buffer[i * 2 + 1]);
    }
  } else {
    for (int i=0; i < 128; i++) {
      sum4[i] += (buffer[i * 2] * buffer[i * 2] + buffer[i * 2 + 1] * buffer[i * 2 + 1]);
    }
  }

  copy_to_fft_buffer(buffer, input16);
  if (window) apply_window_to_fft_buffer(buffer, window);
  arm_cfft_radix4_f32(&fft_inst, buffer);
  // G. Heinzel's paper says we're supposed to average the magnitude
  // squared, then do the square root at the end.
  if (count == 0) {
    for (int i=0; i < 128; i++) {
      sum16[i] = (buffer[i * 2] * buffer[i * 2] + buffer[i * 2 + 1] * buffer[i * 2 + 1]);
    }
  } else {
    for (int i=0; i < 128; i++) {
      sum16[i] += (buffer[i * 2] * buffer[i * 2] + buffer[i * 2 + 1] * buffer[i * 2 + 1]);
    }
  }

  if (++count == naverage) {
    count = 0;
    for (int i=0; i < 128; i++) {
      output4[i] = sqrtf(sum4[i] / naverage) / 64; // I don't know why 64, but a full scale sine wave is 64.
      output16[i] = sqrtf(sum16[i] / naverage) / 64;
    }
    outputflag = true;
  }
  release(block);
}
