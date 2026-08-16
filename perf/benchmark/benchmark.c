#define _POSIX_C_SOURCE 199309L

#include <perf/benchmark/benchmark.h>

#include <stdio.h>
#include <time.h>

double benchmark_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (double)now.tv_sec + ((double)now.tv_nsec / 1000000000.0);
}

void benchmark_report_header(void)
{
    printf("%-10s %-22s %8s %8s %14s %14s %14s\n",
           "MODULE", "OPERATION", "SIZE", "REPEATS",
           "BEST [us]", "MEAN [us]", "PER SECOND");
    printf("---------------------------------------------------------"
           "----------------------------------------\n");
}

void benchmark_report(benchmark_result_t* result)
{
    double mean_seconds = result->total_seconds / (double)result->repeats;
    double per_second = 0.0;

    if(result->best_seconds > 0.0)
    {
        per_second = 1.0 / result->best_seconds;
    }

    printf("%-10s %-22s %8u %8u %14.3f %14.3f %14.0f\n",
           result->group,
           result->operation,
           result->size,
           result->repeats,
           result->best_seconds * 1000000.0,
           mean_seconds * 1000000.0,
           per_second);
}

void benchmark_report_footer(void)
{
    printf("---------------------------------------------------------"
           "----------------------------------------\n");
    printf("BEST is the time of the fastest repeat. MEAN is the time of all "
           "the repeats together,\n");
    printf("divided by the number of repeats. PER SECOND says how many times "
           "the operation runs\n");
    printf("in one second at the speed of the fastest repeat.\n");
}
