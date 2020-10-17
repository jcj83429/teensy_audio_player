#ifndef PEAKMETER_H
#define PEAKMETER

// Peak detector that returns the highest peak in the last 4 blocks (about 0.01s)
// based on AudioAnalyzePeak

#include "Arduino.h"
#include "AudioStream.h"

#define PEAK_HOLD_BLOCKS 4

class AudioPeakHold : public AudioStream
{
public:
	AudioPeakHold(void) : AudioStream(1, inputQueueArray) {
	}

	int16_t read(void) {
		return peak;
	}

	virtual void update(void);
private:
	audio_block_t *inputQueueArray[1];
	int16_t blockPeak[PEAK_HOLD_BLOCKS]; // blockPeak[0] is the most recent
	int16_t peak;
};

#endif
 
