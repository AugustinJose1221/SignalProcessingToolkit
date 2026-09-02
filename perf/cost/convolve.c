// WHAT THE HEADER OF convolve CLAIMS.
//
//   "The plain way multiplies and adds once for each sample of the shape at
//    every place, thus n times m. For a signal of 4096 and a shape of 512 that
//    is two million operations."
//
//   "A convolution in time is a multiplication in frequency, thus the
//    transform does the same work in three transforms. For those numbers that
//    is about 400 thousand, which is five times less. Below a shape of about
//    60 the plain way wins, because the transform has a fixed cost that it has
//    not."
//
// The numbers of the header are used as the header wrote them: a signal of
// 4096 and a shape of 512. The other side of the crossover is a shape of 8,
// which is well below the 60 the header names.
//
// THE TRANSFORM PAYS THE SAME PRICE AT BOTH SHAPES. The transform must be at
// least as long as the whole answer, and the answer is about the length of the
// signal whatever the shape, thus both measurements below run a transform of
// 8192. That is the fixed cost the header speaks of, and it is why a short
// shape loses by it.
//
// The claim of five times counts OPERATIONS and not time, thus it is held at
// one and a half times. The plain way walks its samples in order and a machine
// does that well, and the transform of 8192 is longer than the 4607 the answer
// needs because a transform wants a power of two. Measured 2.22 at 32 bits.
//
// THIS CLAIM BREAKS AT 64 BITS AND HOLDS AT 32, AND THE LIBRARY IS NOT THE
// REASON. GCC 13.3 builds the same transform more than six times slower at 64
// bits with -O2 than it does with -O1, and the answers do not change by a
// digit. perf/cost/README.md holds the measurement. Nothing here is weakened
// to hide it.

#include <perf/cost/cost.h>

#include <ffitt/transform/convolve.h>
#include <ffitt/transform/fft.h>

#include <stdlib.h>

#define CONVOLVE_SIGNAL_SIZE    4096u
#define CONVOLVE_SHORT_SHAPE    8u
#define CONVOLVE_LONG_SHAPE     512u
#define CONVOLVE_CALLS          20u
#define CONVOLVE_REPEATS        7u

static real_t convolve_signal[CONVOLVE_SIGNAL_SIZE];
static real_t convolve_shape[CONVOLVE_LONG_SHAPE];
static real_t convolve_answer[CONVOLVE_SIGNAL_SIZE + CONVOLVE_LONG_SHAPE];

static double convolve_time_of_the_plain_way(uint32_t shape_size)
{
    double seconds;

    COST_MEASURE(seconds, CONVOLVE_REPEATS,
    {
        for(uint32_t call = 0u; call < CONVOLVE_CALLS; call++)
        {
            (void)convolve_direct(convolve_signal, CONVOLVE_SIGNAL_SIZE,
                                  convolve_shape, shape_size, convolve_answer,
                                  CONVOLVE_FULL);
            cost_sink += convolve_answer[0];
        }
    });

    return seconds;
}

static double convolve_time_of_the_transform(uint32_t shape_size)
{
    double seconds;
    uint32_t transform_size = convolve_transform_size(CONVOLVE_SIGNAL_SIZE,
                                                      shape_size);
    fft_t fft = fft_alloc(transform_size);
    cnum_t* first = (cnum_t*)malloc(sizeof(cnum_t) * transform_size);
    cnum_t* second = (cnum_t*)malloc(sizeof(cnum_t) * transform_size);
    real_t* work = (real_t*)malloc(sizeof(real_t) * transform_size);

    COST_MEASURE(seconds, CONVOLVE_REPEATS,
    {
        for(uint32_t call = 0u; call < CONVOLVE_CALLS; call++)
        {
            (void)convolve_by_transform(convolve_signal, CONVOLVE_SIGNAL_SIZE,
                                        convolve_shape, shape_size,
                                        convolve_answer, CONVOLVE_FULL, &fft,
                                        first, second, work);
            cost_sink += convolve_answer[0];
        }
    });

    free(first);
    free(second);
    free(work);
    fft_free(&fft);

    return seconds;
}

void run_convolve_cost_tests(void)
{
    double plain_at_8;
    double transform_at_8;
    double plain_at_512;
    double transform_at_512;

    for(uint32_t index = 0u; index < CONVOLVE_SIGNAL_SIZE; index++)
    {
        convolve_signal[index] = cost_random_value();
    }

    for(uint32_t index = 0u; index < CONVOLVE_LONG_SHAPE; index++)
    {
        convolve_shape[index] = cost_random_value();
    }

    plain_at_8 = convolve_time_of_the_plain_way(CONVOLVE_SHORT_SHAPE);
    transform_at_8 = convolve_time_of_the_transform(CONVOLVE_SHORT_SHAPE);
    plain_at_512 = convolve_time_of_the_plain_way(CONVOLVE_LONG_SHAPE);
    transform_at_512 = convolve_time_of_the_transform(CONVOLVE_LONG_SHAPE);

    cost_claim_at_least("convolve",
                        "below a shape of about 60 the plain way wins",
                        transform_at_8 / plain_at_8, 1.5);

    cost_claim_at_least("convolve",
                        "at a shape of 512 the transform costs far less",
                        plain_at_512 / transform_at_512, 1.5);
}
