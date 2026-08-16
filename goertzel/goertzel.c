#ifndef TEST
#include <goertzel/goertzel.h>
#include <common/defs.h>
#else
#include "goertzel.h"
#include "defs.h"
#endif

#include <math.h>

#define GOERTZEL_PI     3.14159265358979323846f

goertzel_t goertzel_init(float frequency, float sample_rate, uint32_t block_size)
{
    ASSERT(sample_rate > 0.0f);
    ASSERT(frequency >= 0.0f);
    ASSERT(frequency < (sample_rate / 2.0f));
    ASSERT(block_size > 0);

    goertzel_t goertzel;

    // Find the bin of the block that lies nearest to the given frequency, and
    // then take the angle of that bin. The algorithm works with a whole number
    // of turns inside the block.
    float bin = ((float)block_size * frequency) / sample_rate;
    float angle = (2.0f * GOERTZEL_PI * bin) / (float)block_size;

    goertzel.cosine = cosf(angle);
    goertzel.sine = sinf(angle);
    goertzel.coefficient = 2.0f * goertzel.cosine;
    goertzel.block_size = block_size;

    goertzel_reset(&goertzel);

    return goertzel;
}

void goertzel_process_sample(goertzel_t* goertzel, float sample)
{
    ASSERT(goertzel != NULL);

    // The state holds the last two values of a filter with two poles. After
    // the whole block the two values give the answer at the frequency.
    float value = sample + (goertzel->coefficient * goertzel->first)
                  - goertzel->second;

    goertzel->second = goertzel->first;
    goertzel->first = value;
    goertzel->count++;
}

void goertzel_process_block(goertzel_t* goertzel, const float* input, uint32_t size)
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

float goertzel_magnitude_squared(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    float real = goertzel->first - (goertzel->second * goertzel->cosine);
    float imaginary = goertzel->second * goertzel->sine;

    return (real*real) + (imaginary*imaginary);
}

float goertzel_magnitude(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    return sqrtf(goertzel_magnitude_squared(goertzel));
}

float goertzel_phase(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    float real = goertzel->first - (goertzel->second * goertzel->cosine);
    float imaginary = goertzel->second * goertzel->sine;

    return atan2f(imaginary, real);
}

void goertzel_reset(goertzel_t* goertzel)
{
    ASSERT(goertzel != NULL);

    goertzel->first = 0.0f;
    goertzel->second = 0.0f;
    goertzel->count = 0;
}
