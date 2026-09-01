#include <perf/conformation/support.h>
#include <perf/conformation/matrix/matrix.h>
#include <perf/conformation/vector/vector.h>

int test_cases_run = 0;
int test_cases_passed = 0;
int test_cases_failed = 0;

static void run_static_conformation_tests(void)
{
    run_matrix_static_conformation_tests();
    run_vector_static_conformation_tests();
}

static void run_dynamic_conformation_tests(void)
{
    run_matrix_dynamic_conformation_tests();
    run_vector_dynamic_conformation_tests();
}

int main(void)
{
    run_static_conformation_tests();
    run_dynamic_conformation_tests();
    CONFORMATION_TEST_SUMMARY();

    // THE EXIT CODE MUST SAY WHAT THE SUMMARY SAYS.
    //
    // This returned 0 whatever happened. The suite printed "Total Test Cases
    // Failed: 3" and told the shell that all was well, thus a build that ran
    // it would have gone green with three tests failing inside it.
    return (test_cases_failed == 0) ? 0 : 1;
}
