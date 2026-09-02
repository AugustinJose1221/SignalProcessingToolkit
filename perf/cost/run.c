#include <perf/cost/cost.h>

#include <stdlib.h>

int main(void)
{
    // THE SAME NUMBERS AT EVERY RUN, ON PURPOSE. These tests are given random
    // signals. What a signal holds does not change what an operation costs,
    // but a run that cannot be repeated cannot be looked at after it fails.
    srand(1u);

    cost_report_header();

    run_ringbuf_cost_tests();
    run_resample_cost_tests();
    run_movavg_cost_tests();
    run_correlate_cost_tests();
    run_goertzel_cost_tests();
    run_convolve_cost_tests();

    cost_report_summary();

    // THE EXIT CODE MUST SAY WHAT THE REPORT SAYS.
    return (claims_broken == 0) ? 0 : 1;
}
