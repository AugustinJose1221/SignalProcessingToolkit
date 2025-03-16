#ifndef __IMF_H__
#define __IMF_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <common/callback.h>
#else
#include "callback.h"
#endif

typedef struct{
    float* x;
    float* y;
    uint32_t size;
    bool dynamic_alloc;
}imf_t;

imf_t imf_alloc(uint32_t size);
imf_t imf_static_alloc(uint32_t size, float* x, float* y);
void imf_printf(imf_t* imf, print_t func);
void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, print_t func);
void imf_free(imf_t imf);

#endif//__IMF_H__