// This file is left out of the build when FFITT_NO_UTIL is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_UTIL

#ifndef TEST
#include <ffitt/util/stats.h>
#include <ffitt/core/defs.h>
#else
#include "stats.h"
#include "defs.h"
#endif

#include <math.h>

static void stats_swap(real_t* data, uint32_t left, uint32_t right)
{
    real_t held = data[left];
    data[left] = data[right];
    data[right] = held;
}

// Put the sample that belongs at the given place into that place, and put every
// smaller sample below it and every larger one above it.
//
// This is the method of the quick select. It is the quick sort with one half
// left out: after a split, only the half that holds the wanted place is worked
// on again. Thus it costs about one pass over the list and not one pass for
// each level, which is what a whole sort would cost.
static void stats_select(real_t* data, uint32_t size, uint32_t place)
{
    uint32_t low = 0;
    uint32_t high = (size > 0u) ? (size - 1u) : 0u;

    while(low < high)
    {
        // Take the middle sample as the pivot and not the first one. A list
        // that is already in order is a common input, and the first sample
        // would then split it into nothing and everything, which is the worst
        // case of this method.
        uint32_t middle = low + ((high - low) / 2u);
        stats_swap(data, middle, high);

        real_t pivot = data[high];
        uint32_t store = low;

        for(uint32_t index = low; index < high; index++)
        {
            if(data[index] < pivot)
            {
                stats_swap(data, index, store);
                store++;
            }
        }
        stats_swap(data, store, high);

        if(place == store)
        {
            return;
        }
        if(place < store)
        {
            high = (store > low) ? (store - 1u) : low;
        }
        else
        {
            low = store + 1u;
        }
    }
}

// The sum runs at the width of the build, and a sum is where the digits run
// out first. This is worth recording.
//
// At 32 bits a number holds about seven digits. Adding a thousand samples that
// each sit near eight million gives a total near eight thousand million, where
// one step is 512. Every sample after the first few is added to a number too
// large to hold it, and the low digits fall away. The error grows with the
// number of samples.
//
// Measured, on five samples at eight million that move by one, where the
// variance is exactly 2: 32 bits gives 2.25 and 64 bits gives 2.00.
//
// The header says what to do about it. There is nothing this function can do
// on its own, because a sum of large numbers needs digits that the samples
// themselves do not carry.
real_t stats_sum(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    real_t total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (real_t)data[index];
    }

    return (real_t)total;
}

real_t stats_mean(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (real_t)data[index];
    }

    return (real_t)(total / (real_t)size);
}

real_t stats_variance(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    // Take the mean away first, and only then square.
    //
    // The other way, which is the mean of the squares less the square of the
    // mean, needs one pass and not two. It is also wrong for the signals that
    // this library meets. A reading that sits at 8 000 000 and moves by 1
    // gives two numbers near 64 000 000 000 000 whose difference is 1. A float
    // holds about seven digits, thus that difference is lost and the answer
    // comes out as noise or as a negative number.
    real_t mean = stats_mean(data, size);
    real_t total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t distance = (real_t)data[index] - (real_t)mean;
        total += distance * distance;
    }

    return (real_t)(total / (real_t)size);
}

real_t stats_deviation(const real_t* data, uint32_t size)
{
    return REAL_SQRT(stats_variance(data, size));
}

real_t stats_rms(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (real_t)data[index] * (real_t)data[index];
    }

    return (real_t)REAL_SQRT(total / (real_t)size);
}

real_t stats_min(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t smallest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] < smallest)
        {
            smallest = data[index];
        }
    }

    return smallest;
}

real_t stats_max(const real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t largest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] > largest)
        {
            largest = data[index];
        }
    }

    return largest;
}

real_t stats_median(real_t* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    uint32_t middle = size / 2u;
    stats_select(data, size, middle);

    if((size % 2u) == 1u)
    {
        return data[middle];
    }

    // For an even size the median lies between the two middle samples. The
    // select above put every smaller sample below the middle, thus the largest
    // of those is the other one, and it can be found without another select.
    real_t upper = data[middle];
    real_t lower = data[0];

    for(uint32_t index = 1; index < middle; index++)
    {
        if(data[index] > lower)
        {
            lower = data[index];
        }
    }

    return (lower + upper) / REAL_C(2.0);
}

real_t stats_percentile(real_t* data, uint32_t size, real_t part)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }
    if(size == 1u)
    {
        return data[0];
    }
    if(part <= REAL_C(0.0))
    {
        return stats_min(data, size);
    }
    if(part >= REAL_C(1.0))
    {
        return stats_max(data, size);
    }

    // The place that the part asks for, which usually lies between two
    // samples.
    real_t place = part * (real_t)(size - 1u);
    uint32_t below = (uint32_t)place;
    real_t between = place - (real_t)below;

    stats_select(data, size, below);
    real_t lower = data[below];

    if(between <= REAL_C(0.0))
    {
        return lower;
    }

    // The next sample up is the smallest of those that stand above the place.
    real_t upper = data[below + 1u];
    for(uint32_t index = below + 1u; index < size; index++)
    {
        if(data[index] < upper)
        {
            upper = data[index];
        }
    }

    return lower + (between * (upper - lower));
}

real_t stats_mad(const real_t* data, uint32_t size, real_t* work)
{
    ASSERT(data != NULL);
    ASSERT(work != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    for(uint32_t index = 0; index < size; index++)
    {
        work[index] = data[index];
    }

    real_t middle = stats_median(work, size);

    for(uint32_t index = 0; index < size; index++)
    {
        work[index] = REAL_ABS(data[index] - middle);
    }

    return stats_median(work, size);
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int stats_is_not_in_this_build_t;

#endif//FFITT_NO_UTIL
