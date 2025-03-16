#ifndef __EMD_H__
#define __EMD_H__

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <cspline/cspline.h>
#include <imf/imf.h>
#else
#include "cspline.h"
#include "imf.h"
#endif
typedef struct{
    float* x;
    float* y;
    uint32_t size;
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    float* peak_buffer;
    float* peak_index_buffer;
    float* valley_buffer;
    float* valley_index_buffer;
    imf_t* imf;
    uint32_t imf_count;
    float* residue;
    float* working_buffer;
    bool dynamic_alloc;
}emd_t;

emd_t emd_alloc(uint32_t size);
emd_t emd_static_alloc(uint32_t size, float** membank, float** mempool, float* peak_buffer, float* valley_buffer);
void emd_initialize(emd_t* emd, uint32_t num_of_imf, imf_t* imf, float* x, float* y, float* residue, float* working_buffer, float* peak_index_buffer, float* valley_index_buffer);
imf_t* emd_get_imf(emd_t* emd, uint32_t imf_index, uint32_t stopping_threshold, uint32_t* status);
uint32_t emd_sift(emd_t* emd, uint32_t stopping_threshold);
void emd_free(emd_t emd);

#endif//__EMD_H__