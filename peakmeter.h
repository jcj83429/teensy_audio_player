#ifndef PEAKMETER_H
#define PEAKMETER

// Peak detector that returns the highest peak in the last 6 blocks (about 0.05s)
// based on AudioAnalyzePeak

#include "Arduino.h"
#include "AudioStream.h"

#define PEAK_HOLD_BLOCKS 16

class AudioPeakHold : public AudioStream
{
public:
	AudioPeakHold(void) : AudioStream(1, inputQueueArray) {
	}

	int16_t read(void) {
		return peak;
	}
  // since we are interested in the log of the RMS, we don't actually need to do the square root
  // because it's the same as a multiplying by a constant after the log
  float readMeanSq(void) {
    return meanSq;
  }

	virtual void update(void);
private:
	audio_block_t *inputQueueArray[1];
	int16_t blockPeak[PEAK_HOLD_BLOCKS]; // blockPeak[0] is the most recent
  uint64_t blockSqAccum[PEAK_HOLD_BLOCKS]; // sum of squared samples, [0] is the most recent
	int16_t peak;
  float meanSq;
};

#endif
 
