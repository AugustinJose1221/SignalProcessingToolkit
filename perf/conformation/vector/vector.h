#ifndef __CONFORMATION_VECTOR_H__
#define __CONFORMATION_VECTOR_H__

#include <vector/vector.h>
#include <perf/conformation/support.h>

#define VECTOR_INIT(vec, size, arr)  do\
                                      {\
                                          vec = vector_alloc(size);\
                                          for(int i = 0; i < size; i++)\
                                          {\
                                              vector_add_point_at_index(&vec, i, (arr)[i]);\
                                          }\
                                      }while(0)\

#define VECTOR_PRINT(vec)       vector_printf(&vec, NULL)
#define VECTOR_IS_EQUAL(a,b)    vector_is_equal(&a, &b)
#define VECTOR_FREE(vec)        vector_free(&vec)
#define VECTOR_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE(VECTOR_IS_EQUAL(a,b), msg)

void run_vector_static_conformation_tests(void);
void run_vector_dynamic_conformation_tests(void);

#endif//__CONFORMATION_VECTOR_H__