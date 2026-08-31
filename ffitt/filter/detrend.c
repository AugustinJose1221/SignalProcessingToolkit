#ifndef TEST
#include <ffitt/filter/detrend.h>
#include <ffitt/core/defs.h>
#else
#include "detrend.h"
#include "defs.h"
#endif

#include <math.h>

bool detrend_is_valid_kind(detrend_kind_t kind)
{
    return (kind == DETREND_CONSTANT) || (kind == DETREND_LINEAR);
}

// Give the number of a sample counted from the middle of the block.
//
// This is the whole of why the module holds its precision. The middle of a
// block of n samples lies at (n - 1) divided by two, thus the numbers run from
// minus half the block to plus half of it and their sum is zero. A sum that is
// zero is a sum that cannot grow, and every product below stays the size of
// the samples themselves.
static real_t detrend_place_of(uint32_t size, uint32_t index)
{
    return (real_t)index - (((real_t)size - REAL_C(1.0)) / REAL_C(2.0));
}

bool detrend_trend(const real_t* input, uint32_t size, detrend_kind_t kind,
                   real_t* offset, real_t* slope)
{
    ASSERT(input != NULL);
    ASSERT(offset != NULL);
    ASSERT(slope != NULL);

    if(!detrend_is_valid_kind(kind))
    {
        return false;
    }

    // A straight line wants two samples to have a direction at all. A level
    // wants one.
    if(size < ((kind == DETREND_LINEAR) ? 2u : 1u))
    {
        return false;
    }

    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        total += input[index];
    }

    // The middle of the trend is the mean, for both kinds. For the straight
    // line that is so because the numbering above sums to zero: the line of
    // least squared error always passes through the middle of the samples.
    *offset = total / (real_t)size;
    *slope = REAL_C(0.0);

    if(kind == DETREND_CONSTANT)
    {
        return true;
    }

    // The slope of the line of least squared error. With the numbering summing
    // to zero this is one sum divided by another, and neither holds a
    // subtraction of two nearly equal numbers.
    real_t across = REAL_C(0.0);
    real_t spread = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t place = detrend_place_of(size, index);

        across += place * (input[index] - *offset);
        spread += place * place;
    }

    // The spread is zero only for a block of one sample, which the size check
    // above has already refused. The guard is here because a division that
    // cannot happen is still a division.
    if(spread <= REAL_SMALLEST)
    {
        return false;
    }

    *slope = across / spread;

    return true;
}

real_t detrend_trend_at(real_t offset, real_t slope, uint32_t size,
                        uint32_t index)
{
    return offset + (slope * detrend_place_of(size, index));
}

bool detrend_remove(const real_t* input, real_t* output, uint32_t size,
                    real_t offset, real_t slope)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(size == 0u)
    {
        return false;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = input[index]
                        - detrend_trend_at(offset, slope, size, index);
    }

    return true;
}

bool detrend_block(const real_t* input, real_t* output, uint32_t size,
                   detrend_kind_t kind)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    real_t offset;
    real_t slope;

    if(!detrend_trend(input, size, kind, &offset, &slope))
    {
        return false;
    }

    return detrend_remove(input, output, size, offset, slope);
}
