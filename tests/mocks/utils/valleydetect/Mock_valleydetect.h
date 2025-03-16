#ifndef __MOCK_VALLEYDETECT_H__
#define __MOCK_VALLEYDETECT_H__

#ifdef TEST
uint32_t valleydetect_get_valley(float* input, float* index_buffer, float* valley_buffer, uint32_t size);
#endif//TEST

#endif//__MOCK_VALLEYDETECT_H__