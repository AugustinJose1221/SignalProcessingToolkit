// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/hilbert.h>
#include <ffitt/core/defs.h>
#else
#include "hilbert.h"
#include "defs.h"
#endif

#include <math.h>

#define HILBERT_PI      REAL_C(3.14159265358979323846)

void hilbert_analytic_signal(fft_t* fft, const real_t* signal, cnum_t* analytic)
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
        analytic[index] = cnum_scale(analytic[index], REAL_C(2.0));
    }

    for(uint32_t index = half + 1; index < size; index++)
    {
        analytic[index] = cnum_zero();
    }

    fft_inverse(fft, analytic);
}

void hilbert_amplitude(const cnum_t* analytic, real_t* amplitude, uint32_t size)
{
    ASSERT(analytic != NULL);
    ASSERT(amplitude != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        amplitude[index] = cnum_magnitude(analytic[index]);
    }
}

void hilbert_phase(const cnum_t* analytic, real_t* phase, uint32_t size)
{
    ASSERT(analytic != NULL);
    ASSERT(phase != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        phase[index] = REAL_ATAN2(analytic[index].im, analytic[index].re);
    }
}

void hilbert_frequency(const cnum_t* analytic, real_t* frequency, uint32_t size,
                       real_t sample_rate)
{
    ASSERT(analytic != NULL);
    ASSERT(frequency != NULL);
    ASSERT(size > 1);
    ASSERT(sample_rate > REAL_C(0.0));

    for(uint32_t index = 0; index < (size - 1); index++)
    {
        real_t first = REAL_ATAN2(analytic[index].im, analytic[index].re);
        real_t second = REAL_ATAN2(analytic[index+1].im, analytic[index+1].re);
        real_t change = second - first;

        // The phase jumps from pi to -pi. Bring the change back into the range
        // from -pi to pi, so that a jump does not look like a large change of
        // the frequency.
        while(change > HILBERT_PI)
        {
            change -= REAL_C(2.0) * HILBERT_PI;
        }
        while(change < -HILBERT_PI)
        {
            change += REAL_C(2.0) * HILBERT_PI;
        }

        frequency[index] = (change * sample_rate) / (REAL_C(2.0) * HILBERT_PI);
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int hilbert_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
