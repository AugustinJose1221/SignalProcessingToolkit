#define _POSIX_C_SOURCE 199309L

#include <ffitt/core/real.h>
#include <perf/benchmark/benchmark.h>

#include <stdio.h>
#include <time.h>

// A TIME IS A double HERE AND NEVER A real_t, AND EVERY TIME THIS TABLE EVER
// PRINTED BEFORE THIS WAS ZERO.
//
// Built for a float, real_t holds about 7 digits. This clock counts seconds
// since 1970, which is already 10 digits. Thus both readings rounded to the
// same number, every difference came out as exactly zero, and every line of
// the table said 0.000 microseconds and 0 per second. The build ran the
// benchmark and went green on it, because a program that measures nothing
// still finishes and still gives 0 to the shell.
double benchmark_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (double)now.tv_sec + ((double)now.tv_nsec / 1000000000.0);
}

static uint32_t measured_above_zero = 0u;

uint32_t benchmark_measured_above_zero(void)
{
    return measured_above_zero;
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
    real_t mean_seconds = result->total_seconds / (real_t)result->repeats;
    real_t per_second = REAL_C(0.0);

    if(result->best_seconds > REAL_C(0.0))
    {
        per_second = REAL_C(1.0) / result->best_seconds;
        measured_above_zero++;
    }

    printf("%-10s %-22s %8u %8u %14.3f %14.3f %14.0f\n",
           result->group,
           result->operation,
           result->size,
           result->repeats,
           (double)result->best_seconds * 1000000.0,
           (double)mean_seconds * 1000000.0,
           (double)per_second);
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
