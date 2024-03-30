#ifndef __VALLEYDETECT_H__
#define __VALLEYDETECT_H__

#include <stdint.h>
#include <stdio.h>

uint32_t valleydetect_get_valley(float* input, float* index_buffer, float* valley_buffer, uint32_t size);

#endif//__VALLEYDETECT_H__