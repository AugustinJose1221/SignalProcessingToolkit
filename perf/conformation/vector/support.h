#ifndef CONFORMATION_VECTOR_SUPPORT_H
#define CONFORMATION_VECTOR_SUPPORT_H

#include <ffitt/linalg/vector.h>
#include <gsl/gsl_vector_float.h>

void support_init(void);
void support_fill_random_vector_single(vector_t *vec, gsl_vector_float *gsl_vec, int size, float min, float max);
bool support_vector_dot_product_check(int size, float min, float max);
bool support_vector_norm_check(int size, float min, float max);

#endif//CONFORMATION_VECTOR_SUPPORT_H