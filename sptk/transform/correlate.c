#ifndef TEST
#include <sptk/transform/correlate.h>
#include <sptk/transform/fft.h>
#include <sptk/core/defs.h>
#else
#include "correlate.h"
#include "fft.h"
#include "defs.h"
#endif

#include <math.h>

// The sum of the products of two signals, one of them moved by the lag.
//
// The sum runs in real_t like everything else. The header of the stats module
// says what a long sum costs at the narrower width, and the same holds here.
static real_t correlate_sum(const real_t* a, const real_t* b, uint32_t size,
                            uint32_t lag, real_t mean_a, real_t mean_b)
{
    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; (index + lag) < size; index++)
    {
        total += (a[index] - mean_a) * (b[index + lag] - mean_b);
    }

    return total;
}

// Divide a sum by whatever the scaling asks for.
static real_t correlate_scale(real_t total, uint32_t size, uint32_t lag,
                              real_t divisor, correlate_scaling_t scaling)
{
    (void)divisor;

    switch(scaling)
    {
        case CORRELATE_BIASED:
            return total / (real_t)size;

        case CORRELATE_UNBIASED:
            // Only size-lag samples overlapped at this lag, thus dividing by
            // the whole size would make a long lag look weaker than it is.
            return total / (real_t)(size - lag);

        case CORRELATE_RAW:
        default:
            return total;
    }
}

bool correlate_is_valid_scaling(correlate_scaling_t scaling)
{
    return (scaling >= CORRELATE_RAW) && (scaling <= CORRELATE_COEFFICIENT);
}

// The correlation coefficient of the two signals at one lag, worked out over
// the samples that overlap AT THAT LAG and no others.
//
// This is the one scaling whose answer can be judged rather than only compared
// with another answer from the same signal. It is always between -1 and 1, and
// it does not fall away as the lag grows.
//
// The usual short way, which is the sum at each lag divided by the sum at no
// lag, does fall away: at a lag of k only size-k samples overlap, thus the
// answer is about (size-k)/size of the truth. For a lag of an eighth of the
// signal that is an eighth too small, and a threshold set on it would hold for
// one length of signal and not another. That defeats the whole reason for
// having a coefficient.
static real_t correlate_coefficient_at(const real_t* a, const real_t* b,
                                       uint32_t size, uint32_t lag)
{
    uint32_t overlap = size - lag;

    if(overlap < 2u)
    {
        return REAL_C(0.0);
    }

    real_t mean_a = REAL_C(0.0);
    real_t mean_b = REAL_C(0.0);

    for(uint32_t index = 0; index < overlap; index++)
    {
        mean_a += a[index];
        mean_b += b[index + lag];
    }
    mean_a /= (real_t)overlap;
    mean_b /= (real_t)overlap;

    real_t together = REAL_C(0.0);
    real_t energy_a = REAL_C(0.0);
    real_t energy_b = REAL_C(0.0);

    for(uint32_t index = 0; index < overlap; index++)
    {
        real_t da = a[index] - mean_a;
        real_t db = b[index + lag] - mean_b;
        together += da * db;
        energy_a += da * da;
        energy_b += db * db;
    }

    real_t divisor = REAL_SQRT(energy_a * energy_b);

    // A signal that never moves has no shape to match, thus there is no
    // likeness to report and 0 is the only honest answer.
    return (divisor > REAL_C(0.0)) ? (together / divisor) : REAL_C(0.0);
}

bool correlate_cross(const real_t* a, const real_t* b, uint32_t size,
                     real_t* output, uint32_t max_lag,
                     correlate_scaling_t scaling)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(output != NULL);

    if((size == 0u) || (max_lag >= size) || !correlate_is_valid_scaling(scaling))
    {
        return false;
    }

    for(uint32_t lag = 0; lag <= max_lag; lag++)
    {
        if(scaling == CORRELATE_COEFFICIENT)
        {
            output[lag] = correlate_coefficient_at(a, b, size, lag);
        }
        else
        {
            real_t total = correlate_sum(a, b, size, lag,
                                         REAL_C(0.0), REAL_C(0.0));
            output[lag] = correlate_scale(total, size, lag, REAL_C(0.0),
                                          scaling);
        }
    }

    return true;
}

bool correlate_auto(const real_t* data, uint32_t size, real_t* output,
                    uint32_t max_lag, correlate_scaling_t scaling)
{
    return correlate_cross(data, data, size, output, max_lag, scaling);
}

uint32_t correlate_best_lag(const real_t* data, uint32_t size, real_t* output,
                            uint32_t low_lag, uint32_t high_lag,
                            real_t* strength)
{
    ASSERT(data != NULL);
    ASSERT(output != NULL);

    if(strength != NULL)
    {
        *strength = REAL_C(0.0);
    }

    // A lag of 0 says nothing, because every signal matches itself there.
    if((low_lag == 0u) || (low_lag > high_lag) || (high_lag >= size))
    {
        return 0u;
    }

    if(!correlate_auto(data, size, output, high_lag, CORRELATE_COEFFICIENT))
    {
        return 0u;
    }

    uint32_t best = low_lag;
    real_t largest = output[low_lag];

    for(uint32_t lag = low_lag + 1u; lag <= high_lag; lag++)
    {
        if(output[lag] > largest)
        {
            largest = output[lag];
            best = lag;
        }
    }

    if(strength != NULL)
    {
        *strength = largest;
    }

    return best;
}

uint32_t correlate_transform_size(uint32_t size)
{
    if(size == 0u)
    {
        return 0u;
    }

    // Twice the size, so that the end of the signal cannot wrap round and
    // correlate with its own start. A transform works on a signal that repeats
    // for ever, and the zeros in between keep the two ends apart.
    uint32_t wanted = 2u * size;
    uint32_t chosen = 2u;

    while(chosen < wanted)
    {
        uint32_t next = chosen * 2u;
        if(next < chosen)
        {
            return 0u;      // The size cannot be held in the number.
        }
        chosen = next;
    }

    return fft_is_valid_size(chosen) ? chosen : 0u;
}

bool correlate_auto_by_transform(const real_t* data, uint32_t size,
                                 real_t* output, uint32_t max_lag,
                                 correlate_scaling_t scaling,
                                 fft_t* fft, cnum_t* work, real_t* window)
{
    ASSERT(data != NULL);
    ASSERT(output != NULL);
    ASSERT(fft != NULL);
    ASSERT(work != NULL);
    ASSERT(window != NULL);

    if((size == 0u) || (max_lag >= size) || !correlate_is_valid_scaling(scaling))
    {
        return false;
    }

    uint32_t transform = correlate_transform_size(size);
    if((transform == 0u) || (fft->size != transform))
    {
        return false;
    }

    // The transform gives the SUM at each lag and nothing else. A coefficient
    // needs the mean and the energy of the samples that overlap at each lag on
    // their own, and no single transform holds those. Thus this method serves
    // the three scalings that are sums, and a caller who wants a coefficient
    // uses correlate_auto, which works them out lag by lag.
    if(scaling == CORRELATE_COEFFICIENT)
    {
        return false;
    }

    // The signal, and zeros after it up to the size of the transform.
    for(uint32_t index = 0; index < transform; index++)
    {
        window[index] = (index < size) ? data[index] : REAL_C(0.0);
    }

    fft_forward_real(fft, window, work);

    // The power at each bin, which is the transform of the correlation.
    //
    // A correlation in time is a multiplication in frequency, with one of the
    // two turned round. Turning a signal round in time is the same as taking
    // the conjugate of its transform, thus a signal correlated with ITSELF
    // becomes its transform multiplied by its own conjugate, which is the
    // square of how large each bin is. The result holds no turn at all, which
    // is why the answer comes out real.
    for(uint32_t bin = 0; bin < transform; bin++)
    {
        work[bin] = cnum_make(cnum_magnitude_squared(work[bin]), REAL_C(0.0));
    }

    fft_inverse(fft, work);

    // No scaling has to be put back here. The forward transform of this module
    // does not divide and the inverse divides by the size, thus the pair
    // already gives the plain sum at each lag. The rule of Parseval says the
    // same thing: the sum at no lag equals the energy of the signal, which is
    // what the plain method gives there.
    for(uint32_t lag = 0; lag <= max_lag; lag++)
    {
        output[lag] = correlate_scale(cnum_real(work[lag]), size, lag,
                                      REAL_C(0.0), scaling);
    }

    return true;
}
