#ifndef PMATRIX_H
#define PMATRIX_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <matrix/matrix.h>
#else
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
// such as sinf or cosf, fits the type of an element directly.

typedef float (*pmatrix_function_t)(float x);

typedef struct{
    uint32_t m;
    uint32_t n;
    pmatrix_function_t *elem;
    bool dynamic_alloc;
}pmatrix_t;

pmatrix_t pmatrix_alloc(uint32_t m, uint32_t n);
pmatrix_t pmatrix_static_alloc(uint32_t m, uint32_t n, pmatrix_function_t* elem);

// An element that holds NULL gives the value zero. Thus a new matrix that
// pmatrix_set_zero cleared holds zero at every place, and a user who needs a
// zero at one place does not need a function for it.
void pmatrix_add_element(pmatrix_t* matrix, uint32_t i, uint32_t j,
                         pmatrix_function_t function);
pmatrix_function_t pmatrix_get_element(pmatrix_t* matrix, uint32_t i, uint32_t j);
void pmatrix_set_zero(pmatrix_t* matrix);

// Give the value of one element for the given value of the parameter.
float pmatrix_evaluate_element(pmatrix_t* matrix, uint32_t i, uint32_t j, float x);

// Give a new matrix of float values for the given value of the parameter. This
// function gets memory. Use pmatrix_evaluate_into on a target with no heap.
matrix_t pmatrix_evaluate(pmatrix_t* matrix, float x);

// Write the values into a matrix that already holds memory. The destination
// must have the same order as the parameter matrix.
void pmatrix_evaluate_into(pmatrix_t* matrix, float x, matrix_t* dest);

// Two functions for an element that does not change with the parameter.
float pmatrix_zero(float x);
float pmatrix_one(float x);

void pmatrix_free(pmatrix_t* matrix);

#endif//PMATRIX_H
