#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <ffitt/core/real.h>
#include <stdint.h>

// The benchmark measures how long an operation of the library takes. It uses
// the monotonic clock of the system, thus a change of the time of day does not
// disturb a measurement.
//
// The benchmark needs no external library.

typedef struct{
    const char* group;          // The module, for example matrix
    const char* operation;      // The operation, for example multiply
    uint32_t size;              // The size of the input
    uint32_t repeats;           // The number of times the operation ran
    real_t total_seconds;       // The time of all the repeats together
    real_t best_seconds;        // The time of the fastest repeat
}benchmark_result_t;

// The reading of the monotonic clock in seconds.
//
// A TIME IS A double HERE AND NEVER A real_t. Built for a float, real_t holds
// about 7 digits, and this clock counts seconds since 1970, which is already
// 10. Every reading then rounds to the same number.
double benchmark_now(void);

// How many of the measurements came out above nothing.
//
// THE GUARD AGAINST A CLOCK TOO NARROW TO READ. Every time this table ever
// printed before was 0.000, because the clock was read into a real_t. The
// program still finished and still gave 0 to the shell, thus the build ran it
// and went green on it. main asks this and gives 1 when nothing was measured,
// so that the same fault cannot come back unseen.
uint32_t benchmark_measured_above_zero(void);

// Write the head of the table.
void benchmark_report_header(void);

// Write one line of the table.
void benchmark_report(benchmark_result_t* result);

// Write the foot of the table.
void benchmark_report_footer(void);

// Measure one operation. The macro runs the given statement `repeat_count`
// times and keeps the total time and the time of the fastest repeat. The
// fastest repeat gives the best picture of the operation, because the other
// work of the system only makes a repeat slower.
//
// Each name inside the macro starts with benchmark_, so that a name of the
// caller and a name of the macro cannot be the same.
#define BENCHMARK_MEASURE(group_name, operation_name, input_size, repeat_count, statement) \
    do                                                                                    \
    {                                                                                     \
        benchmark_result_t benchmark_measurement;                                         \
        double benchmark_start;                                                           \
        double benchmark_elapsed;                                                         \
                                                                                          \
        benchmark_measurement.group = (group_name);                                       \
        benchmark_measurement.operation = (operation_name);                               \
        benchmark_measurement.size = (input_size);                                        \
        benchmark_measurement.repeats = (repeat_count);                                   \
        benchmark_measurement.total_seconds = REAL_C(0.0);                                \
        benchmark_measurement.best_seconds = REAL_C(0.0);                                 \
                                                                                          \
        for(uint32_t benchmark_repeat = 0;                                                \
            benchmark_repeat < (repeat_count);                                            \
            benchmark_repeat++)                                                           \
        {                                                                                 \
            benchmark_start = benchmark_now();                                            \
            statement;                                                                    \
            benchmark_elapsed = benchmark_now() - benchmark_start;                        \
            benchmark_measurement.total_seconds += (real_t)benchmark_elapsed;             \
            if((benchmark_repeat == 0u)                                                   \
               || (benchmark_elapsed < (double)benchmark_measurement.best_seconds))       \
            {                                                                             \
                benchmark_measurement.best_seconds = (real_t)benchmark_elapsed;            \
            }                                                                             \
        }                                                                                 \
                                                                                          \
        benchmark_report(&benchmark_measurement);                                         \
    }while(0)

void run_matrix_benchmark(void);
void run_vector_benchmark(void);
void run_cspline_benchmark(void);
void run_kalman_benchmark(void);
void run_emd_benchmark(void);

// The moving mean beside the filter that it replaces. This one measures two
// modules against each other, because the whole reason movavg exists is that
// it costs the same for every size of window and a fir does not.
void run_movavg_benchmark(void);

#endif//BENCHMARK_H
