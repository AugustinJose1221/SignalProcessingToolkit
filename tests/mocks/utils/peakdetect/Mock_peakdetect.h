#ifndef __MOCK__PEAKDETECT_H__
#define __MOCK__PEAKDETECT_H__

#ifdef TEST
uint32_t peakdetect_get_peaks(float* input, float* index_buffer, float* peak_buffer, uint32_t size);
#endif//TEST

#endif//__MOCK__PEAKDETECT_H__