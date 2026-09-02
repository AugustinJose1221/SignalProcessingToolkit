// The benchmarks of the utility modules, of the two of the core that do work,
// and of the module that interpolates between readings.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/interpolate/interp.h>
#include <ffitt/util/binarysearch.h>
#include <ffitt/util/curve.h>
#include <ffitt/util/generate.h>
#include <ffitt/util/peakdetect.h>
#include <ffitt/util/quantise.h>
#include <ffitt/util/stats.h>
#include <ffitt/util/valleydetect.h>

#include <stdlib.h>
#include <string.h>

#define UTIL_SIZE       4096u
#define UTIL_TABLE      256u
#define UTIL_RATE       REAL_C(8000.0)

static real_t util_data[UTIL_SIZE];
static real_t util_scratch[UTIL_SIZE];
static real_t util_output[UTIL_SIZE];
static real_t util_index[UTIL_SIZE];
static real_t util_table_x[UTIL_TABLE];
static real_t util_table_y[UTIL_TABLE];
static real_t util_slopes[UTIL_TABLE];
static real_t util_places[UTIL_SIZE];

static real_t util_random(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

static void run_stats_benchmark(void)
{
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("stats", "mean",
                      "the mean of 4096 readings",
                      UTIL_SIZE, 20000,
                      value = stats_mean(util_data, UTIL_SIZE));

    BENCHMARK_MEASURE("stats", "deviation",
                      "how far 4096 readings spread",
                      UTIL_SIZE, 20000,
                      value = stats_deviation(util_data, UTIL_SIZE));

    BENCHMARK_MEASURE("stats", "rms",
                      "the root mean square of 4096 readings",
                      UTIL_SIZE, 20000,
                      value = stats_rms(util_data, UTIL_SIZE));

    BENCHMARK_MEASURE("stats", "median",
                      "the middle of 4096 readings",
                      UTIL_SIZE, 2000,
                      {
                          memcpy(util_scratch, util_data,
                                 sizeof(real_t) * UTIL_SIZE);
                          value = stats_median(util_scratch, UTIL_SIZE);
                      });

    BENCHMARK_MEASURE("stats", "mad",
                      "the spread of 4096 readings, wild ones and all",
                      UTIL_SIZE, 2000,
                      value = stats_mad(util_data, UTIL_SIZE, util_scratch));
    (void)value;
}

static void run_peakdetect_benchmark(void)
{
    peakdetect_options_t options = peakdetect_no_rules();
    uint32_t* found = (uint32_t*)malloc(sizeof(uint32_t) * UTIL_SIZE);
    uint32_t count = 0u;
    real_t value = REAL_C(0.0);

    options.minimum_height = REAL_C(0.2);
    options.minimum_distance = 4u;

    BENCHMARK_MEASURE("peakdetect", "get_peaks",
                      "every peak in 4096 samples",
                      UTIL_SIZE, 2000,
                      count = peakdetect_get_peaks(util_data, util_index,
                                                   util_output, UTIL_SIZE));

    BENCHMARK_MEASURE("peakdetect", "find",
                      "the peaks in 4096 samples that pass the rules",
                      UTIL_SIZE, 500,
                      count = peakdetect_find(util_data, UTIL_SIZE, &options,
                                              found, UTIL_SIZE));

    BENCHMARK_MEASURE("peakdetect", "prominence",
                      "how far one peak stands above its neighbours",
                      UTIL_SIZE, 2000,
                      value = peakdetect_prominence(util_data, UTIL_SIZE,
                                                    UTIL_SIZE / 2u));
    (void)count;
    (void)value;

    free(found);
}

static void run_valleydetect_benchmark(void)
{
    uint32_t count = 0u;

    BENCHMARK_MEASURE("valleydetect", "get_valley",
                      "every valley in 4096 samples",
                      UTIL_SIZE, 2000,
                      count = valleydetect_get_valley(util_data, util_index,
                                                      util_output, UTIL_SIZE));
    (void)count;
}

static void run_generate_benchmark(void)
{
    generate_t sine = generate_make(GENERATE_SINE);
    generate_t square = generate_make(GENERATE_SQUARE);
    generate_t noise = generate_make(GENERATE_WHITE_NOISE);

    (void)generate_design(&sine, REAL_C(1000.0), UTIL_RATE);
    (void)generate_design(&square, REAL_C(1000.0), UTIL_RATE);
    (void)generate_design(&noise, REAL_C(1000.0), UTIL_RATE);

    BENCHMARK_MEASURE("generate", "block sine",
                      "make 4096 samples of a sine",
                      UTIL_SIZE, 2000,
                      (void)generate_block(&sine, util_output, UTIL_SIZE));

    BENCHMARK_MEASURE("generate", "block square",
                      "make 4096 samples of a square wave with no aliases",
                      UTIL_SIZE, 2000,
                      (void)generate_block(&square, util_output, UTIL_SIZE));

    BENCHMARK_MEASURE("generate", "block noise",
                      "make 4096 samples of noise",
                      UTIL_SIZE, 2000,
                      (void)generate_block(&noise, util_output, UTIL_SIZE));
}

static void run_quantise_benchmark(void)
{
    quantise_t plain = quantise_make();
    quantise_t dithered = quantise_make();

    (void)quantise_design(&plain, QUANTISE_PLAIN, 12u, REAL_C(1.0));
    (void)quantise_design(&dithered, QUANTISE_DITHER, 12u, REAL_C(1.0));

    BENCHMARK_MEASURE("quantise", "block plain",
                      "round 4096 samples to 12 bits",
                      UTIL_SIZE, 2000,
                      (void)quantise_block(&plain, util_data, util_output,
                                           UTIL_SIZE));

    BENCHMARK_MEASURE("quantise", "block dither",
                      "the same with a little noise added first",
                      UTIL_SIZE, 2000,
                      (void)quantise_block(&dithered, util_data, util_output,
                                           UTIL_SIZE));
}

static void run_curve_benchmark(void)
{
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("curve", "block",
                      "draw a bell curve over 4096 places",
                      UTIL_SIZE, 2000,
                      (void)curve_block(CURVE_GAUSSIAN, REAL_C(-5.0),
                                        REAL_C(5.0), REAL_C(0.0),
                                        REAL_C(1.0), REAL_C(0.0), util_output,
                                        UTIL_SIZE));

    BENCHMARK_MEASURE("curve", "value",
                      "read a bell curve at one place",
                      1u, 500000,
                      value = curve_value(CURVE_GAUSSIAN, REAL_C(0.5),
                                          REAL_C(0.0), REAL_C(1.0),
                                          REAL_C(0.0)));
    (void)value;
}

static void run_binarysearch_benchmark(void)
{
    uint32_t where = 0u;

    BENCHMARK_MEASURE("binarysearch", "get_index",
                      "find a value in a sorted list of 256",
                      UTIL_TABLE, 500000,
                      where = binarysearch_get_index(util_table_x,
                                                     REAL_C(0.5),
                                                     UTIL_TABLE));
    (void)where;
}

static void run_ringbuf_benchmark(void)
{
    ringbuf_t buffer = ringbuf_alloc(UTIL_TABLE);
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("ringbuf", "put",
                      "put one sample into a buffer of 256",
                      UTIL_TABLE, 500000,
                      ringbuf_put(&buffer, REAL_C(0.5)));

    BENCHMARK_MEASURE("ringbuf", "get",
                      "read a sample from a hundred steps ago",
                      UTIL_TABLE, 500000,
                      value = ringbuf_get(&buffer, 100u));

    BENCHMARK_MEASURE("ringbuf", "copy",
                      "write the whole buffer of 256 out in order",
                      UTIL_TABLE, 50000,
                      (void)ringbuf_copy(&buffer, util_output));
    (void)value;

    ringbuf_free(&buffer);
}

static void run_real_benchmark(void)
{
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("real", "sin",
                      "one sine, at the width the library was built for",
                      1u, 500000,
                      value = real_sin(REAL_C(0.5)));

    BENCHMARK_MEASURE("real", "sqrt",
                      "one square root",
                      1u, 500000,
                      value = real_sqrt(REAL_C(0.5)));

    BENCHMARK_MEASURE("real", "exp",
                      "one exponential",
                      1u, 500000,
                      value = real_exp(REAL_C(0.5)));
    (void)value;
}

static void run_interp_benchmark(void)
{
    real_t value = REAL_C(0.0);

    (void)interp_pchip_slopes(util_table_x, util_table_y, UTIL_TABLE,
                              util_slopes);

    BENCHMARK_MEASURE("interp", "linear",
                      "read one place from a table of 256, straight lines",
                      UTIL_TABLE, 200000,
                      value = interp_linear(util_table_x, util_table_y,
                                            UTIL_TABLE, REAL_C(0.5)));

    BENCHMARK_MEASURE("interp", "pchip",
                      "the same, smooth and never above the neighbours",
                      UTIL_TABLE, 200000,
                      value = interp_pchip(util_table_x, util_table_y,
                                           util_slopes, UTIL_TABLE,
                                           REAL_C(0.5)));

    BENCHMARK_MEASURE("interp", "block",
                      "read 4096 places from that table at one call",
                      UTIL_SIZE, 500,
                      (void)interp_block(util_table_x, util_table_y,
                                         util_slopes, UTIL_TABLE,
                                         INTERP_PCHIP, util_places,
                                         util_output, UTIL_SIZE));
    (void)value;
}

void run_util_benchmark(void)
{
    for(uint32_t index = 0u; index < UTIL_SIZE; index++)
    {
        util_data[index] = util_random()
                           + real_sin((real_t)index * REAL_C(0.05));
        util_places[index] = (real_t)index / (real_t)UTIL_SIZE;
    }

    for(uint32_t index = 0u; index < UTIL_TABLE; index++)
    {
        util_table_x[index] = (real_t)index / (real_t)UTIL_TABLE;
        util_table_y[index] = util_random();
    }

    run_stats_benchmark();
    run_peakdetect_benchmark();
    run_valleydetect_benchmark();
    run_generate_benchmark();
    run_quantise_benchmark();
    run_curve_benchmark();
    run_binarysearch_benchmark();
}

void run_core_benchmark(void)
{
    run_ringbuf_benchmark();
    run_real_benchmark();
}

void run_interpolate_benchmark(void)
{
    run_interp_benchmark();
}
