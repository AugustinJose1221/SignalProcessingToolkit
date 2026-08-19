#ifndef TEST
#include <sptk/util/stats.h>
#include <sptk/core/defs.h>
#else
#include "stats.h"
#include "defs.h"
#endif

#include <math.h>

static void stats_swap(float* data, uint32_t left, uint32_t right)
{
    float held = data[left];
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
static void stats_select(float* data, uint32_t size, uint32_t place)
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

        float pivot = data[high];
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

// Every sum in this module runs in double, and the reason is worth recording.
//
// A float holds about seven digits. Adding a thousand samples that each sit
// near eight million gives a total near eight thousand million, where one step
// of a float is 512. Every sample after the first few is then added to a
// number too large to hold it, and the low digits fall away. The error grows
// with the number of samples.
//
// Measured, on five samples at eight million that move by one, where the
// variance is exactly 2: adding in float gave 2.25, which is out by a tenth.
// With a thousand samples it is far worse.
//
// A double holds about sixteen digits, thus the same sum keeps every digit
// that the float samples ever had. The cost is nothing worth counting, because
// the work is one add for each sample either way.
float stats_sum(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    double total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (double)data[index];
    }

    return (float)total;
}

float stats_mean(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    double total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (double)data[index];
    }

    return (float)(total / (double)size);
}

float stats_variance(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    // Take the mean away first, and only then square.
    //
    // The other way, which is the mean of the squares less the square of the
    // mean, needs one pass and not two. It is also wrong for the signals that
    // this library meets. A reading that sits at 8 000 000 and moves by 1
    // gives two numbers near 64 000 000 000 000 whose difference is 1. A float
    // holds about seven digits, thus that difference is lost and the answer
    // comes out as noise or as a negative number.
    float mean = stats_mean(data, size);
    double total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        double distance = (double)data[index] - (double)mean;
        total += distance * distance;
    }

    return (float)(total / (double)size);
}

float stats_deviation(const float* data, uint32_t size)
{
    return sqrtf(stats_variance(data, size));
}

float stats_rms(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    double total = 0.0;

    for(uint32_t index = 0; index < size; index++)
    {
        total += (double)data[index] * (double)data[index];
    }

    return (float)sqrt(total / (double)size);
}

float stats_min(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    float smallest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] < smallest)
        {
            smallest = data[index];
        }
    }

    return smallest;
}

float stats_max(const float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    float largest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] > largest)
        {
            largest = data[index];
        }
    }

    return largest;
}

float stats_median(float* data, uint32_t size)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
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
    float upper = data[middle];
    float lower = data[0];

    for(uint32_t index = 1; index < middle; index++)
    {
        if(data[index] > lower)
        {
            lower = data[index];
        }
    }

    return (lower + upper) / 2.0f;
}

float stats_percentile(float* data, uint32_t size, float part)
{
    ASSERT(data != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }
    if(size == 1u)
    {
        return data[0];
    }
    if(part <= 0.0f)
    {
        return stats_min(data, size);
    }
    if(part >= 1.0f)
    {
        return stats_max(data, size);
    }

    // The place that the part asks for, which usually lies between two
    // samples.
    float place = part * (float)(size - 1u);
    uint32_t below = (uint32_t)place;
    float between = place - (float)below;

    stats_select(data, size, below);
    float lower = data[below];

    if(between <= 0.0f)
    {
        return lower;
    }

    // The next sample up is the smallest of those that stand above the place.
    float upper = data[below + 1u];
    for(uint32_t index = below + 1u; index < size; index++)
    {
        if(data[index] < upper)
        {
            upper = data[index];
        }
    }

    return lower + (between * (upper - lower));
}

float stats_mad(const float* data, uint32_t size, float* work)
{
    ASSERT(data != NULL);
    ASSERT(work != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        work[index] = data[index];
    }

    float middle = stats_median(work, size);

    for(uint32_t index = 0; index < size; index++)
    {
        work[index] = fabsf(data[index] - middle);
    }

    return stats_median(work, size);
}
