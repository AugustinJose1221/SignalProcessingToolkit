#ifndef __CONFORMATION_SUPPORT_H__
#define __CONFORMATION_SUPPORT_H__

#include <string.h>
#include <stdio.h>

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

#define VALUE_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE((a) == (b), msg)
#define FLAG_CHECK_TRUE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == true, msg)
#define FLAG_CHECK_FALSE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == false, msg)

void support_init();

#endif//__CONFORMATION_SUPPORT_H__
