// This file is left out of the build when FFITT_NO_LINALG is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_LINALG

#ifndef TEST
#include <ffitt/linalg/pmatrix.h>
#include <ffitt/core/defs.h>
#else
#include "pmatrix.h"
#include "defs.h"
#endif

pmatrix_t pmatrix_alloc(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    pmatrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = (pmatrix_function_t*)malloc(sizeof(pmatrix_function_t)*m*n);
    matrix.dynamic_alloc = true;

    // The clearing below walks every place, thus it must not be reached with
    // nothing to walk.
    if(matrix.elem == NULL)
    {
        matrix.m = 0;
        matrix.n = 0;
        matrix.dynamic_alloc = false;

        return matrix;
    }

    pmatrix_set_zero(&matrix);

    return matrix;
}

pmatrix_t pmatrix_static_alloc(uint32_t m, uint32_t n, pmatrix_function_t* elem)
{
    ASSERT(m > 0);
    ASSERT(n > 0);
    ASSERT(elem != NULL);

    pmatrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = elem;
    matrix.dynamic_alloc = false;

    pmatrix_set_zero(&matrix);

    return matrix;
}

void pmatrix_add_element(pmatrix_t* matrix, uint32_t i, uint32_t j,
                         pmatrix_function_t function)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    matrix->elem[(i*matrix->n)+j] = function;
}

pmatrix_function_t pmatrix_get_element(pmatrix_t* matrix, uint32_t i, uint32_t j)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    return matrix->elem[(i*matrix->n)+j];
}

void pmatrix_set_zero(pmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix->elem[(i*matrix->n)+j] = NULL;
        }
    }
}

real_t pmatrix_evaluate_element(pmatrix_t* matrix, uint32_t i, uint32_t j, real_t x)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    pmatrix_function_t function = matrix->elem[(i*matrix->n)+j];

    if(function == NULL)
    {
        return REAL_C(0.0);
    }

    return function(x);
}

matrix_t pmatrix_evaluate(pmatrix_t* matrix, real_t x)
{
    ASSERT(matrix != NULL);

    matrix_t values;

    values = matrix_alloc(matrix->m, matrix->n);
    pmatrix_evaluate_into(matrix, x, &values);

    return values;
}

void pmatrix_evaluate_into(pmatrix_t* matrix, real_t x, matrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->m);
    ASSERT(dest->n == matrix->n);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix_add_element(dest, i, j, pmatrix_evaluate_element(matrix, i, j, x));
        }
    }
}

real_t pmatrix_zero(real_t x)
{
    (void)x;

    return REAL_C(0.0);
}

real_t pmatrix_one(real_t x)
{
    (void)x;

    return REAL_C(1.0);
}

void pmatrix_free(pmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(matrix->dynamic_alloc)
    {
        free(matrix->elem);
        matrix->dynamic_alloc = false;
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int pmatrix_is_not_in_this_build_t;

#endif//FFITT_NO_LINALG
