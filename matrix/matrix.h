#ifndef __MATRIX_H__
#define __MATRIX_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct{
    uint32_t m;
    uint32_t n;
    float *elem;
    bool dynamic_alloc;
}matrix_t;

matrix_t matrix_alloc(uint32_t m, uint32_t n);
matrix_t matrix_static_alloc(uint32_t m, uint32_t n, float* elem);
void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, float value);
float matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j);
matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index);
matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index);
matrix_t matrix_get_order(matrix_t* matrix);
float matrix_trace(matrix_t* matrix);
float matrix_determinant(matrix_t* matrix);
matrix_t matrix_create_unit_matrix(uint32_t size);
matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n);
bool matrix_is_equal(matrix_t* a, matrix_t* b);
bool matrix_is_square(matrix_t* matrix);
bool matrix_is_zero(matrix_t* matrix);
bool matrix_is_unit(matrix_t* matrix);
bool matrix_is_multipliable(matrix_t* a, matrix_t* b);
matrix_t matrix_add(matrix_t* a, matrix_t* b);
matrix_t matrix_multiply_scalar(matrix_t* matrix, float scalar);
matrix_t matrix_multiply(matrix_t* a, matrix_t* b);
matrix_t matrix_transpose(matrix_t* matrix);
void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...));
void matrix_free(matrix_t* matrix);

#endif//__MATRIX_H__