#ifndef TEST
#include <dwt/dwt.h>
#include <common/defs.h>
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
        float root = sqrtf(3.0f);
        float divisor = 4.0f * sqrtf(2.0f);

        dwt.length = 4;
        dwt.low[0] = (1.0f + root) / divisor;
        dwt.low[1] = (3.0f + root) / divisor;
        dwt.low[2] = (3.0f - root) / divisor;
        dwt.low[3] = (1.0f - root) / divisor;
    }
    else
    {
        // The two coefficients of Haar, which give the mean and the difference
        // of two samples.
        float value = 1.0f / sqrtf(2.0f);

        dwt.length = 2;
        dwt.low[0] = value;
        dwt.low[1] = value;
        dwt.low[2] = 0.0f;
        dwt.low[3] = 0.0f;
    }

    // The filter of the detail comes from the filter of the approximation:
    // turn the order around and change the sign of every second one.
    for(uint32_t index = 0; index < dwt.length; index++)
    {
        float sign = ((index % 2) == 0) ? 1.0f : -1.0f;
        dwt.high[index] = sign * dwt.low[dwt.length - 1 - index];
    }
    for(uint32_t index = dwt.length; index < DWT_MAX_COEFFICIENT_COUNT; index++)
    {
        dwt.high[index] = 0.0f;
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

void dwt_forward(dwt_t* dwt, const float* signal, uint32_t size,
                 float* approximation, float* detail)
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
        float low_sum = 0.0f;
        float high_sum = 0.0f;

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

void dwt_inverse(dwt_t* dwt, const float* approximation, const float* detail,
                 uint32_t size, float* signal)
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
        signal[index] = 0.0f;
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

void dwt_forward_multi(dwt_t* dwt, float* signal, uint32_t size, uint32_t levels,
                       float* work)
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

void dwt_inverse_multi(dwt_t* dwt, float* signal, uint32_t size, uint32_t levels,
                       float* work)
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

void dwt_threshold(float* data, uint32_t size, float limit)
{
    ASSERT(data != NULL);
    ASSERT(limit >= 0.0f);

    for(uint32_t index = 0; index < size; index++)
    {
        if(fabsf(data[index]) < limit)
        {
            data[index] = 0.0f;
        }
    }
}
