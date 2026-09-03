// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/dwt.h>
#include <ffitt/core/defs.h>
#else
#include "dwt.h"
#include "defs.h"
#endif

#include <math.h>

dwt_t dwt_init(dwt_wavelet_t wavelet)
{
    dwt_t dwt;

    dwt.wavelet = wavelet;

    if(wavelet == DWT_DAUBECHIES4)
    {
        // The four coefficients of Daubechies. The value of the square root of
        // three decides them.
        real_t root = REAL_SQRT(REAL_C(3.0));
        real_t divisor = REAL_C(4.0) * REAL_SQRT(REAL_C(2.0));

        dwt.length = 4;
        dwt.low[0] = (REAL_C(1.0) + root) / divisor;
        dwt.low[1] = (REAL_C(3.0) + root) / divisor;
        dwt.low[2] = (REAL_C(3.0) - root) / divisor;
        dwt.low[3] = (REAL_C(1.0) - root) / divisor;
    }
    else
    {
        // The two coefficients of Haar, which give the mean and the difference
        // of two samples.
        real_t value = REAL_C(1.0) / REAL_SQRT(REAL_C(2.0));

        dwt.length = 2;
        dwt.low[0] = value;
        dwt.low[1] = value;
        dwt.low[2] = REAL_C(0.0);
        dwt.low[3] = REAL_C(0.0);
    }

    // The filter of the detail comes from the filter of the approximation:
    // turn the order around and change the sign of every second one.
    for(uint32_t index = 0; index < dwt.length; index++)
    {
        real_t sign = ((index % 2) == 0) ? REAL_C(1.0) : -REAL_C(1.0);
        dwt.high[index] = sign * dwt.low[dwt.length - 1 - index];
    }
    for(uint32_t index = dwt.length; index < DWT_MAX_COEFFICIENT_COUNT; index++)
    {
        dwt.high[index] = REAL_C(0.0);
    }

    return dwt;
}

bool dwt_is_valid_size(uint32_t size, uint32_t levels)
{
    if(levels == 0)
    {
        return false;
    }

    uint32_t current = size;

    for(uint32_t level = 0; level < levels; level++)
    {
        if((current < 2) || ((current % 2) != 0))
        {
            return false;
        }
        current /= 2;
    }

    return true;
}

void dwt_forward(dwt_t* dwt, const real_t* signal, uint32_t size,
                 real_t* approximation, real_t* detail)
{
    ASSERT(dwt != NULL);
    ASSERT(signal != NULL);
    ASSERT(approximation != NULL);
    ASSERT(detail != NULL);
    ASSERT(size >= 2);
    ASSERT((size % 2) == 0);

    uint32_t half = size / 2;

    for(uint32_t index = 0; index < half; index++)
    {
        real_t low_sum = REAL_C(0.0);
        real_t high_sum = REAL_C(0.0);

        for(uint32_t tap = 0; tap < dwt->length; tap++)
        {
            // The signal goes round at its end. Thus the transform needs no
            // rule for the edge, and the inverse gives the signal back exactly.
            uint32_t position = ((2 * index) + tap) % size;

            low_sum += dwt->low[tap] * signal[position];
            high_sum += dwt->high[tap] * signal[position];
        }

        approximation[index] = low_sum;
        detail[index] = high_sum;
    }
}

void dwt_inverse(dwt_t* dwt, const real_t* approximation, const real_t* detail,
                 uint32_t size, real_t* signal)
{
    ASSERT(dwt != NULL);
    ASSERT(approximation != NULL);
    ASSERT(detail != NULL);
    ASSERT(signal != NULL);
    ASSERT(size >= 2);
    ASSERT((size % 2) == 0);

    uint32_t half = size / 2;

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = REAL_C(0.0);
    }

    // Each value of the approximation and of the detail spreads back over the
    // places that it came from.
    for(uint32_t index = 0; index < half; index++)
    {
        for(uint32_t tap = 0; tap < dwt->length; tap++)
        {
            uint32_t position = ((2 * index) + tap) % size;

            signal[position] += (dwt->low[tap] * approximation[index])
                                + (dwt->high[tap] * detail[index]);
        }
    }
}

void dwt_forward_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels,
                       real_t* work)
{
    ASSERT(dwt != NULL);
    ASSERT(signal != NULL);
    ASSERT(work != NULL);
    ASSERT(dwt_is_valid_size(size, levels));

    uint32_t current = size;

    for(uint32_t level = 0; level < levels; level++)
    {
        uint32_t half = current / 2;

        // The approximation goes to the front of the work buffer and the
        // detail after it. The next level then works on the front only.
        dwt_forward(dwt, signal, current, work, &work[half]);

        for(uint32_t index = 0; index < current; index++)
        {
            signal[index] = work[index];
        }

        current = half;
    }
}

void dwt_inverse_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels,
                       real_t* work)
{
    ASSERT(dwt != NULL);
    ASSERT(signal != NULL);
    ASSERT(work != NULL);
    ASSERT(dwt_is_valid_size(size, levels));

    uint32_t smallest = size;

    for(uint32_t level = 0; level < levels; level++)
    {
        smallest /= 2;
    }

    // Walk back up, from the smallest level to the whole signal.
    for(uint32_t current = smallest * 2; current <= size; current *= 2)
    {
        uint32_t half = current / 2;

        dwt_inverse(dwt, signal, &signal[half], current, work);

        for(uint32_t index = 0; index < current; index++)
        {
            signal[index] = work[index];
        }
    }
}

void dwt_threshold(real_t* data, uint32_t size, real_t limit)
{
    ASSERT(data != NULL);
    ASSERT(limit >= REAL_C(0.0));

    for(uint32_t index = 0; index < size; index++)
    {
        if(REAL_ABS(data[index]) < limit)
        {
            data[index] = REAL_C(0.0);
        }
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int dwt_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
