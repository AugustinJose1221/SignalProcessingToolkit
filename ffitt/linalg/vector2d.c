// This file is left out of the build when FFITT_NO_LINALG is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_LINALG

#ifndef TEST 
#include <ffitt/linalg/vector2d.h>
#else
#include "vector2d.h"
#endif//TEST

vector_t vector2d_alloc(void)
{
    return vector_alloc(2);
}

vector_t vector2d_static_alloc(real_t* mempool)
{
    return vector_static_alloc(2, mempool);
}

void vector2d_add_point_at_index(vector_t* vector, uint32_t index, real_t data)
{
    vector_add_point_at_index(vector, index, data);
}

void vector2d_add_from_array(vector_t* vector, real_t* data)
{
    vector_add_from_array(vector, 2, data);
}

void vector2d_printf(vector_t* vector, int (*func)(const char *, ...))
{
    vector_printf(vector, func);
}

real_t vector2d_get(vector_t* vector, uint32_t index)
{
    return vector_get(vector, index);
}

real_t vector2d_dot_product(vector_t* x, vector_t* y)
{
    return vector_dot_product(x, y);
}

real_t vector2d_norm(vector_t* x)
{
    return vector_norm(x);
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int vector2d_is_not_in_this_build_t;

#endif//FFITT_NO_LINALG
