#include <perf/benchmark/benchmark.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    // The same seed at each run gives the same input. Thus two runs of the
    // benchmark compare with each other.
    srand(1);

    benchmark_report_header();

    run_core_benchmark();
    run_matrix_benchmark();
    run_vector_benchmark();
    run_linalg_benchmark();
    run_cspline_benchmark();
    run_interpolate_benchmark();
    run_transform_benchmark();
    run_filter_benchmark();
    run_movavg_benchmark();
    run_kalman_benchmark();
    run_estimate_benchmark();
    run_detect_benchmark();
    run_emd_benchmark();
    run_imf_benchmark();
    run_util_benchmark();

    benchmark_report_footer();

    // A BENCHMARK THAT MEASURED NOTHING MUST NOT SAY ALL IS WELL. Every time
    // this table printed before the clock was mended was 0.000, and this
    // program gave 0 to the shell all the same, thus the build ran it and went
    // green on it for as long as it existed.
    if(benchmark_measured_above_zero() == 0u)
    {
        printf("\nEvery measurement came out as zero. The clock is not being "
               "read properly.\n");
        return 1;
    }

    return 0;
}
