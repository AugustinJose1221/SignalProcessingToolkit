#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// A matrix of float values.
//
// The elements lie in one block, one row after the other. Thus the element at
// the row i and the column j lies at the position (i*n)+j.
//
// Two functions give a matrix. matrix_alloc takes the memory from the heap,
// and the caller must give the matrix to matrix_free. matrix_static_alloc
// takes memory that the caller holds, and matrix_free then does nothing. The
// member dynamic_alloc says which of the two made the matrix.
//
// Every operation that gives a new matrix takes memory from the heap. On a
// target with no heap, use the operations at the end of this file, which write
// into a matrix that the caller holds.
typedef struct{
    uint32_t m;                 // The number of rows
    uint32_t n;                 // The number of columns
    real_t *elem;                // The elements, one row after the other
    bool dynamic_alloc;         // True if the memory comes from the heap
}matrix_t;

// Give a matrix with m rows and n columns. The memory comes from the heap, and
// the elements hold no value yet. Give the matrix to matrix_free when you no
// longer need it.
matrix_t matrix_alloc(uint32_t m, uint32_t n);

// Give a matrix with m rows and n columns that uses the memory at elem. That
// memory must hold m*n float values, and it must stay while the matrix is in
// use. This function takes no memory from the heap.
matrix_t matrix_static_alloc(uint32_t m, uint32_t n, real_t* elem);

// Write a value into the matrix at the row i and the column j.
void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, real_t value);

// Give the value of the matrix at the row i and the column j.
real_t matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j);

// Give a new matrix with one row that holds the given row of the matrix. Give
// the result to matrix_free.
matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index);

// Give a new matrix with one column that holds the given column of the matrix.
// Give the result to matrix_free.
matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index);

// Give a new matrix with one row and two columns. The first element is the
// number of rows of the matrix, and the second element is the number of
// columns. Give the result to matrix_free.
matrix_t matrix_get_order(matrix_t* matrix);

// Give the sum of the elements on the diagonal. The matrix must be square.
real_t matrix_trace(matrix_t* matrix);

// Give the determinant of the matrix. The matrix must be square.
//
// The calculation uses the rule of the cofactors. The cost of that rule grows
// with the factorial of the order, thus a matrix of a large order takes a long
// time. Keep the order below 10.
real_t matrix_determinant(matrix_t* matrix);

// Give a new square matrix that holds 1 on the diagonal and 0 at every other
// place. Give the result to matrix_free.
matrix_t matrix_create_unit_matrix(uint32_t size);

// Give a new matrix that holds 0 at every place. Give the result to
// matrix_free.
matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n);

// True if the two matrices have the same order and the same value at every
// place.
bool matrix_is_equal(matrix_t* a, matrix_t* b);

// True if the matrix has as many rows as columns.
bool matrix_is_square(matrix_t* matrix);

// True if every element of the matrix is 0.
bool matrix_is_zero(matrix_t* matrix);

// True if the matrix holds 1 on the diagonal and 0 at every other place. The
// matrix must be square.
bool matrix_is_unit(matrix_t* matrix);

// True if the first matrix can multiply the second one. That asks for as many
// columns in the first matrix as rows in the second one.
bool matrix_is_multipliable(matrix_t* a, matrix_t* b);

// Give a new matrix that holds the sum of the two matrices. Both matrices must
// have the same order. Give the result to matrix_free.
matrix_t matrix_add(matrix_t* a, matrix_t* b);

// Give a new matrix that holds the first matrix less the second one. Both
// matrices must have the same order. Give the result to matrix_free.
matrix_t matrix_subtract(matrix_t* a, matrix_t* b);

// Give a new matrix where each element is the element of the given matrix
// multiplied by the scalar. Give the result to matrix_free.
matrix_t matrix_multiply_scalar(matrix_t* matrix, real_t scalar);

// Give a new matrix that holds the product of the two matrices. The first
// matrix must have as many columns as the second one has rows. The result has
// as many rows as the first matrix and as many columns as the second one. Give
// the result to matrix_free.
matrix_t matrix_multiply(matrix_t* a, matrix_t* b);

// Give a new matrix where the rows of the given matrix are the columns. Give
// the result to matrix_free.
matrix_t matrix_transpose(matrix_t* matrix);

// Give a new matrix that is the inverse of the given matrix. The matrix must
// be square.
//
// The elimination uses a partial pivot, thus a zero on the diagonal does not
// stop it. If the matrix is singular it has no inverse, and the function gives
// a matrix that holds 0 at every place. Use matrix_is_zero on the result to
// find that state. Give the result to matrix_free.
matrix_t matrix_inverse(matrix_t* matrix);

// True if the matrix is symmetric: every element equals the one across the
// diagonal from it, within the given tolerance.
//
// A tolerance is needed because a matrix that a chain of arithmetic has built
// is symmetric in principle and not always in its last digits.
bool matrix_is_symmetric(matrix_t* matrix, real_t tolerance);

// Give the factor of Cholesky of the matrix, which is the lower triangle L for
// which L times its own transpose gives the matrix back.
//
// WHAT THIS IS FOR
//
// A covariance matrix says how far a set of numbers spreads and how their
// spreads lean on each other. Its factor is the SHAPE of that spread: a set of
// directions, each as long as the spread reaches that way. Multiply a step of
// unit length by the factor and the step lands on the edge of the spread,
// whichever way it points.
//
// That is what the unscented Kalman filter needs to place its points, and it
// is what turns a set of unrelated random numbers into a set that spreads the
// way a given covariance says.
//
// It is also the fast way to solve a set of equations whose matrix is a
// covariance: about half the work of a general elimination, because it uses
// the symmetry instead of ignoring it.
//
// WHEN IT DOES NOT EXIST
//
// Only a symmetric matrix that is positive definite has a factor. Positive
// definite means the spread it describes is real: no direction in which the
// spread is zero or, worse, negative. A covariance that has been worked out by
// a long chain of arithmetic can lose that, and when it does the failure is
// the first sign that something upstream has gone wrong.
//
// The function gives a matrix of all zeros when there is no factor. Use
// matrix_is_zero on the result to find that state. Give the result to
// matrix_free.
matrix_t matrix_cholesky(matrix_t* matrix);

// Write the elements of the source into the destination. Both matrices must
// have the same order.
void matrix_copy(matrix_t* src, matrix_t* dest);

// Write the matrix, one row for each line. Give NULL as the function to write
// with printf.
void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...));

// Release the memory of a matrix that came from matrix_alloc. This function
// does nothing for a matrix that came from matrix_static_alloc, thus a call
// for either kind is safe. A second call does nothing.
void matrix_free(matrix_t* matrix);

// Operations that write into a matrix that already holds memory.
//
// The operations above make a new matrix for each result. Code that must not
// use the heap cannot call them. These operations write into a destination
// that the caller gives, thus they get no memory.
//
// The destination must have the correct order, and it must not be one of the
// sources.

// Write the sum of the two matrices into the destination. All three matrices
// must have the same order.
void matrix_add_into(matrix_t* a, matrix_t* b, matrix_t* dest);

// Write the first matrix less the second one into the destination. All three
// matrices must have the same order.
void matrix_subtract_into(matrix_t* a, matrix_t* b, matrix_t* dest);

// Write the product of the two matrices into the destination. The destination
// must have as many rows as the first matrix and as many columns as the second
// one.
void matrix_multiply_into(matrix_t* a, matrix_t* b, matrix_t* dest);

// Write each element of the matrix multiplied by the scalar into the
// destination. Both matrices must have the same order.
void matrix_multiply_scalar_into(matrix_t* matrix, real_t scalar, matrix_t* dest);

// Write the transpose of the matrix into the destination. The destination must
// have as many rows as the matrix has columns, and as many columns as the
// matrix has rows.
void matrix_transpose_into(matrix_t* matrix, matrix_t* dest);

// Write 1 on the diagonal of the matrix and 0 at every other place. The matrix
// must be square.
void matrix_set_unit(matrix_t* matrix);

// Write 0 into every element of the matrix.
void matrix_set_zero(matrix_t* matrix);

// Write the inverse of the matrix into the destination. The matrix must be
// square, and the destination must have the same order.
//
// The scratch matrix must have the order n x 2n, where n is the order of the
// matrix. It loses its content. The function gives false if the matrix is
// singular, and it does not change the destination then.
bool matrix_inverse_into(matrix_t* matrix, matrix_t* dest, matrix_t* scratch);

// Write the factor of Cholesky of the matrix into the destination. Both
// matrices must be square and of the same order.
//
// The function gives false when the matrix has no factor, which is when it is
// not symmetric or not positive definite. The destination is left as it was.
//
// The destination may be the matrix itself. Working in place is safe here,
// because each element of the factor is worked out from elements that are
// already finished and from the one place of the matrix that it replaces.
bool matrix_cholesky_into(matrix_t* matrix, matrix_t* dest);

#endif//MATRIX_H
