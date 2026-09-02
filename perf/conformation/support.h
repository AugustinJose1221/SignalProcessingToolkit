#ifndef __CONFORMATION_SUPPORT_H__
#define __CONFORMATION_SUPPORT_H__

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <ffitt/core/real.h>

// HOW CLOSE IS CLOSE ENOUGH, AND WHY NOTHING HERE ASKS FOR EXACTLY EQUAL.
//
// These tests compare a number the library worked out against a number the GNU
// Scientific Library worked out. The two take different roads to the same
// place, thus they round differently, and asking for the two to be equal to
// the last bit asks for something neither library promises.
//
// It was asked for. matrix_is_equal was used, which is exact, and the suite
// reported the inverse of a 3 by 3 as wrong because the answer was
// -24.0000248 where -24 was wanted. That is 1 part in a million, which is what
// a float holds.
//
// The reference of the GNU Scientific Library is held in single precision even
// where this library is built for double, thus the agreement cannot be closer
// than a float, whatever width this library was built at.
#define CONFORMATION_TOLERANCE      REAL_C(1.0e-4)

// True when two numbers agree to that much, measured against how large they
// are. A number near zero is compared as it stands, because a share of nothing
// is nothing.
// Both sides are brought to real_t first, so that a whole number written in a
// test does not reach a function that wants a floating point number.
#define CONFORMATION_IS_NEAR(a, b)  \
    (REAL_ABS((real_t)(a) - (real_t)(b)) <= \
        (CONFORMATION_TOLERANCE * (REAL_C(1.0) + REAL_ABS((real_t)(a)) \
                                                + REAL_ABS((real_t)(b)))))

extern int test_cases_run;
extern int test_cases_passed;
extern int test_cases_failed;

#define CONFORMATION_TEST_CASE(exp, msg) do\
                                     {\
                                        test_cases_run++;\
                                        if(exp)\
                                        {\
                                            test_cases_passed++;\
                                            printf("Test %d [%s]:\tTest Passed\n", test_cases_run, msg);\
                                        }\
                                        else\
                                        {\
                                            test_cases_failed++;\
                                            printf("Test %d [%s]:\tTest Failed\n", test_cases_run, msg);\
                                        }\
                                     }while(0)\

#define CONFORMATION_TEST_SUMMARY() do\
                                     {\
                                        printf("\n----------------------------\n");\
                                        printf("Total Test Cases Run: %d\n", test_cases_run);\
                                        printf("Total Test Cases Passed: %d\n", test_cases_passed);\
                                        printf("Total Test Cases Failed: %d\n", test_cases_failed);\
                                        printf("----------------------------\n");\
                                    }while(0)\

#define VALUE_CHECK_EQUAL_CASE(a,b, msg) \
    CONFORMATION_TEST_CASE(CONFORMATION_IS_NEAR(a, b), msg)
#define FLAG_CHECK_TRUE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == true, msg)
#define FLAG_CHECK_FALSE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == false, msg)

void support_init(void);

#endif//__CONFORMATION_SUPPORT_H__
