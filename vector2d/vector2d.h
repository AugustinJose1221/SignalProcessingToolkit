#ifndef VECTOR2D_H
#define VECTOR2D_H

#ifndef TEST
#include <vector/vector.h>
#else
#include "vector.h"
#endif//TEST

// A vector with two values.
//
// This module gives the operations of the vector module for a vector of the
// size 2, thus the caller does not give the size at each call. The result is a
// vector_t, and every function of the vector module takes it.

// Give a vector with two values. The memory comes from the heap. Give the
// vector to vector_free when you no longer need it.
vector_t vector2d_alloc();

// Give a vector with two values that uses the memory at mempool. That memory
// must hold two float values. This function takes no memory from the heap.
vector_t vector2d_static_alloc(float* mempool);

// Write a value into the vector at the given index. The index must be 0 or 1.
void vector2d_add_point_at_index(vector_t* vector, uint32_t index, float data);

// Write two values from an array into the vector.
void vector2d_add_from_array(vector_t* vector, float* data);

// Write the vector, one value for each line. Give NULL as the function to
// write with printf.
void vector2d_printf(vector_t* vector, int (*func)(const char *, ...));

// Give the value of the vector at the given index. The index must be 0 or 1.
float vector2d_get(vector_t* vector, uint32_t index);

// Give the dot product of the two vectors.
float vector2d_dot_product(vector_t* x, vector_t* y);

// Give the length of the vector.
float vector2d_norm(vector_t* x);

#endif//VECTOR2D_H
