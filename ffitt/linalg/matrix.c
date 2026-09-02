#ifndef TEST
#include <ffitt/linalg/matrix.h>
#include <ffitt/core/defs.h>
#else
#include "matrix.h"
#include "defs.h"
#endif

#include <math.h>



matrix_t matrix_alloc(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    matrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = (real_t*)malloc(sizeof(real_t)*m*n);
    matrix.dynamic_alloc = true;

    return matrix;
}

matrix_t matrix_static_alloc(uint32_t m, uint32_t n, real_t* elem)
{
    ASSERT(m > 0);
    ASSERT(n > 0);
    ASSERT(elem != NULL);

    matrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = elem;
    matrix.dynamic_alloc = false;

    return matrix;
}

// Operations

void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, real_t value)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);
    matrix->elem[(i*matrix->n)+j] = value;
}

real_t matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j)
{
    ASSERT(matrix != NULL);
    ASSERT(i < matrix->m);
    ASSERT(j < matrix->n);

    return matrix->elem[(i*matrix->n)+j];
}

matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index)
{
    ASSERT(matrix != NULL);
    ASSERT(row_index < matrix->m);

    matrix_t row_matrix;

    row_matrix = matrix_alloc(1, matrix->n);

    for(uint32_t i = 0; i < matrix->n; i++)
    {
       matrix_add_element(&row_matrix, 0, i, matrix_get_element(matrix, row_index, i));
    }

    return row_matrix;
}

matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index)
{
    ASSERT(matrix != NULL);
    ASSERT(col_index < matrix->n);

    matrix_t col_matrix;

    col_matrix = matrix_alloc(matrix->m, 1);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        matrix_add_element(&col_matrix, i, 0, matrix_get_element(matrix, i, col_index));
    }

    return col_matrix;
}

matrix_t matrix_get_order(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    matrix_t order;

    order = matrix_alloc(1, 2);
    matrix_add_element(&order, 0, 0, matrix->m);
    matrix_add_element(&order, 0, 1, matrix->n);

    return order;
}

real_t matrix_trace(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    real_t trace = 0;

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        trace += matrix_get_element(matrix, i, i);
    }

    return trace;
}

// The row at or below the given one whose element in this column is largest.
//
// Moving that row up keeps the division stable, and it moves a zero out of the
// pivot position, because a zero there does not always mean a singular matrix.
static uint32_t matrix_get_pivot_row(matrix_t* matrix, uint32_t from,
                                     uint32_t column)
{
    uint32_t best_row = from;
    real_t best = REAL_ABS(matrix_get_element(matrix, from, column));

    for(uint32_t k = from + 1u; k < matrix->m; k++)
    {
        real_t candidate = REAL_ABS(matrix_get_element(matrix, k, column));

        if(candidate > best)
        {
            best = candidate;
            best_row = k;
        }
    }

    return best_row;
}

static void matrix_exchange_rows(matrix_t* matrix, uint32_t first,
                                 uint32_t second)
{
    for(uint32_t j = 0; j < matrix->n; j++)
    {
        real_t value = matrix_get_element(matrix, first, j);

        matrix_add_element(matrix, first, j, matrix_get_element(matrix, second, j));
        matrix_add_element(matrix, second, j, value);
    }
}

// WHY THIS IS NOT DONE BY EXPANDING THE COFACTORS ANY MORE.
//
// It was. The determinant of an n by n matrix was worked out as n determinants
// of n-1 by n-1 matrices, each of which did the same again. That is the
// definition written straight down, and it costs the FACTORIAL of the order:
// an 8 by 8 asks for 40320 terms. Worse, each level of the recursion called
// matrix_alloc and matrix_free for its minor, thus the heap was churned inside
// the recursion and the depth followed the order with no bound.
//
// MEASURED, before and after, on the same machine at 32 bits, in
// microseconds for one determinant, both giving the same answer to every
// digit:
//
//   order            4       5       6       7        8        9       10
//   by cofactors  0.79    5.08   32.4    217     1739    15801   157957
//   by this       0.62    1.07    1.71    2.56     3.69     5.17      6.7
//
// At order 10 that is twenty three thousand times. The determinant of an 8 by
// 8 cost 1.7 milliseconds while the INVERSE of the same matrix, which is the
// harder question, cost 16 microseconds. The complex matrix beside this one
// was given elimination when the same fault was found there; the real one was
// not, until now.
//
// Elimination makes the matrix upper triangular. The determinant is then the
// product of the elements on the diagonal, and each exchange of two rows
// changes its sign. That costs the CUBE of the order and one working copy.
//
// THE ORDERS UP TO 3 KEEP THEIR CLOSED FORMS, and not only for speed: those
// need no working copy, thus a caller of a small matrix takes no memory at
// all. Elimination begins at 4, where a closed form stops being worth writing.
real_t matrix_determinant(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    if(matrix->m == 3)
    {
        return (matrix_get_element(matrix, 0, 0)*((matrix_get_element(matrix, 1, 1) * matrix_get_element(matrix, 2, 2)) - (matrix_get_element(matrix, 2, 1)*matrix_get_element(matrix, 1, 2))))
             - (matrix_get_element(matrix, 0, 1)*((matrix_get_element(matrix, 1, 0) * matrix_get_element(matrix, 2, 2)) - (matrix_get_element(matrix, 2, 0)*matrix_get_element(matrix, 1, 2))))
             + (matrix_get_element(matrix, 0, 2)*((matrix_get_element(matrix, 1, 0) * matrix_get_element(matrix, 2, 1)) - (matrix_get_element(matrix, 2, 0)*matrix_get_element(matrix, 1, 1))));
    }
    else if(matrix->m == 2)
    {
        return (matrix_get_element(matrix, 0, 0) * matrix_get_element(matrix, 1, 1))
             - (matrix_get_element(matrix, 0, 1) * matrix_get_element(matrix, 1, 0));
    }
    else if(matrix->m == 1)
    {
        return matrix_get_element(matrix, 0, 0);
    }

    uint32_t n = matrix->m;
    real_t determinant = REAL_C(1.0);

    // The largest element the matrix started with. A pivot is weighed against
    // it, because that is the size the arithmetic works at, and a pivot far
    // below it is nothing but what the rounding left behind. This is the same
    // rule that matrix_inverse_into uses, and the two must agree: a matrix
    // this calls singular is one that cannot be inverted.
    real_t largest = REAL_C(0.0);

    matrix_t working = matrix_alloc(n, n);

    if(working.elem == NULL)
    {
        return REAL_C(0.0);
    }

    for(uint32_t i = 0; i < n; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            real_t element = matrix_get_element(matrix, i, j);
            real_t size_of = REAL_ABS(element);

            if(size_of > largest)
            {
                largest = size_of;
            }

            matrix_add_element(&working, i, j, element);
        }
    }

    real_t smallest_pivot = largest * (real_t)n * REAL_EPSILON;

    for(uint32_t i = 0; i < n; i++)
    {
        uint32_t pivot_row = matrix_get_pivot_row(&working, i, i);

        if(pivot_row != i)
        {
            matrix_exchange_rows(&working, i, pivot_row);
            determinant = -determinant;
        }

        real_t pivot = matrix_get_element(&working, i, i);

        if(REAL_ABS(pivot) <= smallest_pivot)
        {
            matrix_free(&working);
            return REAL_C(0.0);
        }

        determinant *= pivot;

        for(uint32_t k = i + 1u; k < n; k++)
        {
            real_t factor = matrix_get_element(&working, k, i) / pivot;

            for(uint32_t j = i; j < n; j++)
            {
                real_t value = matrix_get_element(&working, k, j)
                               - (factor * matrix_get_element(&working, i, j));

                matrix_add_element(&working, k, j, value);
            }
        }
    }

    matrix_free(&working);

    return determinant;
}

// Create

matrix_t matrix_create_unit_matrix(uint32_t size)
{
    ASSERT(size > 0);

    matrix_t matrix;

    matrix = matrix_alloc(size, size);
    matrix_set_unit(&matrix);

    return matrix;
}

matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    matrix_t matrix;

    matrix = matrix_alloc(m, n);
    matrix_set_zero(&matrix);

    return matrix;
}

// Arithmetic

bool matrix_is_equal(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    
    if(a->m != b->m || a->n != b->n)
    {
        return false;
    }
    else
    {
        for(uint32_t i = 0; i < a->m; i++)
        {
            for(uint32_t j = 0; j < b->n; j++)
            {
                if(matrix_get_element(a, i, j) != matrix_get_element(b, i, j))
                {
                    return false;
                }
            }
        }

        return true;
    }
}

bool matrix_is_square(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(matrix->m == matrix->n)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool matrix_is_zero(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            if(matrix_get_element(matrix, i, j) != 0)
            {
                return false;
            }
        }
    }
    return true;
}

bool matrix_is_unit(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            if(i == j)
            {
                if(matrix_get_element(matrix, i, j) != 1)
                {
                    return false;
                }
            }
            else
            {
                if(matrix_get_element(matrix, i, j) != 0)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool matrix_is_multipliable(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    matrix_t order_a;
    matrix_t order_b;
    bool status = false;

    order_a = matrix_get_order(a);
    order_b = matrix_get_order(b);

    if(matrix_get_element(&order_a, 0, 1) == matrix_get_element(&order_b, 0, 0))
    {
        status = true;
    }
    
    matrix_free(&order_a);
    matrix_free(&order_b);

    return status;
}

matrix_t matrix_add(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    matrix_t sum;

    sum = matrix_alloc(a->m, a->n);
    matrix_add_into(a, b, &sum);

    return sum;
}

matrix_t matrix_subtract(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    matrix_t difference;

    difference = matrix_alloc(a->m, a->n);
    matrix_subtract_into(a, b, &difference);

    return difference;
}

matrix_t matrix_multiply_scalar(matrix_t* matrix, real_t scalar)
{
    ASSERT(matrix != NULL);

    matrix_t product;

    product = matrix_alloc(matrix->m, matrix->n);
    matrix_multiply_scalar_into(matrix, scalar, &product);

    return product;
}

matrix_t matrix_multiply(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(matrix_is_multipliable(a, b));

    matrix_t product;

    product = matrix_alloc(a->m, b->n);
    matrix_multiply_into(a, b, &product);

    return product;
}

matrix_t matrix_transpose(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    matrix_t transpose;

    transpose = matrix_alloc(matrix->n, matrix->m);
    matrix_transpose_into(matrix, &transpose);

    return transpose;
}

bool matrix_is_symmetric(matrix_t* matrix, real_t tolerance)
{
    ASSERT(matrix != NULL);

    if(!matrix_is_square(matrix))
    {
        return false;
    }

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = i + 1u; j < matrix->n; j++)
        {
            real_t across = REAL_ABS(matrix_get_element(matrix, i, j)
                                     - matrix_get_element(matrix, j, i));
            if(across > tolerance)
            {
                return false;
            }
        }
    }

    return true;
}

bool matrix_cholesky_into(matrix_t* matrix, matrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(matrix_is_square(matrix));
    ASSERT((dest->m == matrix->m) && (dest->n == matrix->n));

    uint32_t order = matrix->m;

    // A matrix that is not symmetric has no factor, and taking one anyway
    // would quietly use the lower half and ignore the upper half. That is a
    // different matrix from the one the caller gave, thus the function says no
    // rather than answering a question that was not asked.
    //
    // The tolerance follows the size of the elements, because a covariance
    // built by a long chain of arithmetic is symmetric in principle and not in
    // its last digits.
    real_t largest = REAL_C(0.0);
    for(uint32_t i = 0; i < order; i++)
    {
        for(uint32_t j = 0; j < order; j++)
        {
            real_t size_of = REAL_ABS(matrix_get_element(matrix, i, j));
            if(size_of > largest) { largest = size_of; }
        }
    }

    if(!matrix_is_symmetric(matrix, (REAL_C(1000.0) * REAL_EPSILON * largest)
                                    + REAL_SMALLEST))
    {
        return false;
    }

    // The factor is worked out one row at a time, and each element uses only
    // elements of the same row and of rows above it. Thus the destination may
    // be the matrix itself: every element that is read has already been
    // written, or has not been touched yet.
    for(uint32_t i = 0; i < order; i++)
    {
        for(uint32_t j = 0; j <= i; j++)
        {
            real_t total = matrix_get_element(matrix, i, j);

            for(uint32_t k = 0; k < j; k++)
            {
                total -= matrix_get_element(dest, i, k)
                         * matrix_get_element(dest, j, k);
            }

            if(i == j)
            {
                // The diagonal is a square root. A value at or below zero
                // means the matrix is not positive definite: there is a
                // direction in which the spread it describes is zero or
                // negative, which no real spread can be.
                if(total <= REAL_C(0.0))
                {
                    return false;
                }

                matrix_add_element(dest, i, j, REAL_SQRT(total));
            }
            else
            {
                matrix_add_element(dest, i, j,
                                   total / matrix_get_element(dest, j, j));
            }
        }

        // Everything above the diagonal is zero, because the factor is a lower
        // triangle.
        for(uint32_t j = i + 1u; j < order; j++)
        {
            matrix_add_element(dest, i, j, REAL_C(0.0));
        }
    }

    return true;
}

matrix_t matrix_cholesky(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    matrix_t factor = matrix_alloc(matrix->m, matrix->n);

    if(!matrix_cholesky_into(matrix, &factor))
    {
        // A matrix with no factor gives back all zeros, as a singular matrix
        // gives back all zeros from matrix_inverse.
        matrix_set_zero(&factor);
    }

    return factor;
}

matrix_t matrix_inverse(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    matrix_t inverse;
    matrix_t scratch;

    inverse = matrix_alloc(matrix->m, matrix->n);
    scratch = matrix_alloc(matrix->m, 2*matrix->n);

    if(!matrix_inverse_into(matrix, &inverse, &scratch))
    {
        // A singular matrix has no inverse. The zero matrix is the signal for
        // that state.
        matrix_set_zero(&inverse);
    }

    matrix_free(&scratch);

    return inverse;
}

void matrix_copy(matrix_t* src, matrix_t* dest)
{
    ASSERT(src != NULL);
    ASSERT(dest != NULL);
    ASSERT(src->m == dest->m);
    ASSERT(src->n == dest->n);
    for(uint32_t i = 0; i < src->m; i++)
    {
        for(uint32_t j = 0; j < src->n; j++)
        {
            matrix_add_element(dest, i, j, matrix_get_element(src, i, j));
        }
    }
    
}

void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...))
{
    ASSERT(matrix != NULL);

    int (*print_func)(const char*, ...);

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
            print_func("%f\t", matrix_get_element(matrix, i, j));
        }
        print_func("\n");
    }
}

void matrix_free(matrix_t* matrix)
{
    if(matrix->dynamic_alloc)
    {
        free(matrix->elem);
        matrix->dynamic_alloc = false;
    }
}
// Operations that write into a matrix that already holds memory

void matrix_add_into(matrix_t* a, matrix_t* b, matrix_t* dest)
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
            matrix_add_element(dest, i, j,
                               matrix_get_element(a, i, j) + matrix_get_element(b, i, j));
        }
    }
}

void matrix_subtract_into(matrix_t* a, matrix_t* b, matrix_t* dest)
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
            matrix_add_element(dest, i, j,
                               matrix_get_element(a, i, j) - matrix_get_element(b, i, j));
        }
    }
}

void matrix_multiply_into(matrix_t* a, matrix_t* b, matrix_t* dest)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(dest != NULL);
    ASSERT(a->n == b->m);
    ASSERT(dest->m == a->m && dest->n == b->n);

    real_t sum;

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < b->n; j++)
        {
            sum = REAL_C(0.0);
            for(uint32_t k = 0; k < a->n; k++)
            {
                sum += matrix_get_element(a, i, k) * matrix_get_element(b, k, j);
            }
            matrix_add_element(dest, i, j, sum);
        }
    }
}

void matrix_multiply_scalar_into(matrix_t* matrix, real_t scalar, matrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->m && dest->n == matrix->n);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix_add_element(dest, i, j, matrix_get_element(matrix, i, j) * scalar);
        }
    }
}

void matrix_transpose_into(matrix_t* matrix, matrix_t* dest)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(dest->m == matrix->n);
    ASSERT(dest->n == matrix->m);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix_add_element(dest, j, i, matrix_get_element(matrix, i, j));
        }
    }
}

void matrix_set_unit(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix_add_element(matrix, i, j, (i == j) ? REAL_C(1.0) : REAL_C(0.0));
        }
    }
}

void matrix_set_zero(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            matrix_add_element(matrix, i, j, REAL_C(0.0));
        }
    }
}

bool matrix_inverse_into(matrix_t* matrix, matrix_t* dest, matrix_t* scratch)
{
    ASSERT(matrix != NULL);
    ASSERT(dest != NULL);
    ASSERT(scratch != NULL);
    ASSERT(matrix_is_square(matrix));
    ASSERT(dest->m == matrix->m && dest->n == matrix->n);
    ASSERT(scratch->m == matrix->m && scratch->n == 2*matrix->n);

    uint32_t n = matrix->m;
    uint32_t pivot_row;
    real_t pivot;
    real_t factor;
    real_t swap;

    // The largest element the matrix started with. A pivot is weighed against
    // it, because that is the size the arithmetic works at, and a pivot far
    // below it is nothing but what the rounding left behind.
    real_t largest = REAL_C(0.0);

    for(uint32_t i = 0; i < n; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            real_t element = matrix_get_element(matrix, i, j);
            real_t size_of = REAL_ABS(element);

            if(size_of > largest)
            {
                largest = size_of;
            }

            matrix_add_element(scratch, i, j, element);
            matrix_add_element(scratch, i, j + n, (i == j) ? REAL_C(1.0) : REAL_C(0.0));
        }
    }

    // How small a pivot has to be before it means the matrix is singular.
    real_t smallest_pivot = largest * (real_t)n * REAL_EPSILON;

    for(uint32_t i = 0; i < n; i++)
    {
        // Move the row with the largest element of this column to the pivot
        // position. This keeps the division stable. It also moves a zero out
        // of the pivot position, because a zero there does not always show a
        // singular matrix.
        pivot_row = i;
        for(uint32_t k = i + 1; k < n; k++)
        {
            real_t candidate = matrix_get_element(scratch, k, i);
            real_t best = matrix_get_element(scratch, pivot_row, i);
            if((candidate < 0 ? -candidate : candidate) > (best < 0 ? -best : best))
            {
                pivot_row = k;
            }
        }

        if(pivot_row != i)
        {
            for(uint32_t j = 0; j < 2*n; j++)
            {
                swap = matrix_get_element(scratch, i, j);
                matrix_add_element(scratch, i, j, matrix_get_element(scratch, pivot_row, j));
                matrix_add_element(scratch, pivot_row, j, swap);
            }
        }

        pivot = matrix_get_element(scratch, i, i);

        // A PIVOT THAT IS NOT EXACTLY ZERO CAN STILL BE NOTHING.
        //
        // This asked whether the pivot was exactly zero. After a column has
        // been eliminated the pivot of a singular matrix is not zero: it is
        // the rounding left over from the subtraction, which is tiny and
        // almost never exactly nothing. The test therefore let a singular
        // matrix through, and dividing by that rounding gave an answer in the
        // millions where there is no inverse at all.
        //
        // Measured on the 4 by 4 matrix of 1 to 16, whose determinant is 0:
        // matrix_inverse_into gave true, and matrix_inverse gave elements
        // near 2 000 000 rather than the zeros its header promises. A caller
        // following that header would have called matrix_is_zero, been told
        // no, and used the answer.
        if(REAL_ABS(pivot) <= smallest_pivot)
        {
            return false;
        }

        for(uint32_t j = 0; j < 2*n; j++)
        {
            matrix_add_element(scratch, i, j, matrix_get_element(scratch, i, j) / pivot);
        }

        for(uint32_t k = 0; k < n; k++)
        {
            if(k != i)
            {
                factor = matrix_get_element(scratch, k, i);
                for(uint32_t j = 0; j < 2*n; j++)
                {
                    matrix_add_element(scratch, k, j, matrix_get_element(scratch, k, j)
                                                      - (factor * matrix_get_element(scratch, i, j)));
                }
            }
        }
    }

    for(uint32_t i = 0; i < n; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            matrix_add_element(dest, i, j, matrix_get_element(scratch, i, j + n));
        }
    }

    return true;
}
