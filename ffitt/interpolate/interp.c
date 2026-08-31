#ifndef TEST
#include <ffitt/interpolate/interp.h>
#include <ffitt/util/binarysearch.h>
#include <ffitt/core/defs.h>
#else
#include "interp.h"
#include "binarysearch.h"
#include "defs.h"
#endif

#include <math.h>

bool interp_is_valid_kind(interp_kind_t kind)
{
    return (kind >= INTERP_LINEAR) && (kind <= INTERP_PCHIP);
}

bool interp_is_valid_table(const real_t* input, uint32_t size)
{
    ASSERT(input != NULL);

    if(size < 2u)
    {
        return false;
    }

    for(uint32_t index = 1; index < size; index++)
    {
        // Not merely rising, but rising with no two the same. Two entries at
        // one input would ask the curve to hold two values in one place, and
        // every way of reading the table divides by the gap between them.
        if(input[index] <= input[index - 1u])
        {
            return false;
        }
    }

    return true;
}

// Which pair of points a place falls between.
//
// The answer is the index of the point BELOW the place, from 0 to size-2. A
// place outside the table gives the nearest pair, and the caller holds the
// answer flat there rather than carrying the curve on.
static uint32_t interp_pair_below(const real_t* input, uint32_t size,
                                  real_t place)
{
    uint32_t found = binarysearch_get_index((real_t*)input, place, size);

    // The search gives the first point that is not below the place. The pair
    // begins one before that.
    if(found == 0u)
    {
        return 0u;
    }
    if(found >= (size - 1u))
    {
        return size - 2u;
    }

    return found - 1u;
}

real_t interp_linear(const real_t* input, const real_t* output, uint32_t size,
                     real_t place)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }
    if(size == 1u)
    {
        return output[0];
    }

    // Outside the table the answer is held flat. Carrying a straight line on
    // past the end of a calibration says what the device would read where it
    // was never calibrated, and saying nothing is better than saying that.
    if(place <= input[0])
    {
        return output[0];
    }
    if(place >= input[size - 1u])
    {
        return output[size - 1u];
    }

    uint32_t below = interp_pair_below(input, size, place);
    real_t gap = input[below + 1u] - input[below];

    if(gap <= REAL_C(0.0))
    {
        return output[below];
    }

    real_t part = (place - input[below]) / gap;

    return output[below] + (part * (output[below + 1u] - output[below]));
}

bool interp_pchip_slopes(const real_t* input, const real_t* output,
                         uint32_t size, real_t* slopes)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(slopes != NULL);

    if(!interp_is_valid_table(input, size))
    {
        return false;
    }

    if(size == 2u)
    {
        // Two points hold one straight line, and both slopes are that line.
        real_t only = (output[1] - output[0]) / (input[1] - input[0]);
        slopes[0] = only;
        slopes[1] = only;

        return true;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        real_t slope;

        if(index == 0u)
        {
            slope = (output[1] - output[0]) / (input[1] - input[0]);
        }
        else if(index == (size - 1u))
        {
            slope = (output[size - 1u] - output[size - 2u])
                    / (input[size - 1u] - input[size - 2u]);
        }
        else
        {
            real_t before = (output[index] - output[index - 1u])
                            / (input[index] - input[index - 1u]);
            real_t after = (output[index + 1u] - output[index])
                           / (input[index + 1u] - input[index]);

            // THE ONE RULE THAT STOPS THE OVERSHOOTING.
            //
            // Where the table turns, the slope at the turn is nothing. A
            // point that is higher than both its neighbours is a peak, and a
            // curve through it with any slope at all would carry on past it
            // and come back, which is an overshoot.
            //
            // Where the table does not turn, the slope is a mean of the two
            // slopes either side that leans towards the SMALLER of them. That
            // keeps the curve inside what the two neighbours allow: a slope
            // larger than three times the smaller neighbouring slope is what
            // makes a curve leave their range, and this mean can never reach
            // it.
            if((before * after) <= REAL_C(0.0))
            {
                slope = REAL_C(0.0);
            }
            else
            {
                real_t left = input[index] - input[index - 1u];
                real_t right = input[index + 1u] - input[index];
                real_t first = (REAL_C(2.0) * right) + left;
                real_t second = right + (REAL_C(2.0) * left);

                slope = (first + second)
                        / ((first / before) + (second / after));
            }
        }

        slopes[index] = slope;
    }

    return true;
}

real_t interp_pchip(const real_t* input, const real_t* output,
                    const real_t* slopes, uint32_t size, real_t place)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(slopes != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }
    if(size == 1u)
    {
        return output[0];
    }

    if(place <= input[0])
    {
        return output[0];
    }
    if(place >= input[size - 1u])
    {
        return output[size - 1u];
    }

    uint32_t below = interp_pair_below(input, size, place);
    real_t gap = input[below + 1u] - input[below];

    if(gap <= REAL_C(0.0))
    {
        return output[below];
    }

    // A curve of the third order over this pair, fixed by the two values and
    // the two slopes. The four shapes below are the ones that give 1 at one of
    // those four and 0 at the other three.
    real_t part = (place - input[below]) / gap;
    real_t squared = part * part;
    real_t cubed = squared * part;

    real_t hold_first = (REAL_C(2.0) * cubed) - (REAL_C(3.0) * squared)
                        + REAL_C(1.0);
    real_t hold_second = (-REAL_C(2.0) * cubed) + (REAL_C(3.0) * squared);
    real_t lean_first = cubed - (REAL_C(2.0) * squared) + part;
    real_t lean_second = cubed - squared;

    return (hold_first * output[below])
           + (hold_second * output[below + 1u])
           + (gap * lean_first * slopes[below])
           + (gap * lean_second * slopes[below + 1u]);
}

bool interp_block(const real_t* input, const real_t* output,
                  const real_t* slopes, uint32_t size, interp_kind_t kind,
                  const real_t* places, real_t* answers, uint32_t count)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(places != NULL);
    ASSERT(answers != NULL);

    if(!interp_is_valid_kind(kind) || !interp_is_valid_table(input, size))
    {
        return false;
    }
    if((kind == INTERP_PCHIP) && (slopes == NULL))
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        answers[index] = (kind == INTERP_PCHIP)
                         ? interp_pchip(input, output, slopes, size,
                                        places[index])
                         : interp_linear(input, output, size, places[index]);
    }

    return true;
}
