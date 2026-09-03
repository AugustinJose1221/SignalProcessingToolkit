// This file is left out of the build when FFITT_NO_FILTER is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_FILTER

#ifndef TEST
#include <ffitt/filter/hampel.h>
#include <ffitt/util/stats.h>
#include <ffitt/core/defs.h>
#else
#include "hampel.h"
#include "stats.h"
#include "defs.h"
#endif

#include <math.h>

bool hampel_is_valid_window(uint32_t window)
{
    // Odd, so that the window has a true middle. At least three, so that the
    // sample in the middle has a neighbour on each side to be judged against.
    return (window >= 3u) && ((window % 2u) == 1u);
}

hampel_t hampel_alloc(uint32_t window)
{
    ASSERT(hampel_is_valid_window(window));

    hampel_t hampel;

    hampel.middle = medfilt_alloc(window);
    hampel.history = ringbuf_alloc(window);
    hampel.distance = (real_t*)malloc(sizeof(real_t)*window);
    hampel.threshold = HAMPEL_THRESHOLD;
    hampel.replaced = 0;
    hampel.seen = 0;
    hampel.dynamic_alloc = true;

    // The middle and the history are held by modules of their own and answer
    // for themselves: a size of nothing says they got nothing.
    if((hampel.distance == NULL) || (hampel.history.size == 0u)
       || (hampel.middle.sorted == NULL))
    {
        hampel_free(&hampel);

        hampel.distance = NULL;
        hampel.dynamic_alloc = false;
    }

    return hampel;
}

hampel_t hampel_static_alloc(uint32_t window, real_t* sorted, real_t* ordered,
                             real_t* history, real_t* distance)
{
    ASSERT(hampel_is_valid_window(window));
    ASSERT(sorted != NULL);
    ASSERT(ordered != NULL);
    ASSERT(history != NULL);
    ASSERT(distance != NULL);

    hampel_t hampel;

    hampel.middle = medfilt_static_alloc(window, ordered, sorted);
    hampel.history = ringbuf_static_alloc(window, history);
    hampel.distance = distance;
    hampel.threshold = HAMPEL_THRESHOLD;
    hampel.replaced = 0;
    hampel.seen = 0;
    hampel.dynamic_alloc = false;

    return hampel;
}

bool hampel_set_threshold(hampel_t* hampel, real_t threshold)
{
    ASSERT(hampel != NULL);

    if(threshold <= REAL_C(0.0))
    {
        return false;
    }

    hampel->threshold = threshold;

    return true;
}

void hampel_reset(hampel_t* hampel)
{
    ASSERT(hampel != NULL);

    medfilt_reset(&hampel->middle);
    ringbuf_reset(&hampel->history);
    hampel->replaced = 0;
    hampel->seen = 0;
}

uint32_t hampel_delay(const hampel_t* hampel)
{
    ASSERT(hampel != NULL);

    return hampel->history.size / 2u;
}

real_t hampel_process_sample(hampel_t* hampel, real_t sample,
                             bool* was_replaced)
{
    ASSERT(hampel != NULL);

    if(was_replaced != NULL)
    {
        *was_replaced = false;
    }

    real_t centre = medfilt_process_sample(&hampel->middle, sample);
    ringbuf_put(&hampel->history, sample);
    hampel->seen++;

    uint32_t window = hampel->history.size;
    uint32_t half = window / 2u;

    // Until the window is full there are not enough neighbours on both sides
    // to judge anything, thus the samples pass through as they came.
    if(ringbuf_count(&hampel->history) < window)
    {
        return ringbuf_get(&hampel->history, ringbuf_count(&hampel->history) - 1u);
    }

    // The sample in the middle of the window is the one being judged. Every
    // other sample in the window stands on one side of it or the other.
    real_t judged = ringbuf_get(&hampel->history, half);

    // How far each sample of the window stands from the middle of them all.
    // The median of those distances is the median absolute deviation, and the
    // header says why it and not a standard deviation.
    for(uint32_t age = 0; age < window; age++)
    {
        hampel->distance[age] = REAL_ABS(ringbuf_get(&hampel->history, age)
                                         - centre);
    }

    real_t spread = stats_median(hampel->distance, window) * HAMPEL_SCALE;
    real_t distance = REAL_ABS(judged - centre);

    // A spread of nothing means every sample of the window is the same value.
    // Then any sample that differs at all is wrong, and one that does not
    // differ is right.
    bool wrong = (spread > REAL_C(0.0))
                 ? (distance > (hampel->threshold * spread))
                 : (distance > REAL_C(0.0));

    if(wrong)
    {
        hampel->replaced++;
        if(was_replaced != NULL)
        {
            *was_replaced = true;
        }
        return centre;
    }

    return judged;
}

uint32_t hampel_process_block(hampel_t* hampel, const real_t* input,
                              real_t* output, uint32_t size)
{
    ASSERT(hampel != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    uint32_t delay = hampel_delay(hampel);
    uint32_t before = hampel->replaced;

    // The filter answers late, thus the first answers belong to samples that
    // came before the start of this block and are thrown away. Reading past
    // the end of the input is not allowed, so the tail is fed the last sample
    // again: those answers are only used to push the real ones out.
    for(uint32_t index = 0; index < (size + delay); index++)
    {
        real_t sample = (index < size) ? input[index] : input[size - 1u];
        real_t answer = hampel_process_sample(hampel, sample, NULL);

        if(index >= delay)
        {
            output[index - delay] = answer;
        }
    }

    // The samples at the two ends never had neighbours on both sides. Give
    // them back as they arrived rather than as something judged against a
    // window that was not there.
    for(uint32_t index = 0; (index < delay) && (index < size); index++)
    {
        output[index] = input[index];
        output[size - 1u - index] = input[size - 1u - index];
    }

    return hampel->replaced - before;
}

uint32_t hampel_replaced_count(const hampel_t* hampel)
{
    ASSERT(hampel != NULL);

    return hampel->replaced;
}

void hampel_free(hampel_t* hampel)
{
    ASSERT(hampel != NULL);

    medfilt_free(&hampel->middle);
    ringbuf_free(&hampel->history);

    if(hampel->dynamic_alloc)
    {
        free(hampel->distance);
        hampel->distance = NULL;
        hampel->dynamic_alloc = false;
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int hampel_is_not_in_this_build_t;

#endif//FFITT_NO_FILTER
