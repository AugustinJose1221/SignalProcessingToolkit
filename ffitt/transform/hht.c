#ifndef TEST
#include <ffitt/transform/hht.h>
#include <ffitt/transform/hilbert.h>
#include <ffitt/core/defs.h>
#else
#include "hht.h"
#include "hilbert.h"
#include "defs.h"
#endif

void hht_transform_imf(fft_t* fft, imf_t* imf, cnum_t* work,
                       real_t* amplitude, real_t* frequency, real_t sample_rate)
{
    ASSERT(fft != NULL);
    ASSERT(imf != NULL);
    ASSERT(work != NULL);
    ASSERT(amplitude != NULL);
    ASSERT(frequency != NULL);
    ASSERT(imf->size == fft->size);

    hilbert_analytic_signal(fft, imf->y, work);
    hilbert_amplitude(work, amplitude, imf->size);
    hilbert_frequency(work, frequency, imf->size, sample_rate);
}

void hht_transform(fft_t* fft, imf_t* imf, uint32_t count, cnum_t* work,
                   real_t* amplitude, real_t* frequency, real_t sample_rate)
{
    ASSERT(fft != NULL);
    ASSERT(imf != NULL);
    ASSERT(count > 0);

    uint32_t size = fft->size;

    for(uint32_t index = 0; index < count; index++)
    {
        hht_transform_imf(fft, &imf[index], work,
                          &amplitude[index * size],
                          &frequency[index * (size - 1)],
                          sample_rate);
    }
}

real_t hht_mean_frequency(const real_t* amplitude, const real_t* frequency,
                         uint32_t size)
{
    ASSERT(amplitude != NULL);
    ASSERT(frequency != NULL);
    ASSERT(size > 1);

    real_t total_weight = REAL_C(0.0);
    real_t total = REAL_C(0.0);

    // The frequency list holds one value less than the amplitude list.
    for(uint32_t index = 0; index < (size - 1); index++)
    {
        real_t weight = amplitude[index] * amplitude[index];
        total += weight * frequency[index];
        total_weight += weight;
    }

    if(total_weight == REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    return total / total_weight;
}
