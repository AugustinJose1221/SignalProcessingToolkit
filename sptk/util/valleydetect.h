#ifndef VALLEYDETECT_H
#define VALLEYDETECT_H

#include <stdint.h>
#include <stdio.h>

// Find every valley of the signal and give the number of them.
//
// A valley is a sample that is smaller than the sample before it and smaller
// than the sample after it. Thus the first sample and the last sample are
// never valleys, and a signal with fewer than three samples holds no valley.
//
// The function writes the index of each valley into index_buffer and the value
// of each valley into valley_buffer. Both buffers must hold room for as many
// values as the signal holds.
uint32_t valleydetect_get_valley(float* input, float* index_buffer, float* valley_buffer, uint32_t size);

#endif//VALLEYDETECT_H
