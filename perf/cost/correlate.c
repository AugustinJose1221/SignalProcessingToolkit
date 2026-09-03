// WHAT THE HEADER OF correlate CLAIMS.
//
//   "The plain method multiplies and adds once for each sample at each lag,
//    thus it costs size times lags. For 4096 samples and 4096 lags that is 17
//    million operations."
//
//   "The transform does the same work in three transforms and one
//    multiplication of each bin... For the same numbers that is about 300
//    thousand operations, which is fifty times less... Below about 300 samples
//    the plain method wins, because the transform has a fixed cost that the
//    plain method does not."
//
// That is a crossover, thus it is held from BOTH sides. One measurement below
// the crossover, where the plain method must win, and one well above it, where
// the transform must win by a wide margin. A claim held on one side only would
// pass just as well for a module that was always faster one way, which is not
// what the header says and not what a caller needs to know.
//
// 64 AND 4096 AND NOT 299 AND 301. The header says "about 300", thus the
// crossover has no exact place and asking for one would be asking the header
// for a promise it did not make. Measured at 64 and at 4096, both sides stand
// far from the middle and neither answer turns on where exactly the two cross.
//
// The claim of fifty times counts OPERATIONS. Time is not operations: the
// plain method walks its samples in order, which a machine does well. Thus the
// claim is held at four times and the margin is in the comment and not hidden.
//
// THIS CLAIM ONCE BROKE AT 64 BITS AND NOW HOLDS AT BOTH. The butterfly of the
// transform handed a cnum_t to a function and took one back, which at 64 bits
// is a sixteen byte pair that GCC 13.3 built badly at -O2. It is written on the
// parts now. This measured 1.52 against the 4.00 it asks for; it measures 15.95.

#include <perf/cost/cost.h>

#include <ffitt/transform/correlate.h>
#include <ffitt/transform/fft.h>

#include <stdlib.h>

#define CORRELATE_SHORT_SIZE    64u
#define CORRELATE_LONG_SIZE     4096u
#define CORRELATE_REPEATS       7u

static real_t correlate_input[CORRELATE_LONG_SIZE];
static real_t correlate_output[CORRELATE_LONG_SIZE];

// Both ways are measured over the same number of calls, thus what comes back
// can be set side by side without dividing by anything.
static uint32_t correlate_calls_for(uint32_t size)
{
    return (size == CORRELATE_SHORT_SIZE) ? 2000u : 5u;
}

static double correlate_time_of_the_plain_way(uint32_t size)
{
    double seconds;
    uint32_t calls = correlate_calls_for(size);

    COST_MEASURE(seconds, CORRELATE_REPEATS,
    {
        for(uint32_t call = 0u; call < calls; call++)
        {
            (void)correlate_auto(correlate_input, size, correlate_output,
                                 size - 1u, CORRELATE_RAW);
            cost_sink += correlate_output[0];
        }
    });

    return seconds;
}

static double correlate_time_of_the_transform(uint32_t size)
{
    double seconds;
    uint32_t calls = correlate_calls_for(size);
    uint32_t transform_size = correlate_transform_size(size);
    fft_t fft = fft_alloc(transform_size);
    cnum_t* work = (cnum_t*)malloc(sizeof(cnum_t) * transform_size);
    real_t* window = (real_t*)malloc(sizeof(real_t) * transform_size);

    COST_MEASURE(seconds, CORRELATE_REPEATS,
    {
        for(uint32_t call = 0u; call < calls; call++)
        {
            (void)correlate_auto_by_transform(correlate_input, size,
                                              correlate_output, size - 1u,
                                              CORRELATE_RAW, &fft, work,
                                              window);
            cost_sink += correlate_output[0];
        }
    });

    free(work);
    free(window);
    fft_free(&fft);

    return seconds;
}

void run_correlate_cost_tests(void)
{
    double plain_at_64;
    double transform_at_64;
    double plain_at_4096;
    double transform_at_4096;

    for(uint32_t index = 0u; index < CORRELATE_LONG_SIZE; index++)
    {
        correlate_input[index] = cost_random_value();
    }

    plain_at_64 = correlate_time_of_the_plain_way(CORRELATE_SHORT_SIZE);
    transform_at_64 = correlate_time_of_the_transform(CORRELATE_SHORT_SIZE);
    plain_at_4096 = correlate_time_of_the_plain_way(CORRELATE_LONG_SIZE);
    transform_at_4096 = correlate_time_of_the_transform(CORRELATE_LONG_SIZE);

    cost_claim_at_least("correlate",
                        "below about 300 samples the plain way wins",
                        transform_at_64 / plain_at_64, 1.1);

    cost_claim_at_least("correlate",
                        "at 4096 the transform wins by a wide margin",
                        plain_at_4096 / transform_at_4096, 4.0);
}
