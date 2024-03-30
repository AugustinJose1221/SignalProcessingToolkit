#ifndef __CSPLINE_H__
#define __CSPLINE_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct{
    uint32_t size;
    float* x;
    float* y;
    float* b;
    float* c;
    float* d;
    bool dynamic_alloc;
}cspline_t;

typedef struct{
    float* dx;
    float* dp;
    float* d;
    float* b;
    float* q;
    bool dynamic_alloc;
}cspline_mempool_t;

cspline_t cspline_alloc(uint32_t size);
cspline_t cspline_static_alloc(uint32_t size, float** membank);
cspline_mempool_t cspline_alloc_mempool(uint32_t size);
cspline_mempool_t cspline_static_alloc_mempool(float** membank);
void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, float* x, float* y);
void cspline_update_size(cspline_t* cspline, uint32_t size);
float cspline_get_interpolated_point(cspline_t* cspline, float x);
void cspline_free(cspline_t cspline);
void cspline_free_mempool(cspline_mempool_t mempool);

#endif//__CSPLINE_H__