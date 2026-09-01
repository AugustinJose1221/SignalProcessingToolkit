#include <perf/conformation/support.h>
#include <stdlib.h>
#include <perf/conformation/matrix/matrix.h>
#include <perf/conformation/vector/vector.h>
#include <perf/conformation/fft/fft.h>

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
    run_fft_conformation_tests();
}

int main(void)
{
    // THE SAME NUMBERS AT EVERY RUN, ON PURPOSE.
    //
    // Some of these tests are given random signals. A test that draws a
    // different signal at every run cannot be looked at again after it fails,
    // and a build that fails once and passes the next time teaches nothing.
    //
    // support_init seeds from the clock and nothing calls it, thus the stream
    // was already the one that rand gives with no seed at all. Saying so here
    // makes it a choice rather than an oversight.
    srand(1u);

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
