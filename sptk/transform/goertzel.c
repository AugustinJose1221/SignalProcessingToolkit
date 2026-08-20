#ifndef TEST
#include <sptk/transform/goertzel.h>
#include <sptk/core/defs.h>
#else
#include "goertzel.h"
#include "defs.h"
#endif

#include <math.h>

#define GOERTZEL_PI     REAL_C(3.14159265358979323846)

goertzel_t goertzel_init(real_t frequency, real_t sample_rate, uint32_t block_size)
{
    ASSERT(sample_rate > REAL_C(0.0));
    ASSERT(frequency >= REAL_C(0.0));
    ASSERT(frequency < (sample_rate / REAL_C(2.0)));
    ASSERT(block_size > 0);

    goertzel_t goertzel;

    // Find the bin of the block that lies nearest to the given frequency, and
    // then take the angle of that bin. The algorithm works with a whole number
    // of turns inside the block.
    real_t bin = ((real_t)block_size * frequency) / sample_rate;
    real_t angle = (REAL_C(2.0) * GOERTZEL_PI * bin) / (real_t)block_size;

    goertzel.cosine = REAL_COS(angle);
    goertzel.sine = REAL_SIN(angle);
    goertzel.coefficient = REAL_C(2.0) * goertzel.cosine;
    goertzel.block_size = block_size;

    goertzel_reset(&goertzel);

    return goertzel;
}

void goertzel_process_sample(goertzel_t* goertzel, real_t sample)
{
    ASSERT(goertzel != NULL);

    // The state holds the last two values of a filter with two poles. After
    // the whole block the two values give the answer at the frequency.
    real_t value = sample + (goertzel->coefficient * goertzel->first)
                  - goertzel->second;

    goertzel->second = goertzel->first;
    goertzel->first = value;
    goertzel->count++;
}

void goertzel_process_block(goertzel_t* goertzel, const real_t* input, uint32_t size)
{
    ASSERT(goertzel != NULL);
    ASSERT(input != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        goertzel_process_sample(goertzel, input[index]);
    }
}

bool goertzel_is_block_complete(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    return goertzel->count >= goertzel->block_size;
}

real_t goertzel_magnitude_squared(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    real_t real = goertzel->first - (goertzel->second * goertzel->cosine);
    real_t imaginary = goertzel->second * goertzel->sine;

    return (real*real) + (imaginary*imaginary);
}

real_t goertzel_magnitude(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    return REAL_SQRT(goertzel_magnitude_squared(goertzel));
}

real_t goertzel_phase(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    real_t real = goertzel->first - (goertzel->second * goertzel->cosine);
    real_t imaginary = goertzel->second * goertzel->sine;

    return REAL_ATAN2(imaginary, real);
}

void goertzel_reset(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    goertzel->first = REAL_C(0.0);
    goertzel->second = REAL_C(0.0);
    goertzel->count = 0;
}
