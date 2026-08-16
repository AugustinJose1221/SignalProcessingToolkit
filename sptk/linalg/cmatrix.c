#ifndef TEST
#include <sptk/linalg/cmatrix.h>
#include <sptk/core/defs.h>
#else
#include "cmatrix.h"
#include "defs.h"
#endif

// Allocation

cmatrix_t cmatrix_alloc(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    cmatrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = (cnum_t*)malloc(sizeof(cnum_t)*m*n);
    matrix.dynamic_alloc = true;

    return matrix;
}

cmatrix_t cmatrix_static_alloc(uint32_t m, uint32_t n, cnum_t* elem)
{
    ASSERT(m > 0);
    ASSERT(n > 0);
    ASSERT(elem != NULL);

    cmatrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = elem;
    matrix.dynamic_alloc = false;

    return matrix;
}

// Elements

void cmatrix_add_element(cmatrix_t* matrix, uint32_t i, uint32_t j, cnum_t value)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    matrix->elem[(i*matrix->n)+j] = value;
}

cnum_t cmatrix_get_element(cmatrix_t* matrix, uint32_t i, uint32_t j)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    return matrix->elem[(i*matrix->n)+j];
}

// Create

cmatrix_t cmatrix_create_unit_matrix(uint32_t size)
{
    ASSERT(size > 0);

    cmatrix_t matrix;

    matrix = cmatrix_alloc(size, size);
    cmatrix_set_unit(&matrix);

    return matrix;
}

cmatrix_t cmatrix_create_zero_matrix(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    cmatrix_t matrix;

    matrix = cmatrix_alloc(m, n);
    cmatrix_set_zero(&matrix);

    return matrix;
}

// Questions

bool cmatrix_is_equal(cmatrix_t* a, cmatrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    if(a->m != b->m || a->n != b->n)
    {
        return false;
    }

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < a->n; j++)
        {
            if(!cnum_is_equal(cmatrix_get_element(a, i, j), cmatrix_get_element(b, i, j)))
            {
                return false;
            }
        }
    }

    return true;
}

bool cmatrix_is_near(cmatrix_t* a, cmatrix_t* b, float tolerance)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    if(a->m != b->m || a->n != b->n)
    {
        return false;
    }

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < a->n; j++)
        {
            if(!cnum_is_near(cmatrix_get_element(a, i, j),
                             cmatrix_get_element(b, i, j), tolerance))
            {
                return false;
            }
        }
    }

    return true;
}

bool cmatrix_is_square(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    return matrix->m == matrix->n;
}

bool cmatrix_is_zero(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            if(!cnum_is_zero(cmatrix_get_element(matrix, i, j)))
            {
                return false;
            }
        }
    }

    return true;
}

bool cmatrix_is_unit(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(cmatrix_is_square(matrix));

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cnum_t expected = (i == j) ? cnum_one() : cnum_zero();
            if(!cnum_is_equal(cmatrix_get_element(matrix, i, j), expected))
            {
                return false;
            }
        }
    }

    return true;
}

bool cmatrix_is_multipliable(cmatrix_t* a, cmatrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    return a->n == b->m;
}

bool cmatrix_is_hermitian(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(!cmatrix_is_square(matrix))
    {
        return false;
    }

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cnum_t value = cmatrix_get_element(matrix, i, j);
            cnum_t mirror = cnum_conjugate(cmatrix_get_element(matrix, j, i));
            if(!cnum_is_equal(value, mirror))
            {
                return false;
            }
        }
    }

    return true;
}

// Arithmetic

cmatrix_t cmatrix_add(cmatrix_t* a, cmatrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    cmatrix_t sum;

    sum = cmatrix_alloc(a->m, a->n);
    cmatrix_add_into(a, b, &sum);

    return sum;
}

cmatrix_t cmatrix_subtract(cmatrix_t* a, cmatrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    cmatrix_t difference;

    difference = cmatrix_alloc(a->m, a->n);
    cmatrix_subtract_into(a, b, &difference);

    return difference;
}

cmatrix_t cmatrix_multiply(cmatrix_t* a, cmatrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(cmatrix_is_multipliable(a, b));

    cmatrix_t product;

    product = cmatrix_alloc(a->m, b->n);
    cmatrix_multiply_into(a, b, &product);

    return product;
}

cmatrix_t cmatrix_multiply_scalar(cmatrix_t* matrix, cnum_t scalar)
{
    ASSERT(matrix != NULL);

    cmatrix_t product;

    product = cmatrix_alloc(matrix->m, matrix->n);
    cmatrix_multiply_scalar_into(matrix, scalar, &product);

    return product;
}

cmatrix_t cmatrix_transpose(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    cmatrix_t transpose;

    transpose = cmatrix_alloc(matrix->n, matrix->m);
    cmatrix_transpose_into(matrix, &transpose);

    return transpose;
}

cmatrix_t cmatrix_conjugate_transpose(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    cmatrix_t transpose;

    transpose = cmatrix_alloc(matrix->n, matrix->m);
    cmatrix_conjugate_transpose_into(matrix, &transpose);

    return transpose;
}

cnum_t cmatrix_trace(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(cmatrix_is_square(matrix));

    cnum_t trace = cnum_zero();

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        trace = cnum_add(trace, cmatrix_get_element(matrix, i, i));
    }

    return trace;
}

cnum_t cmatrix_determinant(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(cmatrix_is_square(matrix));

    cmatrix_t scratch = cmatrix_alloc(matrix->m, matrix->n);
    cnum_t determinant = cmatrix_determinant_into(matrix, &scratch);

    cmatrix_free(&scratch);

    return determinant;
}

cmatrix_t cmatrix_inverse(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(cmatrix_is_square(matrix));

    cmatrix_t inverse = cmatrix_alloc(matrix->m, matrix->n);
    cmatrix_t scratch = cmatrix_alloc(matrix->m, 2*matrix->n);

    if(!cmatrix_inverse_into(matrix, &inverse, &scratch))
    {
        // A singular matrix has no inverse. The zero matrix is the signal for
        // that state, as in the matrix module.
        cmatrix_set_zero(&inverse);
    }

    cmatrix_free(&scratch);

    return inverse;
}

void cmatrix_copy(cmatrix_t* src, cmatrix_t* dest)
{
    ASSERT(src != NULL);
    ASSERT(dest != NULL);
    ASSERT(src->m == dest->m);
    ASSERT(src->n == dest->n);

    for(uint32_t i = 0; i < src->m; i++)
    {
        for(uint32_t j = 0; j < src->n; j++)
        {
            cmatrix_add_element(dest, i, j, cmatrix_get_element(src, i, j));
        }
    }
}

void cmatrix_printf(cmatrix_t* matrix, print_t func)
{
    ASSERT(matrix != NULL);

    print_t print_func;

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cnum_t value = cmatrix_get_element(matrix, i, j);
            if(value.im < 0.0f)
            {
                print_func("%f - %fi\t", value.re, -value.im);
            }
            else
            {
                print_func("%f + %fi\t", value.re, value.im);
            }
        }
        print_func("\n");
    }
}

void cmatrix_free(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(matrix->dynamic_alloc)
    {
        free(matrix->elem);
        matrix->dynamic_alloc = false;
    }
}

// Operations that write into a matrix that already holds memory

void cmatrix_add_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(dest != NULL);
    ASSERT(a->m == b->m && a->n == b->n);
    ASSERT(dest->m == a->m && dest->n == a->n);

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < a->n; j++)
        {
            cmatrix_add_element(dest, i, j, cnum_add(cmatrix_get_element(a, i, j),
                                                     cmatrix_get_element(b, i, j)));
        }
    }
}

void cmatrix_subtract_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(dest != NULL);
    ASSERT(a->m == b->m && a->n == b->n);
    ASSERT(dest->m == a->m && dest->n == a->n);

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < a->n; j++)
        {
            cmatrix_add_element(dest, i, j, cnum_subtract(cmatrix_get_element(a, i, j),
                                                          cmatrix_get_element(b, i, j)));
        }
    }
}

void cmatrix_multiply_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(dest != NULL);
    ASSERT(a->n == b->m);
    ASSERT(dest->m == a->m && dest->n == b->n);

    cnum_t sum;

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < b->n; j++)
        {
            sum = cnum_zero();
            for(uint32_t k = 0; k < a->n; k++)
            {
                sum = cnum_add(sum, cnum_multiply(cmatrix_get_element(a, i, k),
                                                  cmatrix_get_element(b, k, j)));
            }
            cmatrix_add_element(dest, i, j, sum);
        }
    }
}

void cmatrix_multiply_scalar_into(cmatrix_t* matrix, cnum_t scalar, cmatrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->m && dest->n == matrix->n);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cmatrix_add_element(dest, i, j,
                                cnum_multiply(cmatrix_get_element(matrix, i, j), scalar));
        }
    }
}

void cmatrix_transpose_into(cmatrix_t* matrix, cmatrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->n);
    ASSERT(dest->n == matrix->m);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cmatrix_add_element(dest, j, i, cmatrix_get_element(matrix, i, j));
        }
    }
}

void cmatrix_conjugate_transpose_into(cmatrix_t* matrix, cmatrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->n);
    ASSERT(dest->n == matrix->m);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cmatrix_add_element(dest, j, i,
                                cnum_conjugate(cmatrix_get_element(matrix, i, j)));
        }
    }
}

void cmatrix_set_unit(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(cmatrix_is_square(matrix));

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cmatrix_add_element(matrix, i, j, (i == j) ? cnum_one() : cnum_zero());
        }
    }
}

void cmatrix_set_zero(cmatrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            cmatrix_add_element(matrix, i, j, cnum_zero());
        }
    }
}

// Give the row below the given row that holds the largest element of the given
// column. The elimination then takes that row as the pivot row. This keeps the
// division stable, and it moves a zero out of the pivot position.
static uint32_t cmatrix_get_pivot_row(cmatrix_t* matrix, uint32_t column, uint32_t from_row)
{
    uint32_t pivot_row = from_row;
    float best = cnum_magnitude_squared(cmatrix_get_element(matrix, from_row, column));

    for(uint32_t k = from_row + 1; k < matrix->m; k++)
    {
        float candidate = cnum_magnitude_squared(cmatrix_get_element(matrix, k, column));
        if(candidate > best)
        {
            best = candidate;
            pivot_row = k;
        }
    }

    return pivot_row;
}

static void cmatrix_exchange_rows(cmatrix_t* matrix, uint32_t first, uint32_t second)
{
    for(uint32_t j = 0; j < matrix->n; j++)
    {
        cnum_t value = cmatrix_get_element(matrix, first, j);
        cmatrix_add_element(matrix, first, j, cmatrix_get_element(matrix, second, j));
        cmatrix_add_element(matrix, second, j, value);
    }
}

cnum_t cmatrix_determinant_into(cmatrix_t* matrix, cmatrix_t* scratch)
{
    ASSERT(matrix != NULL);
    ASSERT(scratch != NULL);
    ASSERT(cmatrix_is_square(matrix));
    ASSERT(scratch->m == matrix->m && scratch->n == matrix->n);

    uint32_t n = matrix->m;
    cnum_t determinant = cnum_one();

    cmatrix_copy(matrix, scratch);

    // The elimination makes the matrix upper triangular. The determinant is
    // then the product of the elements on the diagonal. Each exchange of two
    // rows changes the sign of the determinant.
    for(uint32_t i = 0; i < n; i++)
    {
        uint32_t pivot_row = cmatrix_get_pivot_row(scratch, i, i);

        if(pivot_row != i)
        {
            cmatrix_exchange_rows(scratch, i, pivot_row);
            determinant = cnum_negate(determinant);
        }

        cnum_t pivot = cmatrix_get_element(scratch, i, i);

        if(cnum_is_zero(pivot))
        {
            return cnum_zero();
        }

        determinant = cnum_multiply(determinant, pivot);

        for(uint32_t k = i + 1; k < n; k++)
        {
            cnum_t factor = cnum_divide(cmatrix_get_element(scratch, k, i), pivot);

            for(uint32_t j = i; j < n; j++)
            {
                cnum_t value = cnum_subtract(cmatrix_get_element(scratch, k, j),
                                             cnum_multiply(factor,
                                                           cmatrix_get_element(scratch, i, j)));
                cmatrix_add_element(scratch, k, j, value);
            }
        }
    }

    return determinant;
}

bool cmatrix_inverse_into(cmatrix_t* matrix, cmatrix_t* dest, cmatrix_t* scratch)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(scratch != NULL);
    ASSERT(cmatrix_is_square(matrix));
    ASSERT(dest->m == matrix->m && dest->n == matrix->n);
    ASSERT(scratch->m == matrix->m && scratch->n == 2*matrix->n);

    uint32_t n = matrix->m;

    for(uint32_t i = 0; i < n; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            cmatrix_add_element(scratch, i, j, cmatrix_get_element(matrix, i, j));
            cmatrix_add_element(scratch, i, j + n, (i == j) ? cnum_one() : cnum_zero());
        }
    }

    for(uint32_t i = 0; i < n; i++)
    {
        uint32_t pivot_row = cmatrix_get_pivot_row(scratch, i, i);

        if(pivot_row != i)
        {
            cmatrix_exchange_rows(scratch, i, pivot_row);
        }

        cnum_t pivot = cmatrix_get_element(scratch, i, i);

        if(cnum_is_zero(pivot))
        {
            return false;
        }

        for(uint32_t j = 0; j < 2*n; j++)
        {
            cmatrix_add_element(scratch, i, j,
                                cnum_divide(cmatrix_get_element(scratch, i, j), pivot));
        }

        for(uint32_t k = 0; k < n; k++)
        {
            if(k != i)
            {
                cnum_t factor = cmatrix_get_element(scratch, k, i);

                for(uint32_t j = 0; j < 2*n; j++)
                {
                    cnum_t value = cnum_subtract(cmatrix_get_element(scratch, k, j),
                                                 cnum_multiply(factor,
                                                               cmatrix_get_element(scratch, i, j)));
                    cmatrix_add_element(scratch, k, j, value);
                }
            }
        }
    }

    for(uint32_t i = 0; i < n; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            cmatrix_add_element(dest, i, j, cmatrix_get_element(scratch, i, j + n));
        }
    }

    return true;
}
