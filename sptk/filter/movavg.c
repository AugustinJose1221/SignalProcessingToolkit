#ifndef TEST
#include <sptk/filter/movavg.h>
#include <sptk/core/defs.h>
#else
#include "movavg.h"
#include "defs.h"
#endif

#include <math.h>

// Build the totals again from the samples that the window holds.
static void movavg_refresh(movavg_t* movavg)
{
    double total = 0.0;
    double squares = 0.0;
    uint32_t count = ringbuf_count(&movavg->window);

    for(uint32_t age = 0; age < count; age++)
    {
        double sample = (double)ringbuf_get(&movavg->window, age);
        total += sample;
        squares += sample * sample;
    }

    movavg->total = total;
    movavg->square_total = squares;
    movavg->since_refresh = 0;
}

movavg_t movavg_alloc(uint32_t size)
{
    ASSERT(size > 0);

    movavg_t movavg;

    movavg.window = ringbuf_alloc(size);
    movavg.total = 0.0;
    movavg.square_total = 0.0;
    movavg.since_refresh = 0;

    return movavg;
}

movavg_t movavg_static_alloc(uint32_t size, float* data)
{
    ASSERT(size > 0);
    ASSERT(data != NULL);

    movavg_t movavg;

    movavg.window = ringbuf_static_alloc(size, data);
    movavg.total = 0.0;
    movavg.square_total = 0.0;
    movavg.since_refresh = 0;

    return movavg;
}

void movavg_reset(movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    ringbuf_reset(&movavg->window);
    movavg->total = 0.0;
    movavg->square_total = 0.0;
    movavg->since_refresh = 0;
}

float movavg_process_sample(movavg_t* movavg, float sample)
{
    ASSERT(movavg != NULL);

    // The sample that is about to fall off the end, if the window is full.
    if(ringbuf_is_full(&movavg->window))
    {
        double leaving = (double)ringbuf_get(&movavg->window,
                                             ringbuf_count(&movavg->window) - 1u);
        movavg->total -= leaving;
        movavg->square_total -= leaving * leaving;
    }

    movavg->total += (double)sample;
    movavg->square_total += (double)sample * (double)sample;

    ringbuf_put(&movavg->window, sample);

    // Build the totals again from time to time.
    //
    // A total that is added to and taken away from for ever gathers a small
    // error at every step. Each step is exact only while the total is small
    // enough to hold every digit of the sample, and a long run of large
    // samples is not. The error does not cancel: it walks, and after millions
    // of samples the mean can be visibly wrong.
    //
    // A double makes each step far more accurate but does not make it exact.
    // Building the totals again from the window does. It costs one pass over
    // the window every MOVAVG_REFRESH samples, which for a window of 500 and a
    // refresh of 4096 is about one eighth of an operation for each sample.
    movavg->since_refresh++;
    if(movavg->since_refresh >= MOVAVG_REFRESH)
    {
        movavg_refresh(movavg);
    }

    return movavg_get_mean(movavg);
}

void movavg_process_block(movavg_t* movavg, const float* input, float* output,
                          uint32_t size)
{
    ASSERT(movavg != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = movavg_process_sample(movavg, input[index]);
    }
}

float movavg_get_mean(const movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    uint32_t count = ringbuf_count(&movavg->window);

    if(count == 0u)
    {
        return 0.0f;
    }

    return (float)(movavg->total / (double)count);
}

float movavg_get_rms(const movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    uint32_t count = ringbuf_count(&movavg->window);

    if(count == 0u)
    {
        return 0.0f;
    }

    double mean_square = movavg->square_total / (double)count;

    // A sum of squares can only be positive. A very small negative number can
    // still appear from the rounding of the running total, and a root of it
    // would give a value that is not a number.
    if(mean_square < 0.0)
    {
        mean_square = 0.0;
    }

    return (float)sqrt(mean_square);
}

float movavg_get_deviation(const movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    uint32_t count = ringbuf_count(&movavg->window);

    if(count == 0u)
    {
        return 0.0f;
    }

    // Take the mean away from each sample first, then square. The header says
    // why the shorter way is wrong.
    double mean = movavg->total / (double)count;
    double total = 0.0;

    for(uint32_t age = 0; age < count; age++)
    {
        double distance = (double)ringbuf_get(&movavg->window, age) - mean;
        total += distance * distance;
    }

    return (float)sqrt(total / (double)count);
}

uint32_t movavg_count(const movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    return ringbuf_count(&movavg->window);
}

bool movavg_is_full(const movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    return ringbuf_is_full(&movavg->window);
}

void movavg_free(movavg_t* movavg)
{
    ASSERT(movavg != NULL);

    ringbuf_free(&movavg->window);
    movavg->total = 0.0;
    movavg->square_total = 0.0;
}
