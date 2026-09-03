// This file is left out of the build when FFITT_NO_FILTER is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_FILTER

#ifndef TEST
#include <ffitt/filter/resample.h>
#include <ffitt/core/defs.h>
#else
#include "resample.h"
#include "defs.h"
#endif

#include <math.h>

// Where the filter is cut, as a part of the rate it runs at.
//
// Half the new rate is where the aliases begin. The cut stands a little below
// that, so that the turn of the filter is finished before the aliases start
// rather than sitting on top of them.
#define RESAMPLE_CUTOFF_PART    REAL_C(0.45)

// How many coefficients for each factor, as a rule of thumb.
//
// The pass band must end near 0.4 of the new rate and the stop band begin at
// 0.5 of it, thus the turn is 0.1 of the new rate, which is 0.1/factor of the
// rate the filter runs at. A window of Hamming needs about 3.3/turn
// coefficients to make a turn that narrow.
#define RESAMPLE_PER_FACTOR     33u

uint32_t resample_advised_length(uint32_t factor)
{
    if(factor < 2u)
    {
        return 0u;
    }

    uint32_t length = RESAMPLE_PER_FACTOR * factor;

    // The length must be odd, so that the filter has a middle coefficient and
    // delays every frequency by the same time.
    if((length % 2u) == 0u)
    {
        length++;
    }

    return length;
}

bool resample_is_valid_factor(uint32_t factor)
{
    return factor >= 2u;
}

// Build a resampler whose filter is cut for the given factor.
//
// The history holds input samples, and how many depends on which way the rate
// is going. A decimator reads one coefficient for each sample it holds. An
// interpolator reads only every factor-th coefficient for each sample, thus it
// needs factor times fewer of them.
// The design, which is the same whichever road the memory came by. The filter
// and the buffer are already built when this is called; this only decides what
// goes in them, thus the two roads cannot drift apart.
static void resample_design(resample_t* resample, uint32_t factor,
                            uint32_t length, real_t gain)
{
    resample->factor = factor;
    resample->phase = 0;

    fir_design_low_pass(&resample->filter,
                        RESAMPLE_CUTOFF_PART / (real_t)factor);

    // An interpolator puts zeros between the samples, and zeros carry no
    // energy. Each output then comes out factor times too small, thus the
    // coefficients are made factor times larger once, here, rather than the
    // answer being multiplied for every sample.
    if(gain != REAL_C(1.0))
    {
        for(uint32_t index = 0; index < length; index++)
        {
            fir_set_coefficient(&resample->filter, index,
                                gain * fir_get_coefficient(&resample->filter,
                                                           index));
        }
    }
}

static resample_t resample_build(uint32_t factor, uint32_t length,
                                 uint32_t history_size, real_t gain)
{
    resample_t resample;

    resample.filter = fir_alloc(length);
    resample.history = ringbuf_alloc(history_size);
    resample.factor = factor;
    resample.phase = 0;
    resample.dynamic_alloc = true;

    // The two below answer for themselves: a length or a size of nothing says
    // the heap gave nothing. The design writes through both of them, thus it
    // must not be reached with nothing to write to.
    if((resample.filter.length == 0u) || (resample.history.size == 0u))
    {
        resample_free(&resample);

        resample.factor = 0;
        resample.phase = 0;
        resample.dynamic_alloc = false;

        return resample;
    }

    resample_design(&resample, factor, length, gain);

    return resample;
}

// The memory of a resampler, carved out of the one list the caller gave.
//
// The filter takes its coefficients and its history, the length each. What is
// left over is the buffer of samples at the input rate, and how much that
// needs is the one thing that differs between a decimator and an
// interpolator.
static resample_t resample_build_static(uint32_t factor, uint32_t length,
                                        uint32_t history_size, real_t gain,
                                        real_t* mempool)
{
    resample_t resample;

    resample.filter = fir_static_alloc(length, &mempool[0], &mempool[length]);
    resample.history = ringbuf_static_alloc(history_size,
                                            &mempool[2u * length]);
    resample.dynamic_alloc = false;

    resample_design(&resample, factor, length, gain);

    return resample;
}

resample_t resample_alloc_decimator(uint32_t factor, uint32_t length)
{
    ASSERT(resample_is_valid_factor(factor));
    ASSERT(length > 0);
    ASSERT((length % 2u) == 1u);

    return resample_build(factor, length, length, REAL_C(1.0));
}

resample_t resample_alloc_interpolator(uint32_t factor, uint32_t length)
{
    ASSERT(resample_is_valid_factor(factor));
    ASSERT(length > 0);
    ASSERT((length % 2u) == 1u);

    // One input sample feeds every factor-th coefficient, thus the history
    // needs the length divided by the factor, rounded up.
    uint32_t needed = ((length + factor) - 1u) / factor;

    return resample_build(factor, length, needed, (real_t)factor);
}

resample_t resample_static_alloc_decimator(uint32_t factor, uint32_t length,
                                           real_t* mempool)
{
    ASSERT(resample_is_valid_factor(factor));
    ASSERT(length > 0);
    ASSERT((length % 2u) == 1u);
    ASSERT(mempool != NULL);

    return resample_build_static(factor, length, length, REAL_C(1.0), mempool);
}

resample_t resample_static_alloc_interpolator(uint32_t factor, uint32_t length,
                                              real_t* mempool)
{
    ASSERT(resample_is_valid_factor(factor));
    ASSERT(length > 0);
    ASSERT((length % 2u) == 1u);
    ASSERT(mempool != NULL);

    uint32_t needed = ((length + factor) - 1u) / factor;

    return resample_build_static(factor, length, needed, (real_t)factor,
                                 mempool);
}

void resample_reset(resample_t* resample)
{
    ASSERT(resample != NULL);

    ringbuf_reset(&resample->history);
    fir_reset(&resample->filter);
    resample->phase = 0;
}

bool resample_decimate(resample_t* resample, real_t sample, real_t* output)
{
    ASSERT(resample != NULL);
    ASSERT(output != NULL);

    ringbuf_put(&resample->history, sample);
    resample->phase++;

    if(resample->phase < resample->factor)
    {
        return false;
    }
    resample->phase = 0;

    // Here is the whole saving. The filter is worked out ONLY for the samples
    // that are kept. A filter that ran at the input rate would work out
    // factor-1 answers out of every factor and throw them all away: for a
    // factor of 64 and a filter of 128 coefficients, 8064 multiplications
    // wasted for every 128 that are used.
    real_t total = REAL_C(0.0);
    uint32_t length = resample->filter.length;

    for(uint32_t index = 0; index < length; index++)
    {
        total += fir_get_coefficient(&resample->filter, index)
                 * ringbuf_get(&resample->history, index);
    }

    *output = total;

    return true;
}

uint32_t resample_interpolate(resample_t* resample, real_t sample,
                              real_t* output)
{
    ASSERT(resample != NULL);
    ASSERT(output != NULL);

    ringbuf_put(&resample->history, sample);

    uint32_t factor = resample->factor;
    uint32_t length = resample->filter.length;

    // The same saving the other way round. Between two input samples the
    // filter would be fed zeros, and a zero multiplied by a coefficient is
    // nothing. Each output therefore reads only every factor-th coefficient,
    // and which ones depends on where that output falls between the samples.
    for(uint32_t place = 0; place < factor; place++)
    {
        real_t total = REAL_C(0.0);
        uint32_t age = 0;

        for(uint32_t index = place; index < length; index += factor)
        {
            total += fir_get_coefficient(&resample->filter, index)
                     * ringbuf_get(&resample->history, age);
            age++;
        }

        output[place] = total;
    }

    return factor;
}

uint32_t resample_decimate_block(resample_t* resample, const real_t* input,
                                 real_t* output, uint32_t size)
{
    ASSERT(resample != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    uint32_t written = 0;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t sample;

        if(resample_decimate(resample, input[index], &sample))
        {
            output[written] = sample;
            written++;
        }
    }

    return written;
}

uint32_t resample_interpolate_block(resample_t* resample, const real_t* input,
                                    real_t* output, uint32_t size)
{
    ASSERT(resample != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    uint32_t written = 0;

    for(uint32_t index = 0; index < size; index++)
    {
        written += resample_interpolate(resample, input[index],
                                        &output[written]);
    }

    return written;
}

uint32_t resample_delay(const resample_t* resample)
{
    ASSERT(resample != NULL);

    // A filter with a middle delays by half its length, counted at the rate
    // the filter runs at. For a decimator that rate is the input rate, thus
    // the delay in output samples is smaller by the factor.
    uint32_t half = resample->filter.length / 2u;

    return half / resample->factor;
}

void resample_free(resample_t* resample)
{
    ASSERT(resample != NULL);

    fir_free(&resample->filter);
    ringbuf_free(&resample->history);
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int resample_is_not_in_this_build_t;

#endif//FFITT_NO_FILTER
