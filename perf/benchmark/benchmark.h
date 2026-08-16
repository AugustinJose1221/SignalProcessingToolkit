#ifndef BENCHMARK_H
#define BENCHMARK_H

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
    double total_seconds;       // The time of all the repeats together
    double best_seconds;        // The time of the fastest repeat
}benchmark_result_t;

// Give the reading of the monotonic clock in seconds.
double benchmark_now(void);

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
        benchmark_measurement.total_seconds = 0.0;                                        \
        benchmark_measurement.best_seconds = 0.0;                                         \
                                                                                          \
        for(uint32_t benchmark_repeat = 0;                                                \
            benchmark_repeat < (repeat_count);                                            \
            benchmark_repeat++)                                                           \
        {                                                                                 \
            benchmark_start = benchmark_now();                                            \
            statement;                                                                    \
            benchmark_elapsed = benchmark_now() - benchmark_start;                        \
            benchmark_measurement.total_seconds += benchmark_elapsed;                     \
            if(benchmark_repeat == 0                                                      \
               || benchmark_elapsed < benchmark_measurement.best_seconds)                 \
            {                                                                             \
                benchmark_measurement.best_seconds = benchmark_elapsed;                   \
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

#endif//BENCHMARK_H
