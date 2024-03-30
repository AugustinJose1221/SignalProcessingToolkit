#ifndef __PEAKDETECT_H__
#define __PEAKDETECT_H__

#include <stdint.h>
#include <stdio.h>

uint32_t peakdetect_get_peaks(float* input, float* index_buffer, float* peak_buffer, uint32_t size);

#endif//__PEAKDETECT_H__
