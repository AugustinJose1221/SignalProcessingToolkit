// The benchmarks of the modules that estimate a state, and of the two
// detectors that watch a signal for something.
//
// FOUR STATES AND ONE MEASUREMENT for each of the three filters, so that the
// three can be set beside each other. That is the size of a body moving in a
// straight line with its speed, which is the case the three are written for.
//
// The Kalman filter is not here. It is in modules.c, where it is measured over
// a run of sizes of state, because how it grows with the state is what a
// caller of it wants to know.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/detect/changepoint.h>
#include <ffitt/detect/delay.h>
#include <ffitt/detect/matched.h>
#include <ffitt/estimate/ekf.h>
#include <ffitt/estimate/pll.h>
#include <ffitt/estimate/propagate.h>
#include <ffitt/estimate/ukf.h>
#include <ffitt/linalg/matrix.h>
#include <ffitt/transform/fft.h>

#include <stdlib.h>

#define ESTIMATE_STATES     4u
#define ESTIMATE_SIGNAL     4096u
#define ESTIMATE_PATTERN    64u
#define ESTIMATE_RATE       REAL_C(8000.0)

static real_t estimate_input[ESTIMATE_SIGNAL];
static real_t estimate_other[ESTIMATE_SIGNAL];
static real_t estimate_output[ESTIMATE_SIGNAL];

static real_t estimate_random(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

// A body that keeps its speed. The next place is the place plus the speed.
static void estimate_state_function(const matrix_t* state,
                                    const matrix_t* input, matrix_t* result)
{
    (void)input;

    for(uint32_t index = 0u; index < ESTIMATE_STATES; index++)
    {
        real_t value = matrix_get_element((matrix_t*)state, index, 0u);

        if(index + 1u < ESTIMATE_STATES)
        {
            value += matrix_get_element((matrix_t*)state, index + 1u, 0u);
        }

        matrix_add_element(result, index, 0u, value);
    }
}

// Only the first of the states is measured, which is the usual case.
static void estimate_measurement_function(const matrix_t* state,
                                          matrix_t* result)
{
    matrix_add_element(result, 0u, 0u, matrix_get_element((matrix_t*)state, 0u, 0u));
}

// The rate of a body that keeps its speed, for the module that propagates.
static void estimate_rate_function(real_t time, const real_t* state,
                                   const real_t* input, real_t* rate,
                                   uint32_t count)
{
    (void)time;
    (void)input;

    for(uint32_t index = 0u; index < count; index++)
    {
        rate[index] = (index + 1u < count) ? state[index + 1u] : REAL_C(0.0);
    }
}

static void run_ekf_benchmark(void)
{
    ekf_t ekf = ekf_alloc(1u, ESTIMATE_STATES, 1u);

    matrix_t p = matrix_create_unit_matrix(ESTIMATE_STATES);
    matrix_t q = matrix_create_zero_matrix(ESTIMATE_STATES, ESTIMATE_STATES);
    matrix_t r = matrix_create_unit_matrix(1u);
    matrix_t u = matrix_create_zero_matrix(1u, 1u);
    matrix_t x = matrix_create_zero_matrix(ESTIMATE_STATES, 1u);
    matrix_t y = matrix_create_zero_matrix(1u, 1u);

    matrix_add_element(&y, 0u, 0u, REAL_C(1.0));

    ekf_set_state_function(&ekf, estimate_state_function);
    ekf_set_measurement_function(&ekf, estimate_measurement_function);
    ekf_set_state_matrix(&ekf, &x);
    ekf_set_covariance_matrix(&ekf, &p);
    ekf_set_process_noise_covariance_matrix(&ekf, &q);
    ekf_set_measurement_covariance_matrix(&ekf, &r);
    ekf_set_input_matrix(&ekf, &u);
    ekf_set_measurement_matrix(&ekf, &y);

    BENCHMARK_MEASURE("ekf", "step",
                      "one step of a bending filter over four states",
                      ESTIMATE_STATES, 2000,
                      (void)ekf_step(&ekf, &u, &y));

    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&u);
    matrix_free(&x);
    matrix_free(&y);
    ekf_free(&ekf);
}

static void run_ukf_benchmark(void)
{
    ukf_t ukf = ukf_alloc(1u, ESTIMATE_STATES, 1u);

    matrix_t p = matrix_create_unit_matrix(ESTIMATE_STATES);
    matrix_t q = matrix_create_zero_matrix(ESTIMATE_STATES, ESTIMATE_STATES);
    matrix_t r = matrix_create_unit_matrix(1u);
    matrix_t u = matrix_create_zero_matrix(1u, 1u);
    matrix_t x = matrix_create_zero_matrix(ESTIMATE_STATES, 1u);
    matrix_t y = matrix_create_zero_matrix(1u, 1u);

    matrix_add_element(&y, 0u, 0u, REAL_C(1.0));

    ukf_set_state_function(&ukf, estimate_state_function);
    ukf_set_measurement_function(&ukf, estimate_measurement_function);
    ukf_set_state_matrix(&ukf, &x);
    ukf_set_covariance_matrix(&ukf, &p);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);
    ukf_set_input_matrix(&ukf, &u);
    ukf_set_measurement_matrix(&ukf, &y);

    BENCHMARK_MEASURE("ukf", "step",
                      "one step of a filter that places points, four states",
                      ESTIMATE_STATES, 2000,
                      (void)ukf_step(&ukf, &u, &y));

    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&u);
    matrix_free(&x);
    matrix_free(&y);
    ukf_free(&ukf);
}

static void run_pll_benchmark(void)
{
    pll_t pll = pll_make();

    (void)pll_design(&pll, REAL_C(1000.0), ESTIMATE_RATE, REAL_C(50.0),
                     REAL_C(0.707));

    BENCHMARK_MEASURE("pll", "process_block",
                      "hold a loop on a tone through 4096 samples",
                      ESTIMATE_SIGNAL, 500,
                      (void)pll_process_block(&pll, estimate_input,
                                              estimate_output,
                                              ESTIMATE_SIGNAL));
}

static void run_propagate_benchmark(void)
{
    real_t state[ESTIMATE_STATES] = {REAL_C(0.0), REAL_C(1.0), REAL_C(0.0),
                                     REAL_C(0.0)};

    BENCHMARK_MEASURE("propagate", "state",
                      "carry four states one step by the way of Runge",
                      ESTIMATE_STATES, 20000,
                      (void)propagate_state(PROPAGATE_RUNGE,
                                            estimate_rate_function,
                                            REAL_C(0.0), REAL_C(0.01), state,
                                            NULL, ESTIMATE_STATES));

    BENCHMARK_MEASURE("propagate", "state_over",
                      "carry four states across a hundred such steps",
                      ESTIMATE_STATES, 1000,
                      (void)propagate_state_over(PROPAGATE_RUNGE,
                                                 estimate_rate_function,
                                                 REAL_C(0.0), REAL_C(1.0),
                                                 100u, state, NULL,
                                                 ESTIMATE_STATES));
}

static void run_changepoint_benchmark(void)
{
    changepoint_t watcher = changepoint_make();
    changepoint_way_t way = CHANGEPOINT_NONE;
    uint32_t at = 0u;

    (void)changepoint_design(&watcher, REAL_C(0.0), REAL_C(1.0), REAL_C(1.0),
                             REAL_C(5.0));

    BENCHMARK_MEASURE("changepoint", "process_block",
                      "watch 4096 readings for the moment a level moves",
                      ESTIMATE_SIGNAL, 2000,
                      (void)changepoint_process_block(&watcher,
                                                      estimate_input,
                                                      ESTIMATE_SIGNAL, &way,
                                                      &at));
}

static void run_matched_benchmark(void)
{
    matched_t matched = matched_make();
    uint32_t where = 0u;
    real_t score = REAL_C(0.0);

    (void)matched_design(&matched, estimate_other, ESTIMATE_PATTERN);

    BENCHMARK_MEASURE("matched", "score_block",
                      "look for a shape of 64 all through 4096 samples",
                      ESTIMATE_SIGNAL, 50,
                      (void)matched_score_block(&matched, estimate_input,
                                                ESTIMATE_SIGNAL,
                                                estimate_output));

    BENCHMARK_MEASURE("matched", "best",
                      "find where that shape fits best",
                      ESTIMATE_SIGNAL, 50,
                      (void)matched_best(&matched, estimate_input,
                                         ESTIMATE_SIGNAL, &where, &score));
}

static void run_delay_benchmark(void)
{
    static const uint32_t SIZE = 1024u;

    fft_t fft = fft_alloc(SIZE);
    cnum_t* first = (cnum_t*)malloc(sizeof(cnum_t) * SIZE);
    cnum_t* second = (cnum_t*)malloc(sizeof(cnum_t) * SIZE);
    real_t* work = (real_t*)malloc(sizeof(real_t) * (SIZE + 1u));
    real_t delay = REAL_C(0.0);
    real_t strength = REAL_C(0.0);

    BENCHMARK_MEASURE("delay", "by_correlation",
                      "how far two signals of 1024 stand apart, by lag",
                      SIZE, 200,
                      (void)delay_by_correlation(estimate_input,
                                                 estimate_other, SIZE, 256u,
                                                 work, &delay, &strength));

    BENCHMARK_MEASURE("delay", "by_phase",
                      "the same to a fraction of a sample, by phase",
                      SIZE, 500,
                      (void)delay_by_phase(estimate_input, estimate_other,
                                           SIZE, &fft, first, second,
                                           &delay));

    free(first);
    free(second);
    free(work);
    fft_free(&fft);
}

void run_estimate_benchmark(void)
{
    for(uint32_t index = 0u; index < ESTIMATE_SIGNAL; index++)
    {
        estimate_input[index] = estimate_random();
        estimate_other[index] = estimate_random();
    }

    run_ekf_benchmark();
    run_ukf_benchmark();
    run_pll_benchmark();
    run_propagate_benchmark();
}

void run_detect_benchmark(void)
{
    run_changepoint_benchmark();
    run_matched_benchmark();
    run_delay_benchmark();
}
