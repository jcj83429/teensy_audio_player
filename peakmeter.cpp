#include "peakmeter.h"

void AudioPeakHold::update(void)
{
	audio_block_t *block;
	const int16_t *p, *end;

	block = receiveReadOnly();
	if (!block) {
		return;
	}
	
	// shift the old blockPeak
	for(int i = PEAK_HOLD_BLOCKS - 1; i > 0; i--){
		blockPeak[i] = blockPeak[i-1];
	}
	
	uint16_t thisBlockPeak = 0;
	
	p = block->data;
	end = p + AUDIO_BLOCK_SAMPLES;
	do {
		int32_t d=*p++;
    d = abs(d);
		// TODO: can we speed this up with SSUB16 and SEL
		// http://www.m4-unleashed.com/parallel-comparison/
		if(d > thisBlockPeak){
			thisBlockPeak = d;
		}
	} while (p < end);

	blockPeak[0] = thisBlockPeak;
	
	peak = 0;
	for(int i = 0; i < PEAK_HOLD_BLOCKS; i++){
		if(blockPeak[i] > peak){
			peak = blockPeak[i];
		}
	}
	
	release(block);
}

 
