#ifndef TEST
#include <sptk/util/peakdetect.h>
#include <sptk/core/defs.h>
#else
#include "peakdetect.h"
#include "defs.h"
#endif

uint32_t peakdetect_get_peaks(real_t* input, real_t* index_buffer, real_t* peak_buffer, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(index_buffer != NULL);
    ASSERT(size > 0);

    uint32_t peakcount = 0;

    if(size > 2)
    {
        for(uint32_t index = 1; index < size-1; index++)
        {
            if(input[index] > input[index-1] && input[index] > input[index+1])
            {
                peak_buffer[peakcount] = input[index];
                index_buffer[peakcount] = (real_t)index;
                peakcount++;
            }
        }
    }

    return peakcount;
}


peakdetect_options_t peakdetect_no_rules(void)
{
    peakdetect_options_t options;

    // Every rule switched off. A height of the smallest number the width can
    // hold lets through a peak of any height, including a negative one, which
    // a height of zero would not.
    options.minimum_height = -REAL_LARGEST;
    options.minimum_prominence = REAL_C(0.0);
    options.minimum_width = REAL_C(0.0);
    options.minimum_distance = 0u;

    return options;
}

// Where a peak stands, allowing for a flat top.
//
// A reading from a converter is a whole number of counts, thus the top of a
// peak is often two or three samples of exactly the same value. A test of
// "larger than both neighbours" finds no peak there at all, which loses the
// peak completely. This walks past any equal samples on each side and asks
// what stands beyond them.
//
// Give the index of the middle of the flat top if this is a peak, or the size
// of the signal if it is not.
static uint32_t peakdetect_peak_at(const real_t* input, uint32_t size,
                                   uint32_t index)
{
    if((index == 0u) || ((index + 1u) >= size))
    {
        return size;
    }

    real_t here = input[index];

    // Walk back over samples of the same value, and stop unless this is the
    // first of them: a flat top is reported once and not once for each sample.
    if(input[index - 1u] == here)
    {
        return size;
    }
    if(input[index - 1u] > here)
    {
        return size;
    }

    uint32_t last = index;
    while(((last + 1u) < size) && (input[last + 1u] == here))
    {
        last++;
    }

    if((last + 1u) >= size)
    {
        return size;
    }
    if(input[last + 1u] >= here)
    {
        return size;
    }

    return index + ((last - index) / 2u);
}

real_t peakdetect_prominence(const real_t* input, uint32_t size, uint32_t peak)
{
    ASSERT(input != NULL);

    if((peak >= size) || (size < 3u))
    {
        return REAL_C(0.0);
    }

    real_t top = input[peak];

    // Look left until the signal rises above the peak, keeping the lowest
    // point reached. That lowest point is the base on this side.
    real_t left_base = top;
    for(uint32_t index = peak; index > 0u; index--)
    {
        real_t value = input[index - 1u];
        if(value > top)
        {
            break;
        }
        if(value < left_base)
        {
            left_base = value;
        }
    }

    real_t right_base = top;
    for(uint32_t index = peak + 1u; index < size; index++)
    {
        real_t value = input[index];
        if(value > top)
        {
            break;
        }
        if(value < right_base)
        {
            right_base = value;
        }
    }

    // The HIGHER of the two bases. The peak must clear that one to stand out
    // at all: if it only clears the lower, there is a way to a higher peak
    // without descending that far.
    real_t base = (left_base > right_base) ? left_base : right_base;

    return top - base;
}

real_t peakdetect_width(const real_t* input, uint32_t size, uint32_t peak,
                        real_t part)
{
    ASSERT(input != NULL);

    if((peak >= size) || (size < 3u) || (part < REAL_C(0.0))
       || (part > REAL_C(1.0)))
    {
        return REAL_C(0.0);
    }

    real_t prominence = peakdetect_prominence(input, size, peak);

    if(prominence <= REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    real_t level = input[peak] - (part * prominence);

    // The left edge: the first place going back where the signal falls below
    // the level. The crossing is taken between the two samples either side of
    // it, thus the width is not limited to whole samples.
    real_t left = REAL_C(0.0);
    bool found = false;
    for(uint32_t index = peak; index > 0u; index--)
    {
        if(input[index - 1u] < level)
        {
            real_t gap = input[index] - input[index - 1u];
            real_t across = (gap > REAL_C(0.0))
                            ? ((level - input[index - 1u]) / gap) : REAL_C(0.0);
            left = (real_t)(index - 1u) + across;
            found = true;
            break;
        }
    }
    if(!found)
    {
        left = REAL_C(0.0);
    }

    real_t right = (real_t)(size - 1u);
    found = false;
    for(uint32_t index = peak; (index + 1u) < size; index++)
    {
        if(input[index + 1u] < level)
        {
            real_t gap = input[index] - input[index + 1u];
            real_t across = (gap > REAL_C(0.0))
                            ? ((input[index] - level) / gap) : REAL_C(0.0);
            right = (real_t)index + across;
            found = true;
            break;
        }
    }
    if(!found)
    {
        right = (real_t)(size - 1u);
    }

    return right - left;
}

uint32_t peakdetect_find(const real_t* input, uint32_t size,
                         const peakdetect_options_t* options,
                         uint32_t* index_out, uint32_t room)
{
    ASSERT(input != NULL);
    ASSERT(options != NULL);
    ASSERT(index_out != NULL);

    if((size < 3u) || (room == 0u))
    {
        return 0u;
    }

    uint32_t found = 0;

    // Walk the signal once, keeping every peak that passes the three rules
    // that look at one peak on its own.
    for(uint32_t index = 1; (index + 1u) < size; index++)
    {
        uint32_t at = peakdetect_peak_at(input, size, index);

        if(at >= size)
        {
            continue;
        }
        if(input[at] < options->minimum_height)
        {
            continue;
        }
        if(options->minimum_prominence > REAL_C(0.0))
        {
            if(peakdetect_prominence(input, size, at)
               < options->minimum_prominence)
            {
                continue;
            }
        }
        if(options->minimum_width > REAL_C(0.0))
        {
            if(peakdetect_width(input, size, at, REAL_C(0.5))
               < options->minimum_width)
            {
                continue;
            }
        }

        if(found < room)
        {
            index_out[found] = at;
            found++;
        }
        else
        {
            // No room. Throw away whichever kept peak stands out least, so
            // that what is kept is the best of what was seen and not merely
            // the first of it.
            uint32_t weakest = 0;
            real_t least = peakdetect_prominence(input, size,
                                                 index_out[0]);
            for(uint32_t k = 1; k < found; k++)
            {
                real_t stands = peakdetect_prominence(input, size,
                                                      index_out[k]);
                if(stands < least)
                {
                    least = stands;
                    weakest = k;
                }
            }

            if(peakdetect_prominence(input, size, at) > least)
            {
                for(uint32_t k = weakest; (k + 1u) < found; k++)
                {
                    index_out[k] = index_out[k + 1u];
                }
                index_out[found - 1u] = at;

                // The list must stay in the order the peaks stand in the
                // signal, thus the new one is put into its place.
                uint32_t last = found - 1u;
                while((last > 0u) && (index_out[last - 1u] > index_out[last]))
                {
                    uint32_t held = index_out[last - 1u];
                    index_out[last - 1u] = index_out[last];
                    index_out[last] = held;
                    last--;
                }
            }
        }
    }

    // The last rule looks at pairs. Where two peaks stand closer than the
    // distance allows, the one that stands out less goes.
    if((options->minimum_distance > 0u) && (found > 1u))
    {
        uint32_t kept = 0;

        for(uint32_t index = 0; index < found; index++)
        {
            bool keep = true;

            for(uint32_t other = 0; other < found; other++)
            {
                if(other == index)
                {
                    continue;
                }

                uint32_t apart = (index_out[index] > index_out[other])
                                 ? (index_out[index] - index_out[other])
                                 : (index_out[other] - index_out[index]);

                if(apart >= options->minimum_distance)
                {
                    continue;
                }

                real_t mine = peakdetect_prominence(input, size,
                                                    index_out[index]);
                real_t theirs = peakdetect_prominence(input, size,
                                                      index_out[other]);

                // The taller of the two stays. Where the two stand out
                // equally, the earlier one stays, so that the answer does not
                // depend on which was looked at first.
                if((theirs > mine)
                   || ((theirs == mine) && (other < index)))
                {
                    keep = false;
                    break;
                }
            }

            if(keep)
            {
                index_out[kept] = index_out[index];
                kept++;
            }
        }

        found = kept;
    }

    return found;
}
