#ifndef __CONFORMATION_SUPPORT_H__
#define __CONFORMATION_SUPPORT_H__

#include <string.h>
#include <stdio.h>
#include <matrix/matrix.h>

#define CONFORMATION_TEST_CASE(exp, msg) do\
                                     {\
                                        if(exp)\
                                        {\
                                            printf("%s: Test Passed\n", msg);\
                                        }\
                                        else\
                                        {\
                                            printf("%s: Test Failed\n", msg);\
                                        }\
                                     }while(0)\


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

#define MATRIX_PRINT(mat)       matrix_printf(&mat, NULL);
#define MATRIX_IS_EQUAL(a,b)    matrix_is_equal(&a, &b)
#define MATRIX_FREE(mat)        matrix_free(&mat);

#define MATRIX_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE(MATRIX_IS_EQUAL(a,b), msg)
#define VALUE_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE((a) == (b), msg)

#endif//__CONFORMATION_SUPPORT_H__