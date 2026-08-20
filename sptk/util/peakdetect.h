#ifndef PEAKDETECT_H
#define PEAKDETECT_H

#include <stdint.h>
#include <stdio.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Find every peak of the signal and give the number of them.
//
// A peak is a sample that is larger than the sample before it and larger than
// the sample after it. Thus the first sample and the last sample are never
// peaks, and a signal with fewer than three samples holds no peak.
//
// The function writes the index of each peak into index_buffer and the value
// of each peak into peak_buffer. Both buffers must hold room for as many
// values as the signal holds.
uint32_t peakdetect_get_peaks(real_t* input, real_t* index_buffer, real_t* peak_buffer, uint32_t size);

#endif//PEAKDETECT_H
