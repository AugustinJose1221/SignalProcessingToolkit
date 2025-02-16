#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <point2d/point2d.h>
#include <common/callback.h>
#else
#include "point2d.h"
#include "callback.h"
#endif

typedef struct{
    uint32_t size;
    float* data;
    bool dynamic_alloc;
}vector_t;

vector_t vector_alloc(uint32_t size);
vector_t vector_static_alloc(uint32_t size, uint32_t* mempool);
void vector_add_point_at_index(vector_t* vector, uint32_t index, float data);
void vector_add_from_array(vector_t* vector, uint32_t size, float* data);
void vector_printf(vector_t* vector, print_t func);
float vector_get(vector_t* vector, uint32_t index);
float vector_dot_product(vector_t* x, vector_t* y);
float vector_norm(vector_t* x);
void vector_free(vector_t* vector);

#endif//__VECTOR_H__