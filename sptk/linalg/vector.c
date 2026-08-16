#ifndef TEST
#include <sptk/linalg/vector.h>
#include <sptk/core/defs.h>
#else
#include "vector.h"
#include "defs.h"
#endif
#include <math.h>
#include <string.h>

vector_t vector_alloc(uint32_t size)
{
    ASSERT(size > 0);

    vector_t vector;

    vector.size = size;
    vector.data = (float*)malloc(sizeof(float)*size);
    vector.dynamic_alloc = true;

    return vector;
}

vector_t vector_static_alloc(uint32_t size, float* mempool)
{
    ASSERT(size > 0);
    ASSERT(mempool != NULL);

    vector_t vector;
    vector.size = size;
    vector.data = mempool;
    vector.dynamic_alloc = false;

    return vector;
}

void vector_add_point_at_index(vector_t* vector, uint32_t index, float data)
{
    ASSERT(vector != NULL);
    ASSERT(index < vector->size);

    vector->data[index] = data;
}

void vector_add_from_array(vector_t* vector, uint32_t size, float* data)
{
    ASSERT(vector != NULL);
    ASSERT(size == vector->size);
    ASSERT(data != NULL);

    memcpy(vector->data, data, sizeof(float)*size);
}

void vector_printf(vector_t* vector, print_t func)
{
    ASSERT(vector != NULL);
    for(uint32_t index = 0; index < vector->size; index++)
    {
        if(func != NULL)
        {
            func("%f\n", (float)vector->data[index]);
        }
        else
        {
            printf("%f\n", (float)vector->data[index]);
        }
    }
}

float vector_get(vector_t* vector, uint32_t index)
{
    ASSERT(vector != NULL);
    ASSERT(index < vector->size);

    return vector->data[index];
}

float vector_dot_product(vector_t* x, vector_t* y)
{
    ASSERT(x != NULL);
    ASSERT(y != NULL);
    ASSERT(x->size == y->size);

    float product = 0;
    for(uint32_t index = 0; index < x->size; index++)
    {
        product += x->data[index]*y->data[index];
    }

    return product;
}

float vector_norm(vector_t* x)
{
    return (float)sqrt(vector_dot_product(x, x));
}

void vector_free(vector_t* vector)
{
    ASSERT(vector != NULL);
    ASSERT(vector->data != NULL);

    if(vector->dynamic_alloc)
    {
        free(vector->data);
        vector->dynamic_alloc = false;
    }
}