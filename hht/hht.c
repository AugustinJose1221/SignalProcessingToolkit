#ifndef TEST
#include <hht/hht.h>
#include <hilbert/hilbert.h>
#include <common/defs.h>
#else
#include "hht.h"
#include "hilbert.h"
#include "defs.h"
#endif

void hht_transform_imf(fft_t* fft, imf_t* imf, cnum_t* work,
                       float* amplitude, float* frequency, float sample_rate)
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
                   float* amplitude, float* frequency, float sample_rate)
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

float hht_mean_frequency(const float* amplitude, const float* frequency,
                         uint32_t size)
{
    ASSERT(amplitude != NULL);
    ASSERT(frequency != NULL);
    ASSERT(size > 1);

    float total_weight = 0.0f;
    float total = 0.0f;

    // The frequency list holds one value less than the amplitude list.
    for(uint32_t index = 0; index < (size - 1); index++)
    {
        float weight = amplitude[index] * amplitude[index];
        total += weight * frequency[index];
        total_weight += weight;
    }

    if(total_weight == 0.0f)
    {
        return 0.0f;
    }

    return total / total_weight;
}
