#ifndef __CONFORMATION_MATRIX_SUPPORT_H__
#define __CONFORMATION_MATRIX_SUPPORT_H__

#include <ffitt/linalg/matrix.h>
#include <gsl/gsl_matrix.h>

void support_init(void);
void support_fill_random_matrix_double(matrix_t *mat, gsl_matrix *gsl_mat, int rows, int cols, float min, float max);
void support_fill_random_matrix_single(matrix_t *mat, gsl_matrix_float *gsl_mat, int rows, int cols, float min, float max);
bool support_matrix_addition_check(int rows, int cols, float min, float max);
bool support_matrix_scalar_multiplication_check(int rows, int cols, float min, float max);
bool support_matrix_multiplication_check(int rows_a, int cols_a, int rows_b, int cols_b, float min, float max);
bool support_matrix_transpose_check(int rows, int cols, float min, float max);
bool support_matrix_inverse_check(int size, float min, float max);
bool support_matrix_determinant_check(int size, float min, float max);

#endif//__CONFORMATION_MATRIX_SUPPORT_H__