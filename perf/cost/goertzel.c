// WHAT THE HEADER OF goertzel CLAIMS.
//
//   "The cost for one frequency is one multiplication and two additions for
//    each sample. For a few frequencies that is much less work than a
//    transform. When you need more than about log2(n) frequencies, the
//    transform costs less."
//
// This too is a crossover and it is held from both sides. The block is 1024
// samples, thus log2(n) is 10. Below it, two frequencies, which is the keypad
// tone that this module was written for. Above it, 128 frequencies.
//
// WHAT IS SET AGAINST WHAT. One transform of the block gives EVERY frequency
// at one time, thus it is one measurement whatever the caller wanted. This
// algorithm gives one frequency, thus asking for k of them means running it k
// times over the same block. The transform is given the same block through
// fft_forward_real, which is the call a caller makes who holds a real signal.
//
// 2 AND 128 AND NOT 9 AND 11. The header says "about log2(n)", thus the
// crossover has no exact place. Both measurements stand far from the middle,
// and neither answer turns on where exactly the two cross.

#include <perf/cost/cost.h>

#include <ffitt/transform/goertzel.h>
#include <ffitt/transform/fft.h>

#include <stdlib.h>

#define GOERTZEL_BLOCK          1024u
#define GOERTZEL_SAMPLE_RATE    REAL_C(8000.0)
#define GOERTZEL_FEW            2u
#define GOERTZEL_MANY           128u
#define GOERTZEL_BLOCKS         200u
#define GOERTZEL_REPEATS        7u

static real_t goertzel_input[GOERTZEL_BLOCK];
static cnum_t goertzel_spectrum[GOERTZEL_BLOCK];

// The frequencies are spread over the band that the sample rate can carry, so
// that no two of them are the same and none falls above half the rate.
static real_t goertzel_frequency(uint32_t index, uint32_t count)
{
    return (GOERTZEL_SAMPLE_RATE * REAL_C(0.4) * (real_t)(index + 1u))
           / (real_t)count;
}

static double goertzel_time_of_watching(uint32_t count)
{
    double seconds;

    COST_MEASURE(seconds, GOERTZEL_REPEATS,
    {
        for(uint32_t block = 0u; block < GOERTZEL_BLOCKS; block++)
        {
            for(uint32_t which = 0u; which < count; which++)
            {
                goertzel_t watcher = goertzel_init(goertzel_frequency(which,
                                                                      count),
                                                   GOERTZEL_SAMPLE_RATE,
                                                   GOERTZEL_BLOCK);

                goertzel_process_block(&watcher, goertzel_input,
                                       GOERTZEL_BLOCK);

                cost_sink += goertzel_magnitude_squared(&watcher);
            }
        }
    });

    return seconds;
}

static double goertzel_time_of_the_transform(void)
{
    double seconds;
    fft_t fft = fft_alloc(GOERTZEL_BLOCK);

    COST_MEASURE(seconds, GOERTZEL_REPEATS,
    {
        for(uint32_t block = 0u; block < GOERTZEL_BLOCKS; block++)
        {
            fft_forward_real(&fft, goertzel_input, goertzel_spectrum);
            cost_sink += goertzel_spectrum[1].re;
        }
    });

    fft_free(&fft);

    return seconds;
}

void run_goertzel_cost_tests(void)
{
    double watching_2;
    double watching_128;
    double the_transform;

    for(uint32_t index = 0u; index < GOERTZEL_BLOCK; index++)
    {
        goertzel_input[index] = cost_random_value();
    }

    watching_2 = goertzel_time_of_watching(GOERTZEL_FEW);
    watching_128 = goertzel_time_of_watching(GOERTZEL_MANY);
    the_transform = goertzel_time_of_the_transform();

    cost_claim_at_least("goertzel",
                        "below log2(n) frequencies this beats the transform",
                        the_transform / watching_2, 1.5);

    cost_claim_at_least("goertzel",
                        "above log2(n) frequencies the transform costs less",
                        watching_128 / the_transform, 1.5);
}
