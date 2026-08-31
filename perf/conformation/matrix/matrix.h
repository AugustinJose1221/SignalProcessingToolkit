#ifndef __CONFORMATION_MATRIX_H__
#define __CONFORMATION_MATRIX_H__

#include <ffitt/linalg/matrix.h>
#include <perf/conformation/support.h>

#define MATRIX_INIT(mat, m, n, arr)  do\
                                     {\
                                         mat = matrix_alloc(m, n);\
                                         for(int i = 0; i < m; i++)\
                                         {\
                                             for(int j = 0; j < n; j++)\
                                             {\
                                                 matrix_add_element(&mat, i, j, (arr)[(i*n)+j]);\
                                             }\
                                         }\
                                     }while(0)\

#define MATRIX_PRINT(mat)       matrix_printf(&mat, NULL)
#define MATRIX_IS_EQUAL(a,b)    matrix_is_equal(&a, &b)
#define MATRIX_FREE(mat)        matrix_free(&mat)
#define MATRIX_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE(MATRIX_IS_EQUAL(a,b), msg)

void run_matrix_static_conformation_tests(void);
void run_matrix_dynamic_conformation_tests(void);

#endif//__CONFORMATION_MATRIX_H__