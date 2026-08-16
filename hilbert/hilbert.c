#ifndef TEST
#include <hilbert/hilbert.h>
#include <common/defs.h>
#else
#include "hilbert.h"
#include "defs.h"
#endif

#include <math.h>

#define HILBERT_PI      3.14159265358979323846f

void hilbert_analytic_signal(fft_t* fft, const float* signal, cnum_t* analytic)
{
    ASSERT(fft != NULL);
    ASSERT(signal != NULL);
    ASSERT(analytic != NULL);

    uint32_t size = fft->size;
    uint32_t half = size / 2;

    fft_forward_real(fft, signal, analytic);

    // Keep the bin of the frequency zero and the bin of the middle as they
    // are. Double every bin below the middle, and set every bin above the
    // middle to zero. The inverse transform of that spectrum gives a signal
    // whose imaginary part is the Hilbert transform of the input.
    for(uint32_t index = 1; index < half; index++)
    {
        analytic[index] = cnum_scale(analytic[index], 2.0f);
    }

    for(uint32_t index = half + 1; index < size; index++)
    {
        analytic[index] = cnum_zero();
    }

    fft_inverse(fft, analytic);
}

void hilbert_amplitude(const cnum_t* analytic, float* amplitude, uint32_t size)
{
    ASSERT(analytic != NULL);
    ASSERT(amplitude != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        amplitude[index] = cnum_magnitude(analytic[index]);
    }
}

void hilbert_phase(const cnum_t* analytic, float* phase, uint32_t size)
{
    ASSERT(analytic != NULL);
    ASSERT(phase != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        phase[index] = atan2f(analytic[index].im, analytic[index].re);
    }
}

void hilbert_frequency(const cnum_t* analytic, float* frequency, uint32_t size,
                       float sample_rate)
{
    ASSERT(analytic != NULL);
    ASSERT(frequency != NULL);
    ASSERT(size > 1);
    ASSERT(sample_rate > 0.0f);

    for(uint32_t index = 0; index < (size - 1); index++)
    {
        float first = atan2f(analytic[index].im, analytic[index].re);
        float second = atan2f(analytic[index+1].im, analytic[index+1].re);
        float change = second - first;

        // The phase jumps from pi to -pi. Bring the change back into the range
        // from -pi to pi, so that a jump does not look like a large change of
        // the frequency.
        while(change > HILBERT_PI)
        {
            change -= 2.0f * HILBERT_PI;
        }
        while(change < -HILBERT_PI)
        {
            change += 2.0f * HILBERT_PI;
        }

        frequency[index] = (change * sample_rate) / (2.0f * HILBERT_PI);
    }
}
