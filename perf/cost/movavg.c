// WHAT THE HEADER OF movavg CLAIMS.
//
//   "The cost of this module does not follow the window. The cost of the other
//    one does, and at a window of 4096 it is two hundred times as much."
//
//   "BELOW A WINDOW OF 16 THE PLAIN FILTER IS FASTER... Take the fir module
//    for a very short window; take this one from about 16 upwards."
//
// The header holds a table of measured times, in nanoseconds for one sample:
//
//   window          4      8     16     64    500   4096
//   this module  13.4   13.4   12.5   12.4   14.1   17.6
//   equal fir     6.0    8.2   14.1   58.9  438.7 3588.4
//
// That table is the reason a caller picks one module over the other, thus it
// is what these three claims hold. A fir whose coefficients are all the same
// gives the same answer as the moving mean, thus the two are doing one job by
// two roads and the times can be set beside each other.
//
// THE ADVICE TO SWAP AT 16 IS THE ONE HELD LOOSELY IN THE OTHER DIRECTION. It
// is held at a window of 4, where the table gives the fir more than twice the
// speed, and not at 16, where the table has the two within a tenth of each
// other and no machine could be asked to tell them apart.

#include <perf/cost/cost.h>

#include <ffitt/filter/movavg.h>
#include <ffitt/filter/fir.h>

#include <stdlib.h>

#define MOVAVG_SHORT_WINDOW     4u
#define MOVAVG_LONG_WINDOW      4096u
#define MOVAVG_SAMPLES          200000u
#define MOVAVG_REPEATS          7u

static real_t movavg_input[MOVAVG_SAMPLES];

static double movavg_time_of_the_mean(uint32_t window)
{
    double seconds;
    movavg_t mean = movavg_alloc(window);

    COST_MEASURE(seconds, MOVAVG_REPEATS,
    {
        for(uint32_t index = 0u; index < MOVAVG_SAMPLES; index++)
        {
            cost_sink += movavg_process_sample(&mean, movavg_input[index]);
        }
    });

    movavg_free(&mean);

    return seconds;
}

static double movavg_time_of_the_equal_filter(uint32_t window)
{
    double seconds;
    fir_t filter = fir_alloc(window);

    for(uint32_t index = 0u; index < window; index++)
    {
        fir_set_coefficient(&filter, index, REAL_C(1.0) / (real_t)window);
    }

    COST_MEASURE(seconds, MOVAVG_REPEATS,
    {
        for(uint32_t index = 0u; index < MOVAVG_SAMPLES; index++)
        {
            cost_sink += fir_process_sample(&filter, movavg_input[index]);
        }
    });

    fir_free(&filter);

    return seconds;
}

void run_movavg_cost_tests(void)
{
    double mean_at_4;
    double mean_at_4096;
    double filter_at_4;
    double filter_at_4096;

    for(uint32_t index = 0u; index < MOVAVG_SAMPLES; index++)
    {
        movavg_input[index] = cost_random_value();
    }

    mean_at_4 = movavg_time_of_the_mean(MOVAVG_SHORT_WINDOW);
    mean_at_4096 = movavg_time_of_the_mean(MOVAVG_LONG_WINDOW);
    filter_at_4 = movavg_time_of_the_equal_filter(MOVAVG_SHORT_WINDOW);
    filter_at_4096 = movavg_time_of_the_equal_filter(MOVAVG_LONG_WINDOW);

    cost_claim_at_most("movavg",
                       "the cost does not follow the window, 4 to 4096",
                       mean_at_4096 / mean_at_4, 3.0);

    cost_claim_at_least("movavg",
                        "an equal fir of 4096 costs two hundred times as much",
                        filter_at_4096 / mean_at_4096, 50.0);

    cost_claim_at_least("movavg",
                        "below a window of 16 the plain filter is faster",
                        mean_at_4 / filter_at_4, 1.1);
}
