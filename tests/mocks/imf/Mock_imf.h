#ifndef __MOCK_IMF_H__
#define __MOCK_IMF_H__

#ifdef TEST
imf_t imf_alloc(uint32_t size);
imf_t imf_static_alloc(uint32_t size, float* x, float* y);
void imf_printf(imf_t* imf, int (*func)(const char*, ...));
void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, int (*func)(const char*, ...));
void imf_free(imf_t imf);
#endif//TEST

#endif//__MOCK_IMF_H__