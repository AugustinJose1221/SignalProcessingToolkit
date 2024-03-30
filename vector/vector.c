#include <vector/vector.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

vector_t vector_alloc(uint32_t size)
{
    assert(size > 0);

    vector_t vector;

    vector.size = size;
    vector.data = (float*)(sizeof(float)*size);

    return vector;
}

vector_t vector_static_alloc(uint32_t size, uint32_t* mempool)
{
    assert(size > 0);
    assert(mempool != NULL);

    vector_t vector;
    vector.size = size;
    vector.data = (float*)mempool;

    return vector;
}

void vector_add_point_at_index(vector_t* vector, uint32_t index, float data)
{
    assert(vector != NULL);
    assert(index >= 0 && index < vector->size);

    vector->data[index] = data;
}

void vector_add_from_array(vector_t* vector, uint32_t size, float* data)
{
    assert(vector != NULL);
    assert(size == vector->size);
    assert(data != NULL);

    memcpy(vector->data, data, sizeof(float)*size);
}

void vector_printf(vector_t* vector, int (*func)(const char *, ...))
{
    assert(vector != NULL);
    for(int index = 0; index < vector->size; index++)
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
    assert(vector != NULL);
    assert(index >= 0 && index < vector->size);

    return vector->data[index];
}

float vector_dot_product(vector_t* x, vector_t* y)
{
    assert(x != NULL);
    assert(y != NULL);
    assert(x->size == y->size);

    float product = 0;
    for(int index = 0; index < x->size; index++)
    {
        product += x->data[index]*y->data[index];
    }

    return product;
}

float vector_norm(vector_t* x)
{
    return sqrt(vector_dot_product(x, x));
}