#ifndef VECTOR_H
#define VECTOR_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <point2d/point2d.h>
#include <common/callback.h>
#else
#include "point2d.h"
#include "callback.h"
#endif

// A vector of float values.
//
// Two functions give a vector. vector_alloc takes the memory from the heap,
// and the caller must give the vector to vector_free. vector_static_alloc
// takes memory that the caller holds, and vector_free then does nothing.
typedef struct{
    uint32_t size;              // The number of values
    float* data;                // The values
    bool dynamic_alloc;         // True if the memory comes from the heap
}vector_t;

// Give a vector that holds the given number of values. The memory comes from
// the heap, and the values hold nothing yet. Give the vector to vector_free
// when you no longer need it.
vector_t vector_alloc(uint32_t size);

// Give a vector that uses the memory at mempool. That memory must hold as many
// float values as the given size, and it must stay while the vector is in use.
// This function takes no memory from the heap.
vector_t vector_static_alloc(uint32_t size, float* mempool);

// Write a value into the vector at the given index. The index must be below
// the size of the vector.
void vector_add_point_at_index(vector_t* vector, uint32_t index, float data);

// Write the values of an array into the vector. The size must be the same as
// the size of the vector.
void vector_add_from_array(vector_t* vector, uint32_t size, float* data);

// Write the vector, one value for each line. Give NULL as the function to
// write with printf.
void vector_printf(vector_t* vector, print_t func);

// Give the value of the vector at the given index. The index must be below the
// size of the vector.
float vector_get(vector_t* vector, uint32_t index);

// Give the dot product of the two vectors, which is the sum of the products of
// the values at the same index. Both vectors must have the same size.
float vector_dot_product(vector_t* x, vector_t* y);

// Give the length of the vector, which is the square root of the dot product
// of the vector with itself. The result is never less than zero.
float vector_norm(vector_t* x);

// Release the memory of a vector that came from vector_alloc. This function
// does nothing for a vector that came from vector_static_alloc, thus a call
// for either kind is safe.
void vector_free(vector_t* vector);

#endif//VECTOR_H
