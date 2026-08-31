#ifndef PMATRIX_H
#define PMATRIX_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif

// A matrix with a parameter, for example:
//
//     [ sin(x)  cos(x) ]
//     [   0       1    ]
//
// Each element is a pointer to a function of the parameter. To use the matrix,
// give a value for the parameter. The matrix then gives a matrix of float
// values, which every other module of the library can take.
//
// Why a pointer to a function, and not an expression that the module reads
// from text:
//
// A module that reads an expression from text must hold a tree of operations.
// Such a tree needs memory while the program runs, and it needs a parser. Both
// go against the way this library works, because the library must run on a
// target with no heap. A pointer to a function needs no memory while the
// program runs, and the compiler makes the code for the expression. The
// library already uses a pointer to a function for the print callback, thus
// this way fits the library.
//
// The cost is one pointer for each element, where a matrix of float values
// holds one float for each element. On a small target a pointer is often the
// same size as a float or two times that size. Keep a parameter matrix small,
// and give the value of the parameter one time for each step of the
// calculation.
//
// A function of the standard library that takes a float and gives a float,
// such as real_sin or real_cos, fits the type of an element directly.

typedef real_t (*pmatrix_function_t)(real_t x);

typedef struct{
    uint32_t m;
    uint32_t n;
    pmatrix_function_t *elem;
    bool dynamic_alloc;
}pmatrix_t;

// Give a parameter matrix with m rows and n columns. The memory comes from
// the heap, and every element holds zero. Give the matrix to pmatrix_free when
// you no longer need it.
pmatrix_t pmatrix_alloc(uint32_t m, uint32_t n);
// Give a parameter matrix that uses the memory at elem. That memory must hold
// m*n pointers to a function. Every element holds zero after the call. This
// function takes no memory from the heap.
pmatrix_t pmatrix_static_alloc(uint32_t m, uint32_t n, pmatrix_function_t* elem);

// An element that holds NULL gives the value zero. Thus a new matrix that
// pmatrix_set_zero cleared holds zero at every place, and a user who needs a
// zero at one place does not need a function for it.
void pmatrix_add_element(pmatrix_t* matrix, uint32_t i, uint32_t j,
                         pmatrix_function_t function);
// Give the function that stands at the row i and the column j. The result is
// NULL if that element holds zero.
pmatrix_function_t pmatrix_get_element(pmatrix_t* matrix, uint32_t i, uint32_t j);
// Write zero into every element of the matrix.
void pmatrix_set_zero(pmatrix_t* matrix);

// Give the value of one element for the given value of the parameter.
real_t pmatrix_evaluate_element(pmatrix_t* matrix, uint32_t i, uint32_t j, real_t x);

// Give a new matrix of float values for the given value of the parameter. This
// function gets memory. Use pmatrix_evaluate_into on a target with no heap.
matrix_t pmatrix_evaluate(pmatrix_t* matrix, real_t x);

// Write the values into a matrix that already holds memory. The destination
// must have the same order as the parameter matrix.
void pmatrix_evaluate_into(pmatrix_t* matrix, real_t x, matrix_t* dest);

// An element that always gives zero.
real_t pmatrix_zero(real_t x);
// An element that always gives one.
real_t pmatrix_one(real_t x);

// Release the memory of a matrix that came from pmatrix_alloc. This function
// does nothing for a matrix that came from pmatrix_static_alloc.
void pmatrix_free(pmatrix_t* matrix);

#endif//PMATRIX_H
