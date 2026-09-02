// The benchmarks of the filter modules.
//
// A BLOCK OF 4096 SAMPLES THROUGH EACH, so that the times of the filters can
// be set beside each other. Where a filter has a window or a length, it is 33,
// which is short enough for a filter that runs as the signal arrives and long
// enough that the work is not lost in the cost of one call.
//
// The moving mean, the plain filter and the resampler are not here. They are
// measured against each other in modules.c and in perf/cost, because what
// matters about those three is how they stand against one another and not what
// one of them costs on its own.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/filter/adaptive.h>
#include <ffitt/filter/dcblock.h>
#include <ffitt/filter/detrend.h>
#include <ffitt/filter/farrow.h>
#include <ffitt/filter/filtfilt.h>
#include <ffitt/filter/fir.h>
#include <ffitt/filter/hampel.h>
#include <ffitt/filter/iir.h>
#include <ffitt/filter/lattice.h>
#include <ffitt/filter/medfilt.h>
#include <ffitt/filter/resample.h>
#include <ffitt/filter/rls.h>
#include <ffitt/filter/savgol.h>

#include <stdlib.h>

#define FILTER_SIGNAL       4096u
#define FILTER_WINDOW       33u
#define FILTER_LENGTH       32u

static real_t filter_input[FILTER_SIGNAL];
static real_t filter_wanted[FILTER_SIGNAL];
static real_t filter_output[FILTER_SIGNAL];
static real_t filter_error[FILTER_SIGNAL];

static real_t filter_random(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

static void run_fir_benchmark(void)
{
    fir_t fir = fir_alloc(FILTER_WINDOW);

    (void)fir_design_low_pass(&fir, REAL_C(0.1));

    BENCHMARK_MEASURE("fir", "design_low_pass",
                      "design a low pass of 33 coefficients",
                      FILTER_WINDOW, 5000,
                      (void)fir_design_low_pass(&fir, REAL_C(0.1)));

    BENCHMARK_MEASURE("fir", "process_block",
                      "4096 samples through a filter of 33 with no feedback",
                      FILTER_SIGNAL, 500,
                      fir_process_block(&fir, filter_input, filter_output,
                                        FILTER_SIGNAL));

    fir_free(&fir);
}

static void run_iir_benchmark(void)
{
    iir_t iir = iir_alloc(2u);

    (void)iir_design_low_pass(&iir, REAL_C(0.1));

    BENCHMARK_MEASURE("iir", "design_low_pass",
                      "design a low pass of two sections",
                      2u, 5000,
                      (void)iir_design_low_pass(&iir, REAL_C(0.1)));

    BENCHMARK_MEASURE("iir", "process_block",
                      "4096 samples through a filter of two sections",
                      FILTER_SIGNAL, 2000,
                      iir_process_block(&iir, filter_input, filter_output,
                                        FILTER_SIGNAL));

    iir_free(&iir);
}

static void run_filtfilt_benchmark(void)
{
    iir_t iir = iir_alloc(2u);
    fir_t fir = fir_alloc(FILTER_WINDOW);

    (void)iir_design_low_pass(&iir, REAL_C(0.1));
    (void)fir_design_low_pass(&fir, REAL_C(0.1));

    BENCHMARK_MEASURE("filtfilt", "iir",
                      "4096 samples both ways, thus with no delay at all",
                      FILTER_SIGNAL, 1000,
                      (void)filtfilt_iir(&iir, filter_input, filter_output,
                                         FILTER_SIGNAL));

    BENCHMARK_MEASURE("filtfilt", "fir",
                      "the same with a filter of 33 and no feedback",
                      FILTER_SIGNAL, 200,
                      (void)filtfilt_fir(&fir, filter_input, filter_output,
                                         FILTER_SIGNAL));

    iir_free(&iir);
    fir_free(&fir);
}

static void run_adaptive_benchmark(void)
{
    adaptive_t adaptive = adaptive_alloc(FILTER_LENGTH);

    (void)adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.01));

    BENCHMARK_MEASURE("adaptive", "process_block",
                      "4096 samples through a filter of 32 that learns",
                      FILTER_SIGNAL, 200,
                      (void)adaptive_process_block(&adaptive, filter_input,
                                                   filter_wanted,
                                                   filter_output, filter_error,
                                                   FILTER_SIGNAL));

    adaptive_free(&adaptive);
}

static void run_rls_benchmark(void)
{
    rls_t rls = rls_alloc(FILTER_LENGTH);

    (void)rls_design(&rls, REAL_C(0.99), REAL_C(100.0));

    BENCHMARK_MEASURE("rls", "process_block",
                      "4096 samples through a filter of 32 that learns fast",
                      FILTER_SIGNAL, 20,
                      (void)rls_process_block(&rls, filter_input,
                                              filter_wanted, filter_output,
                                              filter_error, FILTER_SIGNAL));

    rls_free(&rls);
}

static void run_lattice_benchmark(void)
{
    lattice_t lattice = lattice_alloc(8u);

    (void)lattice_design(&lattice, REAL_C(0.01), REAL_C(0.99));

    BENCHMARK_MEASURE("lattice", "process_block",
                      "4096 samples through eight stages that learn",
                      FILTER_SIGNAL, 200,
                      (void)lattice_process_block(&lattice, filter_input,
                                                  filter_wanted, filter_error,
                                                  FILTER_SIGNAL));

    lattice_free(&lattice);
}

static void run_savgol_benchmark(void)
{
    savgol_t savgol = savgol_alloc(FILTER_WINDOW);

    (void)savgol_design(&savgol, 3u, 0u);

    BENCHMARK_MEASURE("savgol", "design",
                      "design a smoother of 33 that keeps a cubic",
                      FILTER_WINDOW, 2000,
                      (void)savgol_design(&savgol, 3u, 0u));

    BENCHMARK_MEASURE("savgol", "process_block",
                      "smooth 4096 samples and keep the shape of the peaks",
                      FILTER_SIGNAL, 200,
                      savgol_process_block(&savgol, filter_input,
                                           filter_output, FILTER_SIGNAL));

    savgol_free(&savgol);
}

static void run_medfilt_benchmark(void)
{
    medfilt_t medfilt = medfilt_alloc(FILTER_WINDOW);

    BENCHMARK_MEASURE("medfilt", "process_block",
                      "the middle value of a window of 33 over 4096 samples",
                      FILTER_SIGNAL, 100,
                      medfilt_process_block(&medfilt, filter_input,
                                            filter_output, FILTER_SIGNAL));

    medfilt_free(&medfilt);
}

static void run_hampel_benchmark(void)
{
    hampel_t hampel = hampel_alloc(FILTER_WINDOW);
    uint32_t replaced = 0u;

    hampel_set_threshold(&hampel, REAL_C(3.0));

    BENCHMARK_MEASURE("hampel", "process_block",
                      "take the wild readings out of 4096 samples",
                      FILTER_SIGNAL, 100,
                      replaced = hampel_process_block(&hampel, filter_input,
                                                      filter_output,
                                                      FILTER_SIGNAL));
    (void)replaced;

    hampel_free(&hampel);
}

static void run_dcblock_benchmark(void)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));

    BENCHMARK_MEASURE("dcblock", "process_block",
                      "take the standing level out of 4096 samples",
                      FILTER_SIGNAL, 2000,
                      dcblock_process_block(&dcblock, filter_input,
                                            filter_output, FILTER_SIGNAL));
}

static void run_detrend_benchmark(void)
{
    real_t offset = REAL_C(0.0);
    real_t slope = REAL_C(0.0);

    BENCHMARK_MEASURE("detrend", "trend",
                      "find the drift under 4096 samples",
                      FILTER_SIGNAL, 2000,
                      (void)detrend_trend(filter_input, FILTER_SIGNAL,
                                          DETREND_LINEAR, &offset, &slope));

    BENCHMARK_MEASURE("detrend", "block",
                      "find that drift and take it away",
                      FILTER_SIGNAL, 2000,
                      (void)detrend_block(filter_input, filter_output,
                                          FILTER_SIGNAL, DETREND_LINEAR));

    BENCHMARK_MEASURE("detrend", "remove",
                      "take a drift already known away from 4096 samples",
                      FILTER_SIGNAL, 5000,
                      (void)detrend_remove(filter_input, filter_output,
                                           FILTER_SIGNAL, offset, slope));
}

static void run_farrow_benchmark(void)
{
    farrow_t farrow = farrow_alloc(3u);

    (void)farrow_set_delay(&farrow, REAL_C(0.5));

    BENCHMARK_MEASURE("farrow", "process_block",
                      "delay 4096 samples by half a sample",
                      FILTER_SIGNAL, 500,
                      (void)farrow_process_block(&farrow, filter_input,
                                                 filter_output,
                                                 FILTER_SIGNAL));

    farrow_free(&farrow);
}

static void run_resample_block_benchmark(void)
{
    resample_t decimator = resample_alloc_decimator(4u, 65u);
    resample_t interpolator = resample_alloc_interpolator(4u, 65u);
    real_t* wide = (real_t*)malloc(sizeof(real_t) * FILTER_SIGNAL * 4u);
    uint32_t written = 0u;

    BENCHMARK_MEASURE("resample", "decimate_block",
                      "4096 samples down to a quarter of the rate",
                      FILTER_SIGNAL, 500,
                      written = resample_decimate_block(&decimator,
                                                        filter_input,
                                                        filter_output,
                                                        FILTER_SIGNAL));

    BENCHMARK_MEASURE("resample", "interpolate_block",
                      "4096 samples up to four times the rate",
                      FILTER_SIGNAL, 200,
                      written = resample_interpolate_block(&interpolator,
                                                           filter_input, wide,
                                                           FILTER_SIGNAL));
    (void)written;

    free(wide);
    resample_free(&decimator);
    resample_free(&interpolator);
}

void run_filter_benchmark(void)
{
    for(uint32_t index = 0u; index < FILTER_SIGNAL; index++)
    {
        filter_input[index] = filter_random();
        filter_wanted[index] = filter_random();
    }

    run_fir_benchmark();
    run_iir_benchmark();
    run_filtfilt_benchmark();
    run_adaptive_benchmark();
    run_rls_benchmark();
    run_lattice_benchmark();
    run_savgol_benchmark();
    run_medfilt_benchmark();
    run_hampel_benchmark();
    run_dcblock_benchmark();
    run_detrend_benchmark();
    run_farrow_benchmark();
    run_resample_block_benchmark();
}
