#ifndef TEST
#include <sptk/filter/medfilt.h>
#include <sptk/util/binarysearch.h>
#include <sptk/core/defs.h>
#else
#include "medfilt.h"
#include "binarysearch.h"
#include "defs.h"
#endif

// Where a value belongs in the list that is already in order.
//
// The binarysearch module gives the first place whose value is not less than
// the one asked for. That is what is wanted, with one exception that the
// module states: when EVERY value is less, it gives the last place and not the
// place after the end, so that a caller who reads the list there is safe.
//
// An insertion needs the place after the end in that case, thus this function
// puts the exception right. It is the only place where the difference matters.
static uint32_t medfilt_place_of(const real_t* sorted, uint32_t count, real_t value)
{
    if(count == 0u)
    {
        return 0u;
    }

    uint32_t place = binarysearch_get_index((real_t*)sorted, value, count);

    if((place == (count - 1u)) && (sorted[count - 1u] < value))
    {
        place = count;
    }

    return place;
}

static void medfilt_insert(real_t* sorted, uint32_t count, real_t value)
{
    uint32_t place = medfilt_place_of(sorted, count, value);

    for(uint32_t index = count; index > place; index--)
    {
        sorted[index] = sorted[index - 1u];
    }

    sorted[place] = value;
}

static void medfilt_remove(real_t* sorted, uint32_t count, real_t value)
{
    // The value is known to stand in the list, thus the search always finds
    // it and the exception above cannot arise here. Where the same value
    // stands more than once, the first of them is taken, and that is the same
    // as taking any of them.
    uint32_t place = medfilt_place_of(sorted, count, value);

    if(place >= count)
    {
        return;
    }

    for(uint32_t index = place; (index + 1u) < count; index++)
    {
        sorted[index] = sorted[index + 1u];
    }
}

medfilt_t medfilt_alloc(uint32_t size)
{
    ASSERT(size > 0);

    medfilt_t medfilt;

    medfilt.window = ringbuf_alloc(size);
    medfilt.sorted = (real_t*)malloc(sizeof(real_t)*size);
    medfilt.dynamic_alloc = true;

    for(uint32_t index = 0; index < size; index++)
    {
        medfilt.sorted[index] = REAL_C(0.0);
    }

    return medfilt;
}

medfilt_t medfilt_static_alloc(uint32_t size, real_t* window, real_t* sorted)
{
    ASSERT(size > 0);
    ASSERT(window != NULL);
    ASSERT(sorted != NULL);

    medfilt_t medfilt;

    medfilt.window = ringbuf_static_alloc(size, window);
    medfilt.sorted = sorted;
    medfilt.dynamic_alloc = false;

    for(uint32_t index = 0; index < size; index++)
    {
        medfilt.sorted[index] = REAL_C(0.0);
    }

    return medfilt;
}

void medfilt_reset(medfilt_t* medfilt)
{
    ASSERT(medfilt != NULL);

    ringbuf_reset(&medfilt->window);

    for(uint32_t index = 0; index < medfilt->window.size; index++)
    {
        medfilt->sorted[index] = REAL_C(0.0);
    }
}

real_t medfilt_process_sample(medfilt_t* medfilt, real_t sample)
{
    ASSERT(medfilt != NULL);

    uint32_t count = ringbuf_count(&medfilt->window);

    // Take the oldest sample out of the ordered list before it is pushed out
    // of the window, while it can still be found.
    if(ringbuf_is_full(&medfilt->window))
    {
        real_t leaving = ringbuf_get(&medfilt->window, count - 1u);
        medfilt_remove(medfilt->sorted, count, leaving);
        count--;
    }

    medfilt_insert(medfilt->sorted, count, sample);
    ringbuf_put(&medfilt->window, sample);

    return medfilt_get_median(medfilt);
}

void medfilt_process_block(medfilt_t* medfilt, const real_t* input, real_t* output,
                           uint32_t size)
{
    ASSERT(medfilt != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = medfilt_process_sample(medfilt, input[index]);
    }
}

real_t medfilt_get_median(medfilt_t* medfilt)
{
    ASSERT(medfilt != NULL);

    uint32_t count = ringbuf_count(&medfilt->window);

    if(count == 0u)
    {
        return REAL_C(0.0);
    }

    uint32_t middle = count / 2u;

    if((count % 2u) == 1u)
    {
        return medfilt->sorted[middle];
    }

    return (medfilt->sorted[middle - 1u] + medfilt->sorted[middle]) / REAL_C(2.0);
}

real_t medfilt_get_percentile(medfilt_t* medfilt, real_t part)
{
    ASSERT(medfilt != NULL);

    uint32_t count = ringbuf_count(&medfilt->window);

    if(count == 0u)
    {
        return REAL_C(0.0);
    }
    if(count == 1u)
    {
        return medfilt->sorted[0];
    }
    if(part <= REAL_C(0.0))
    {
        return medfilt->sorted[0];
    }
    if(part >= REAL_C(1.0))
    {
        return medfilt->sorted[count - 1u];
    }

    real_t place = part * (real_t)(count - 1u);
    uint32_t below = (uint32_t)place;
    real_t between = place - (real_t)below;

    if((below + 1u) >= count)
    {
        return medfilt->sorted[count - 1u];
    }

    return medfilt->sorted[below]
           + (between * (medfilt->sorted[below + 1u] - medfilt->sorted[below]));
}

uint32_t medfilt_count(const medfilt_t* medfilt)
{
    ASSERT(medfilt != NULL);

    return ringbuf_count(&medfilt->window);
}

bool medfilt_is_full(const medfilt_t* medfilt)
{
    ASSERT(medfilt != NULL);

    return ringbuf_is_full(&medfilt->window);
}

void medfilt_free(medfilt_t* medfilt)
{
    ASSERT(medfilt != NULL);

    ringbuf_free(&medfilt->window);

    if(medfilt->dynamic_alloc)
    {
        free(medfilt->sorted);
        medfilt->sorted = NULL;
        medfilt->dynamic_alloc = false;
    }
}
