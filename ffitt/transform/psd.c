// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/psd.h>
#include <ffitt/core/defs.h>
#else
#include "psd.h"
#include "defs.h"
#endif

#include <math.h>

bool psd_is_valid_block(uint32_t block)
{
    // A block of two holds one bin above zero frequency, which is the least
    // that means anything.
    return (block >= 4u) && fft_is_valid_size(block);
}

psd_t psd_alloc(uint32_t block)
{
    ASSERT(psd_is_valid_block(block));

    psd_t psd;

    psd.block = block;
    psd.overlap = block / 2u;
    psd.kind = WINDOW_HANN;
    psd.parameter = REAL_C(0.0);
    psd.window = (real_t*)malloc(sizeof(real_t)*block);
    psd.windowed = (real_t*)malloc(sizeof(real_t)*block);
    psd.spectrum = (cnum_t*)malloc(sizeof(cnum_t)*block);
    psd.fft = fft_alloc(block);
    psd.window_power = REAL_C(0.0);
    psd.dynamic_alloc = true;

    // The design writes the window through the lists, thus it must not be
    // reached with nothing to write to.
    if((psd.window == NULL) || (psd.windowed == NULL) || (psd.spectrum == NULL)
       || (psd.fft.size == 0u))
    {
        psd_free(&psd);

        psd.window = NULL;
        psd.windowed = NULL;
        psd.spectrum = NULL;
        psd.block = 0;
        psd.dynamic_alloc = false;

        return psd;
    }

    psd_design(&psd, block / 2u, WINDOW_HANN, REAL_C(0.0));

    return psd;
}

psd_t psd_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                       cnum_t* spectrum, fft_t fft)
{
    ASSERT(psd_is_valid_block(block));
    ASSERT(window != NULL);
    ASSERT(windowed != NULL);
    ASSERT(spectrum != NULL);

    psd_t psd;

    psd.block = block;
    psd.overlap = block / 2u;
    psd.kind = WINDOW_HANN;
    psd.parameter = REAL_C(0.0);
    psd.window = window;
    psd.windowed = windowed;
    psd.spectrum = spectrum;
    psd.fft = fft;
    psd.window_power = REAL_C(0.0);
    psd.dynamic_alloc = false;

    psd_design(&psd, block / 2u, WINDOW_HANN, REAL_C(0.0));

    return psd;
}

bool psd_design(psd_t* psd, uint32_t overlap, window_kind_t kind,
                real_t parameter)
{
    ASSERT(psd != NULL);

    if((overlap >= psd->block) || !window_is_valid_kind(kind))
    {
        return false;
    }

    psd->overlap = overlap;
    psd->kind = kind;
    psd->parameter = parameter;

    window_build_with(psd->window, psd->block, kind, parameter);

    // The sum of the SQUARES of the window, because power follows the square.
    // Using the sum of the window itself is the usual mistake, and it gives an
    // answer that is wrong by a steady factor: for a Hann window it is out by
    // about a quarter.
    psd->window_power = REAL_C(0.0);
    for(uint32_t index = 0; index < psd->block; index++)
    {
        psd->window_power += psd->window[index] * psd->window[index];
    }

    return true;
}

uint32_t psd_bin_count(const psd_t* psd)
{
    ASSERT(psd != NULL);

    return (psd->block / 2u) + 1u;
}

uint32_t psd_block_count(const psd_t* psd, uint32_t size)
{
    ASSERT(psd != NULL);

    if(size < psd->block)
    {
        return 0u;
    }

    uint32_t step = psd->block - psd->overlap;

    return ((size - psd->block) / step) + 1u;
}

real_t psd_bin_frequency(const psd_t* psd, uint32_t bin, real_t sample_rate)
{
    ASSERT(psd != NULL);

    return fft_bin_frequency(bin, psd->block, sample_rate);
}

real_t psd_bin_width(const psd_t* psd, real_t sample_rate)
{
    ASSERT(psd != NULL);

    return sample_rate / (real_t)psd->block;
}

bool psd_estimate(psd_t* psd, const real_t* data, uint32_t size,
                  real_t sample_rate, real_t* output)
{
    ASSERT(psd != NULL);
    ASSERT(data != NULL);
    ASSERT(output != NULL);

    uint32_t blocks = psd_block_count(psd, size);

    if((blocks == 0u) || (psd->window_power <= REAL_C(0.0))
       || (sample_rate <= REAL_C(0.0)))
    {
        return false;
    }

    uint32_t bins = psd_bin_count(psd);
    uint32_t step = psd->block - psd->overlap;

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        output[bin] = REAL_C(0.0);
    }

    for(uint32_t number = 0; number < blocks; number++)
    {
        const real_t* start = &data[number * step];

        window_apply(psd->window, start, psd->windowed, psd->block);
        fft_forward_real(&psd->fft, psd->windowed, psd->spectrum);

        for(uint32_t bin = 0; bin < bins; bin++)
        {
            output[bin] += cnum_magnitude_squared(psd->spectrum[bin]);
        }
    }

    // Three corrections, and the header says why each one is needed.
    //
    //   the number of blocks    to take the mean of them
    //   the window and the rate to turn power for a bin into power for a hertz
    //   the other half          which holds the same power at the negative
    //                           frequencies
    real_t divisor = (real_t)blocks * sample_rate * psd->window_power;

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        output[bin] /= divisor;

        // The first bin stands at zero frequency and the last at half the
        // sample rate. Neither has a partner among the negative frequencies,
        // thus neither is doubled.
        if((bin > 0u) && ((bin + 1u) < bins))
        {
            output[bin] *= REAL_C(2.0);
        }
    }

    return true;
}

real_t psd_band_power(const psd_t* psd, const real_t* density,
                      real_t sample_rate, real_t low, real_t high)
{
    ASSERT(psd != NULL);
    ASSERT(density != NULL);

    if((high <= low) || (sample_rate <= REAL_C(0.0)))
    {
        return REAL_C(0.0);
    }

    real_t width = psd_bin_width(psd, sample_rate);
    uint32_t bins = psd_bin_count(psd);
    real_t total = REAL_C(0.0);

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        real_t frequency = psd_bin_frequency(psd, bin, sample_rate);

        if((frequency >= low) && (frequency <= high))
        {
            // A density is power for each hertz, thus each bin holds its
            // density times how many hertz the bin covers.
            total += density[bin] * width;
        }
    }

    return total;
}

void psd_free(psd_t* psd)
{
    ASSERT(psd != NULL);

    if(psd->dynamic_alloc)
    {
        free(psd->window);
        free(psd->windowed);
        free(psd->spectrum);
        fft_free(&psd->fft);
        psd->window = NULL;
        psd->windowed = NULL;
        psd->spectrum = NULL;
        psd->dynamic_alloc = false;
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int psd_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
