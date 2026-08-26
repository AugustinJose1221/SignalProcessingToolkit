#ifndef TEST
#include <sptk/util/quantise.h>
#include <sptk/core/defs.h>
#else
#include "quantise.h"
#include "defs.h"
#endif

#include <math.h>

bool quantise_is_valid_way(quantise_way_t way)
{
    return (way >= QUANTISE_PLAIN) && (way <= QUANTISE_SHAPED);
}

bool quantise_is_valid_bits(uint32_t bits)
{
    return (bits >= 1u) && (bits <= QUANTISE_LARGEST_BITS);
}

quantise_t quantise_make(void)
{
    quantise_t quantise;

    quantise.way = QUANTISE_DITHER;
    quantise.step = REAL_C(0.0);
    quantise.reach = REAL_C(1.0);
    quantise.carried = REAL_C(0.0);
    quantise.seed = 1u;
    quantise.designed = false;

    return quantise;
}

void quantise_reset(quantise_t* quantise)
{
    ASSERT(quantise != NULL);

    quantise->carried = REAL_C(0.0);
}

bool quantise_design(quantise_t* quantise, quantise_way_t way, uint32_t bits,
                     real_t reach)
{
    ASSERT(quantise != NULL);

    if(!quantise_is_valid_way(way) || !quantise_is_valid_bits(bits)
       || (reach <= REAL_SMALLEST))
    {
        return false;
    }

    quantise->way = way;
    quantise->reach = reach;

    // The steps run from minus the reach to plus it, thus a quantiser of n
    // bits has its steps this far apart.
    real_t count = (real_t)((uint32_t)1u << (bits - 1u));

    quantise->step = reach / count;
    quantise->designed = true;

    quantise_reset(quantise);

    return true;
}

void quantise_set_seed(quantise_t* quantise, uint32_t seed)
{
    ASSERT(quantise != NULL);

    quantise->seed = (seed == 0u) ? 1u : seed;
}

real_t quantise_step_of(const quantise_t* quantise)
{
    ASSERT(quantise != NULL);

    return quantise->step;
}

real_t quantise_noise_floor(uint32_t bits)
{
    // Rounded plainly, the error is spread across one step, thus its power is
    // the step squared over twelve. Against a sine filling the whole reach
    // that works out at about six decibels for each bit and a little over.
    return -((REAL_C(6.02) * (real_t)bits) + REAL_C(1.76));
}

// The next dither value, spread evenly across one step either way.
//
// TWO DRAWS ADDED AND NOT ONE. A single draw breaks the pattern but leaves the
// loudness of the error following the signal, which can still be heard as the
// signal moving. Two added together leave the loudness the same whatever the
// signal is doing, which is what is wanted and costs one more shift.
static real_t quantise_next_dither(quantise_t* quantise)
{
    real_t total = REAL_C(0.0);

    for(uint32_t draw = 0; draw < 2u; draw++)
    {
        uint32_t held = quantise->seed;

        held ^= held << 13u;
        held ^= held >> 17u;
        held ^= held << 5u;

        quantise->seed = held;

        real_t part = (real_t)(held >> 8u) / (real_t)(1u << 24u);

        total += (part - REAL_C(0.5));
    }

    return total * quantise->step;
}

real_t quantise_sample(quantise_t* quantise, real_t sample)
{
    ASSERT(quantise != NULL);

    if(!quantise->designed)
    {
        return sample;
    }

    real_t going_in = sample;

    // SHAPING: the error of the sample before is taken off this one. What the
    // quantiser then throws away is the difference between the two errors
    // rather than the error itself, and a difference holds far less of its
    // power low down.
    if(quantise->way == QUANTISE_SHAPED)
    {
        going_in -= quantise->carried;
    }

    real_t rounded_from = going_in;

    if(quantise->way != QUANTISE_PLAIN)
    {
        rounded_from += quantise_next_dither(quantise);
    }

    // To the nearest step.
    real_t steps = REAL_FLOOR((rounded_from / quantise->step) + REAL_C(0.5));
    real_t answer = steps * quantise->step;

    // HELD AT THE ENDS RATHER THAN WRAPPED. A signal that wraps does not sound
    // loud; it sounds broken, and one sample of it can undo a whole
    // measurement.
    if(answer > quantise->reach)
    {
        answer = quantise->reach;
    }

    real_t lowest = -quantise->reach;

    if(answer < lowest)
    {
        answer = lowest;
    }

    if(quantise->way == QUANTISE_SHAPED)
    {
        // What this sample threw away, to be taken off the next one. It is
        // measured against what went in AFTER the shaping, so that the carried
        // error cannot gather without bound.
        quantise->carried = answer - going_in;
    }

    return answer;
}

bool quantise_block(quantise_t* quantise, const real_t* input, real_t* output,
                    uint32_t count)
{
    ASSERT(quantise != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(!quantise->designed)
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        output[index] = quantise_sample(quantise, input[index]);
    }

    return true;
}
