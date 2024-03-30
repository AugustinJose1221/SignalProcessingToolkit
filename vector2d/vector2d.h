#ifndef __VECTOR2D_H__
#define __VECTOR2D_H__

#include <vector/vector.h>

vector_t vector2d_alloc();
vector_t vector2d_static_alloc(uint32_t *mempool);
void vector2d_add_point_at_index(vector_t* vector, uint32_t index, float data);
void vector2d_add_from_array(vector_t* vector, float* data);
void vector2d_printf(vector_t* vector, int (*func)(const char *, ...));
float vector2d_get(vector_t* vector, uint32_t index);
float vector2d_dot_product(vector_t* x, vector_t* y);
float vector2d_norm(vector_t* x);

#endif//__VECTOR2D_H__