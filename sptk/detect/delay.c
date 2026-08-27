#ifndef TEST
#include <sptk/detect/delay.h>
#include <sptk/transform/correlate.h>
#include <sptk/core/defs.h>
#else
#include "delay.h"
#include "correlate.h"
#include "defs.h"
#endif

#include <math.h>

bool delay_is_valid_way(delay_way_t way)
{
    return (way == DELAY_CORRELATE) || (way == DELAY_PHASE);
}

real_t delay_refine_peak(const real_t* values, uint32_t count, uint32_t peak)
{
    ASSERT(values != NULL);

    // Three points are needed and there are not three at either end.
    if((count < 3u) || (peak == 0u) || (peak >= (count - 1u)))
    {
        return REAL_C(0.0);
    }

    real_t before = values[peak - 1u];
    real_t here = values[peak];
    real_t after = values[peak + 1u];

    // How much the three points curve. A curve that does not bend downwards
    // has no top between the two neighbours, thus the middle point is not a
    // peak and there is nothing to refine.
    real_t bend = (REAL_C(2.0) * here) - before - after;

    if(bend <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    // The top of the curve through the three points. Written this way the
    // answer cannot leave the range from -0.5 to 0.5, because the middle point
    // is the largest of the three.
    return (REAL_C(0.5) * (after - before)) / bend;
}

bool delay_by_correlation(const real_t* first, const real_t* second,
                          uint32_t size, uint32_t largest_lag, real_t* work,
                          real_t* delay, real_t* strength)
{
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(work != NULL);
    ASSERT(delay != NULL);

    if((size == 0u) || (largest_lag == 0u) || (largest_lag >= size))
    {
        return false;
    }

    uint32_t count = DELAY_WORK_COUNT(largest_lag);

    // correlate_cross gives the lags from nothing upwards only, and a delay can
    // fall either way, thus it is called twice. The second call swaps the two
    // readings, which is what turns a lag one way into a lag the other.
    //
    // The two halves are written into one list with the lag of nothing in the
    // middle, so that a peak can be refined against both of its neighbours
    // however near the middle it stands.

    // The half where the second reading is later, which lands from the middle
    // of the list upwards, already in the order wanted.
    if(!correlate_cross(first, second, size, &work[largest_lag], largest_lag,
                        CORRELATE_COEFFICIENT))
    {
        return false;
    }

    // And the half where it is earlier. This one comes out counting upwards
    // from the middle as well, thus it lands in the front of the list back to
    // front and is turned round afterwards.
    if(!correlate_cross(second, first, size, work, largest_lag,
                        CORRELATE_COEFFICIENT))
    {
        return false;
    }

    for(uint32_t low = 0u; low < (largest_lag - low); low++)
    {
        uint32_t high = largest_lag - low;
        real_t held = work[low];

        work[low] = work[high];
        work[high] = held;
    }

    // The front of the list now runs from the largest lag the other way up to
    // one, and the middle holds the lag of nothing.
    uint32_t best = 0u;

    for(uint32_t index = 1u; index < count; index++)
    {
        if(work[index] > work[best])
        {
            best = index;
        }
    }

    real_t within = delay_refine_peak(work, count, best);

    *delay = ((real_t)best - (real_t)largest_lag) + within;

    if(strength != NULL)
    {
        *strength = work[best];
    }

    return true;
}

bool delay_by_phase(const real_t* first, const real_t* second, uint32_t size,
                    fft_t* fft, cnum_t* first_work, cnum_t* second_work,
                    real_t* delay)
{
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(fft != NULL);
    ASSERT(first_work != NULL);
    ASSERT(second_work != NULL);
    ASSERT(delay != NULL);

    uint32_t across = fft->size;

    if(!fft_is_valid_size(across) || (size < across))
    {
        return false;
    }

    fft_forward_real(fft, first, first_work);
    fft_forward_real(fft, second, second_work);

    uint32_t bins = FFT_REAL_BIN_COUNT(across);

    // THE PHASE IS NEVER UNWRAPPED, and not unwrapping it is the whole of why
    // this works on a real reading. The slope wanted is how far the phase of
    // the cross spectrum turns from one bin to the next. Reading each phase and
    // taking the differences would need every phase put back onto one line
    // first, and a single bin of noise puts the whole of the rest of that line
    // a turn out.
    //
    // Instead each step is formed as a number rather than an angle: the value
    // at one bin multiplied by the value at the bin below it turned round. Its
    // angle IS the step, and it can be nothing else, because a product of two
    // numbers has one angle. Adding those numbers up over the bins and taking
    // the angle of the sum at the end gives the average step weighted by how
    // loud each bin was, which is what should be weighted by.
    real_t total_re = REAL_C(0.0);
    real_t total_im = REAL_C(0.0);

    real_t last_re = REAL_C(0.0);
    real_t last_im = REAL_C(0.0);

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        // The cross spectrum at this bin: the first reading turned round
        // multiplied by the second. Its angle is how far the second stands
        // behind the first at this frequency.
        real_t re = (first_work[bin].re * second_work[bin].re)
                    + (first_work[bin].im * second_work[bin].im);
        real_t im = (first_work[bin].re * second_work[bin].im)
                    - (first_work[bin].im * second_work[bin].re);

        if(bin > 0u)
        {
            total_re += (re * last_re) + (im * last_im);
            total_im += (im * last_re) - (re * last_im);
        }

        last_re = re;
        last_im = im;
    }

    if((REAL_ABS(total_re) <= REAL_SMALLEST)
       && (REAL_ABS(total_im) <= REAL_SMALLEST))
    {
        // Nothing in either reading. There is no phase to have a slope.
        *delay = REAL_C(0.0);

        return true;
    }

    real_t step = REAL_ATAN2(total_im, total_re);

    // A delay of d samples turns the phase by a whole turn of d over the
    // transform between one bin and the next, and the sign is the other way
    // because a later reading LAGS.
    *delay = -(step * (real_t)across) / (REAL_C(2.0) * REAL_PI);

    return true;
}
