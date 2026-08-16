#ifndef __CONFORMATION_SUPPORT_H__
#define __CONFORMATION_SUPPORT_H__

#include <string.h>
#include <stdio.h>

extern int testCasesRun;
extern int testCasesPassed;
extern int testCasesFailed;

#define CONFORMATION_TEST_CASE(exp, msg) do\
                                     {\
                                        testCasesRun++;\
                                        if(exp)\
                                        {\
                                            testCasesPassed++;\
                                            printf("Test %d [%s]:\tTest Passed\n", testCasesRun, msg);\
                                        }\
                                        else\
                                        {\
                                            testCasesFailed++;\
                                            printf("Test %d [%s]:\tTest Failed\n", testCasesRun, msg);\
                                        }\
                                     }while(0)\

#define CONFORMATION_TEST_SUMMARY() do\
                                     {\
                                        printf("\n----------------------------\n");\
                                        printf("Total Test Cases Run: %d\n", testCasesRun);\
                                        printf("Total Test Cases Passed: %d\n", testCasesPassed);\
                                        printf("Total Test Cases Failed: %d\n", testCasesFailed);\
                                        printf("----------------------------\n");\
                                    }while(0)\

#define VALUE_CHECK_EQUAL_CASE(a,b, msg) CONFORMATION_TEST_CASE((a) == (b), msg)
#define FLAG_CHECK_TRUE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == true, msg)
#define FLAG_CHECK_FALSE_CASE(a, msg) CONFORMATION_TEST_CASE((a) == false, msg)

void support_init();

#endif//__CONFORMATION_SUPPORT_H__