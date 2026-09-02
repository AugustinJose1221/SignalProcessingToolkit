// WHAT THE HEADER OF resample CLAIMS.
//
//   "This module works out only the answers it keeps. The filtering costs the
//    same as the OUTPUT rate and not the input rate, thus decimating by 64
//    with a filter of 128 coefficients costs 2 multiplications for each input
//    sample and not 128."
//
// Two things are measured, and both feed the SAME number of input samples
// through a filter of the SAME length.
//
//   AGAINST ITSELF. Going down by 64 against going down by 2. If the cost
//   followed the input rate the two would take the same time. It follows the
//   output rate, thus going down by 64 does a thirty-second of the work.
//
//   AGAINST A PLAIN FILTER. A fir of the same length, run over every input
//   sample, which is the way a caller writes it who has not read the header.
//   That one does the full length at each sample and is the cost the module
//   exists to avoid.
//
// The filter is 129 and not 128 because the header asks for an odd length, so
// that the filter has a middle and delays every frequency by the same time.
//
// WHY THE MEASURED NUMBER IS NOT 64. The header counts MULTIPLICATIONS, and by
// that count going down by 64 does a sixty-fourth of the work. It is measured
// here in TIME, and both ways pay the same cost of one call for each sample
// that arrives, whether or not an answer comes out of it. That fixed cost is
// the whole of what going down by 64 still spends, thus it holds the measured
// number near ten and not near sixty-four. Ten is still the difference between
// a filter a caller can afford and one it cannot, which is what the header is
// telling a caller.

#include <perf/cost/cost.h>

#include <ffitt/filter/resample.h>
#include <ffitt/filter/fir.h>

#include <stdlib.h>

#define RESAMPLE_LENGTH         129u
#define RESAMPLE_SMALL_FACTOR   2u
#define RESAMPLE_LARGE_FACTOR   64u
#define RESAMPLE_SAMPLES        200000u
#define RESAMPLE_REPEATS        7u

static real_t resample_input[RESAMPLE_SAMPLES];

static double resample_time_of_decimating(uint32_t factor)
{
    double seconds;
    resample_t decimator = resample_alloc_decimator(factor, RESAMPLE_LENGTH);

    COST_MEASURE(seconds, RESAMPLE_REPEATS,
    {
        real_t output = REAL_C(0.0);

        for(uint32_t index = 0u; index < RESAMPLE_SAMPLES; index++)
        {
            if(resample_decimate(&decimator, resample_input[index], &output))
            {
                cost_sink += output;
            }
        }
    });

    resample_free(&decimator);

    return seconds;
}

static double resample_time_of_the_plain_filter(void)
{
    double seconds;
    fir_t filter = fir_alloc(RESAMPLE_LENGTH);

    fir_design_low_pass(&filter, REAL_C(0.5) / (real_t)RESAMPLE_LARGE_FACTOR);

    COST_MEASURE(seconds, RESAMPLE_REPEATS,
    {
        for(uint32_t index = 0u; index < RESAMPLE_SAMPLES; index++)
        {
            cost_sink += fir_process_sample(&filter, resample_input[index]);
        }
    });

    fir_free(&filter);

    return seconds;
}

void run_resample_cost_tests(void)
{
    double going_down_by_2;
    double going_down_by_64;
    double the_plain_filter;

    for(uint32_t index = 0u; index < RESAMPLE_SAMPLES; index++)
    {
        resample_input[index] = cost_random_value();
    }

    going_down_by_2 = resample_time_of_decimating(RESAMPLE_SMALL_FACTOR);
    going_down_by_64 = resample_time_of_decimating(RESAMPLE_LARGE_FACTOR);
    the_plain_filter = resample_time_of_the_plain_filter();

    cost_claim_at_least("resample",
                        "down by 64 beats down by 2, same filter and input",
                        going_down_by_2 / going_down_by_64, 5.0);

    cost_claim_at_least("resample",
                        "down by 64 beats a plain filter of the same length",
                        the_plain_filter / going_down_by_64, 5.0);
}
