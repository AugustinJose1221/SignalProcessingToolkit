#ifndef TEST 
#include <vector2d/vector2d.h>
#else
#include "vector2d.h"
#endif//TEST

vector_t vector2d_alloc()
{
    return vector_alloc(2);
}

vector_t vector2d_static_alloc(uint32_t *mempool)
{
    return vector_static_alloc(2, mempool);
}

void vector2d_add_point_at_index(vector_t* vector, uint32_t index, float data)
{
    vector_add_point_at_index(vector, index, data);
}

void vector2d_add_from_array(vector_t* vector, float* data)
{
    vector_add_from_array(vector, 2, data);
}

void vector2d_printf(vector_t* vector, int (*func)(const char *, ...))
{
    vector_printf(vector, func);
}

float vector2d_get(vector_t* vector, uint32_t index)
{
    return vector_get(vector, index);
}

float vector2d_dot_product(vector_t* x, vector_t* y)
{
    return vector_dot_product(x, y);
}

float vector2d_norm(vector_t* x)
{
    return vector_norm(x);
}