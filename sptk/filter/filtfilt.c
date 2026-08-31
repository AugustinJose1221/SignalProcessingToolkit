#ifndef TEST
#include <sptk/filter/filtfilt.h>
#include <sptk/core/defs.h>
#else
#include "filtfilt.h"
#include "defs.h"
#endif

#include <math.h>

// How many times a filter with feedback is fed one value before it is taken
// to have settled, at the most.
//
// The carried part of the signal alone is not enough. It is three times the
// state of the filter, which is a dozen samples for two sections, and a filter
// with a low cutoff takes thousands of samples to answer a step. Starting from
// nothing, the answer to the first sample would then still be swinging when
// the real samples began, and running both ways would put that swing at BOTH
// ends.
//
// So the filter is first fed the value it is about to meet, again and again,
// until its answer stops moving. That says: assume the signal stood at this
// value for ever before now. It is the same thing the dcblock module does with
// its level, and for the same reason.
//
// The cap is generous because the number of samples needed follows 1/cutoff,
// and the lowest cutoff the library allows at 64 bits is a millionth.
#define FILTFILT_SETTLE_MAX     1000000u

// How still the answer must be, and for how many samples in a row, before the
// filter counts as settled. Asking for one still sample would stop at any
// turning point of a filter that is still ringing.
#define FILTFILT_SETTLE_PART    REAL_C(1.0e-6)
#define FILTFILT_SETTLE_RUN     8u

// WHY THE STILLNESS IS MEASURED AGAINST THE SIGNAL AND NOTHING ELSE.
//
// The question is whether the answer has stopped moving, and the only honest
// measure of that is how far it moved COMPARED WITH HOW LARGE IT IS. A test
// that added a fixed amount to the comparison would ask less of a small signal
// than of a large one, and the module would then give a different shape for
// the same measurement read in volts and in millivolts.
//
// That was measured and it was not small. A low pass at a cutoff of 0.02 whose
// answer sat near a thousandth came back with a shape that differed by 2 parts
// in 100 from the same signal at full size. The units a caller chose changed
// the reading.
//
// The smallest number the width can hold is added so that a level of exactly
// zero settles rather than waiting for ever. It is far below anything a signal
// can be, thus it changes nothing else.
#define FILTFILT_SETTLE_FLOOR   REAL_SMALLEST

static void filtfilt_settle_iir(iir_t* iir, real_t level)
{
    real_t previous = iir_process_sample(iir, level);
    uint32_t still = 0;

    for(uint32_t step = 0; step < FILTFILT_SETTLE_MAX; step++)
    {
        real_t now = iir_process_sample(iir, level);
        real_t moved = REAL_ABS(now - previous);
        real_t size_of = REAL_ABS(now) + REAL_ABS(level);

        if(moved <= (FILTFILT_SETTLE_PART * (size_of + FILTFILT_SETTLE_FLOOR)))
        {
            still++;
            if(still >= FILTFILT_SETTLE_RUN)
            {
                return;
            }
        }
        else
        {
            still = 0;
        }

        previous = now;
    }
}

// A filter with no feedback holds only the samples it was given, thus it is
// settled exactly when it has been fed its own length and no sooner. There is
// nothing to wait for and nothing to measure.
static void filtfilt_settle_fir(fir_t* fir, real_t level)
{
    for(uint32_t step = 0; step < fir->length; step++)
    {
        fir_process_sample(fir, level);
    }
}

uint32_t filtfilt_padding(uint32_t filter_size, uint32_t size)
{
    if(size == 0u)
    {
        return 0u;
    }

    uint32_t wanted = 3u * filter_size;

    // The signal cannot be carried further past its end than it is long.
    if(wanted > (size - 1u))
    {
        wanted = size - 1u;
    }

    return wanted;
}

real_t filtfilt_iir_gain(iir_t* iir, real_t frequency)
{
    ASSERT(iir != NULL);

    real_t once = iir_get_gain(iir, frequency);

    return once * once;
}

real_t filtfilt_fir_gain(fir_t* fir, real_t frequency)
{
    ASSERT(fir != NULL);

    real_t once = fir_get_gain(fir, frequency);

    return once * once;
}

// The sample that would stand the given number of places before the start, if
// the signal were carried on past it.
//
// The signal is turned about its first sample: the sample one place before the
// first stands as far ABOVE the first as the second stands below it. Thus the
// carried part meets the real part with no step and no corner, and a filter
// running over it is settled by the time the real samples begin.
//
// A simple mirror would meet it with a corner, and repeating the first sample
// would meet it with a change of slope. Turning it about the end does neither.
static real_t filtfilt_before(const real_t* data, uint32_t place)
{
    return (REAL_C(2.0) * data[0]) - data[place];
}

bool filtfilt_iir(iir_t* iir, const real_t* input, real_t* output,
                  uint32_t size)
{
    ASSERT(iir != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    // Two samples of state for each section. A signal shorter than that cannot
    // fill the filter, thus there is nothing to be gained by running it.
    uint32_t state = 2u * iir->sections;

    if(size <= state)
    {
        return false;
    }

    uint32_t pad = filtfilt_padding(state, size);

    // Forwards. The filter is first settled at the value it is about to meet,
    // then fed the carried part, then the signal itself.
    iir_reset(iir);
    filtfilt_settle_iir(iir, input[0]);
    for(uint32_t step = pad; step >= 1u; step--)
    {
        iir_process_sample(iir, filtfilt_before(input, step));
    }
    for(uint32_t index = 0; index < size; index++)
    {
        // Read before writing, so that the input and the output may be the
        // same list.
        real_t sample = input[index];
        output[index] = iir_process_sample(iir, sample);
    }

    // Backwards over the same memory. The end of the signal is now the start,
    // thus the signal is carried past THAT end in the same way.
    iir_reset(iir);
    filtfilt_settle_iir(iir, output[size - 1u]);
    for(uint32_t step = pad; step >= 1u; step--)
    {
        real_t carried = (REAL_C(2.0) * output[size - 1u])
                         - output[size - 1u - step];
        iir_process_sample(iir, carried);
    }
    for(uint32_t index = size; index >= 1u; index--)
    {
        real_t sample = output[index - 1u];
        output[index - 1u] = iir_process_sample(iir, sample);
    }

    return true;
}

bool filtfilt_fir(fir_t* fir, const real_t* input, real_t* output,
                  uint32_t size)
{
    ASSERT(fir != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(size <= fir->length)
    {
        return false;
    }

    uint32_t pad = filtfilt_padding(fir->length, size);

    fir_reset(fir);
    filtfilt_settle_fir(fir, input[0]);
    for(uint32_t step = pad; step >= 1u; step--)
    {
        fir_process_sample(fir, filtfilt_before(input, step));
    }
    for(uint32_t index = 0; index < size; index++)
    {
        real_t sample = input[index];
        output[index] = fir_process_sample(fir, sample);
    }

    fir_reset(fir);
    filtfilt_settle_fir(fir, output[size - 1u]);
    for(uint32_t step = pad; step >= 1u; step--)
    {
        real_t carried = (REAL_C(2.0) * output[size - 1u])
                         - output[size - 1u - step];
        fir_process_sample(fir, carried);
    }
    for(uint32_t index = size; index >= 1u; index--)
    {
        real_t sample = output[index - 1u];
        output[index - 1u] = fir_process_sample(fir, sample);
    }

    return true;
}
