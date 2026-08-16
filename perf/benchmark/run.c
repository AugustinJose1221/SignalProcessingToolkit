#include <perf/benchmark/benchmark.h>

#include <stdlib.h>
#include <time.h>

int main(void)
{
    // The same seed at each run gives the same input. Thus two runs of the
    // benchmark compare with each other.
    srand(1);

    benchmark_report_header();

    run_matrix_benchmark();
    run_vector_benchmark();
    run_cspline_benchmark();
    run_kalman_benchmark();
    run_emd_benchmark();

    benchmark_report_footer();

    return 0;
}
