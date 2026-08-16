#ifndef CMATRIX_H
#define CMATRIX_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <cnum/cnum.h>
#include <common/callback.h>
#else
#include "cnum.h"
#include "callback.h"
#endif

// A matrix of complex numbers.
//
// This module is a separate module and not a part of the matrix module. A
// matrix of complex numbers holds another type of element, thus every
// operation needs another calculation. One module for both types would need a
// second copy of each function, or a union in the structure and a check of the
// type in each loop. Both make the matrix module larger and slower for the
// user who only needs real numbers, and that user is the common one on a small
// target.
//
// The two modules give the same names for the same operations, thus a user who
// knows the matrix module knows this module as well.

typedef struct{
    uint32_t m;
    uint32_t n;
    cnum_t *elem;
    bool dynamic_alloc;
}cmatrix_t;

cmatrix_t cmatrix_alloc(uint32_t m, uint32_t n);
cmatrix_t cmatrix_static_alloc(uint32_t m, uint32_t n, cnum_t* elem);

void cmatrix_add_element(cmatrix_t* matrix, uint32_t i, uint32_t j, cnum_t value);
cnum_t cmatrix_get_element(cmatrix_t* matrix, uint32_t i, uint32_t j);

cmatrix_t cmatrix_create_unit_matrix(uint32_t size);
cmatrix_t cmatrix_create_zero_matrix(uint32_t m, uint32_t n);

bool cmatrix_is_equal(cmatrix_t* a, cmatrix_t* b);
bool cmatrix_is_near(cmatrix_t* a, cmatrix_t* b, float tolerance);
bool cmatrix_is_square(cmatrix_t* matrix);
bool cmatrix_is_zero(cmatrix_t* matrix);
bool cmatrix_is_unit(cmatrix_t* matrix);
bool cmatrix_is_multipliable(cmatrix_t* a, cmatrix_t* b);

// True if the matrix does not change when the conjugate transpose is taken.
// Such a matrix is Hermitian, and its values on the diagonal are all real.
bool cmatrix_is_hermitian(cmatrix_t* matrix);

cmatrix_t cmatrix_add(cmatrix_t* a, cmatrix_t* b);
cmatrix_t cmatrix_subtract(cmatrix_t* a, cmatrix_t* b);
cmatrix_t cmatrix_multiply(cmatrix_t* a, cmatrix_t* b);
cmatrix_t cmatrix_multiply_scalar(cmatrix_t* matrix, cnum_t scalar);
cmatrix_t cmatrix_transpose(cmatrix_t* matrix);

// The transpose where each element becomes its conjugate. This operation takes
// the place of the transpose for a matrix of complex numbers.
cmatrix_t cmatrix_conjugate_transpose(cmatrix_t* matrix);

cnum_t cmatrix_trace(cmatrix_t* matrix);

// The elimination needs a copy of the matrix, thus these two functions get
// memory. Use the forms below on a target with no heap.
cnum_t cmatrix_determinant(cmatrix_t* matrix);
cmatrix_t cmatrix_inverse(cmatrix_t* matrix);

void cmatrix_copy(cmatrix_t* src, cmatrix_t* dest);
void cmatrix_printf(cmatrix_t* matrix, print_t func);
void cmatrix_free(cmatrix_t* matrix);

// Operations that write into a matrix that already holds memory.
//
// The destination must have the correct order, and it must not be one of the
// sources. These operations get no memory, thus code that must not use the
// heap can call them.
void cmatrix_add_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
void cmatrix_subtract_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
void cmatrix_multiply_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
void cmatrix_multiply_scalar_into(cmatrix_t* matrix, cnum_t scalar, cmatrix_t* dest);
void cmatrix_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
void cmatrix_conjugate_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
void cmatrix_set_unit(cmatrix_t* matrix);
void cmatrix_set_zero(cmatrix_t* matrix);

// The elimination writes its steps into the scratch matrix, which must have
// the order n x n. The scratch matrix loses its content.
cnum_t cmatrix_determinant_into(cmatrix_t* matrix, cmatrix_t* scratch);

// The scratch matrix must have the order n x 2n. The function gives false if
// the matrix is singular, and it does not change the destination then.
bool cmatrix_inverse_into(cmatrix_t* matrix, cmatrix_t* dest, cmatrix_t* scratch);

#endif//CMATRIX_H
