#ifndef CONFORMATION_MATRIX_H
#define CONFORMATION_MATRIX_H

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
// Element by element, and near rather than exact. The library gives no
// matrix_is_near for a matrix of real numbers, and the freeze is not the time
// to add one, thus the comparison is made here.
static inline bool matrix_is_near_enough(matrix_t* a, matrix_t* b)
{
    if((a->m != b->m) || (a->n != b->n))
    {
        return false;
    }

    for(uint32_t i = 0; i < a->m; i++)
    {
        for(uint32_t j = 0; j < a->n; j++)
        {
            real_t left = matrix_get_element(a, i, j);
            real_t right = matrix_get_element(b, i, j);

            if(!CONFORMATION_IS_NEAR(left, right))
            {
                return false;
            }
        }
    }

    return true;
}

#define MATRIX_IS_EQUAL(a,b)    matrix_is_near_enough(&a, &b)
#define MATRIX_FREE(mat)        matrix_free(&mat)
#define MATRIX_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE(MATRIX_IS_EQUAL(a,b), msg)

void run_matrix_static_conformation_tests(void);
void run_matrix_dynamic_conformation_tests(void);

#endif//CONFORMATION_MATRIX_H