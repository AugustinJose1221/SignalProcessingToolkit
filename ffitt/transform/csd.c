// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/csd.h>
#include <ffitt/core/defs.h>
#else
#include "csd.h"
#include "defs.h"
#endif

#include <math.h>
#include <stdlib.h>

bool csd_is_valid_block(uint32_t block)
{
    return fft_is_valid_size(block);
}

csd_t csd_alloc(uint32_t block)
{
    csd_t csd;

    csd.block = 0u;
    csd.overlap = 0u;
    csd.kind = WINDOW_HANN;
    csd.parameter = REAL_C(0.0);
    csd.window = NULL;
    csd.windowed = NULL;
    csd.first = NULL;
    csd.second = NULL;
    csd.cross = NULL;
    csd.first_power = NULL;
    csd.second_power = NULL;
    csd.window_power = REAL_C(0.0);
    csd.designed = false;
    csd.dynamic_alloc = true;

    if(!csd_is_valid_block(block))
    {
        csd.fft = fft_alloc(2);
        fft_free(&csd.fft);
        return csd;
    }

    csd.fft = fft_alloc(block);
    csd.window = (real_t*)malloc(sizeof(real_t) * (size_t)block);
    csd.windowed = (real_t*)malloc(sizeof(real_t) * (size_t)block);
    csd.first = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)block);
    csd.second = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)block);

    uint32_t bins = CSD_BIN_COUNT(block);

    csd.cross = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)bins);
    csd.first_power = (real_t*)malloc(sizeof(real_t) * (size_t)bins);
    csd.second_power = (real_t*)malloc(sizeof(real_t) * (size_t)bins);

    if((csd.fft.size == 0u) || (csd.window == NULL) || (csd.windowed == NULL)
       || (csd.first == NULL) || (csd.second == NULL) || (csd.cross == NULL)
       || (csd.first_power == NULL) || (csd.second_power == NULL))
    {
        csd_free(&csd);
        return csd;
    }

    csd.block = block;

    return csd;
}

csd_t csd_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                       cnum_t* first, cnum_t* second, cnum_t* cross,
                       real_t* first_power, real_t* second_power, fft_t fft)
{
    ASSERT(window != NULL);
    ASSERT(windowed != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(cross != NULL);
    ASSERT(first_power != NULL);
    ASSERT(second_power != NULL);

    csd_t csd;

    csd.block = csd_is_valid_block(block) ? block : 0u;
    csd.overlap = 0u;
    csd.kind = WINDOW_HANN;
    csd.parameter = REAL_C(0.0);
    csd.window = window;
    csd.windowed = windowed;
    csd.first = first;
    csd.second = second;
    csd.cross = cross;
    csd.first_power = first_power;
    csd.second_power = second_power;
    csd.fft = fft;
    csd.window_power = REAL_C(0.0);
    csd.designed = false;
    csd.dynamic_alloc = false;

    return csd;
}

bool csd_design(csd_t* csd, uint32_t overlap, window_kind_t kind,
                real_t parameter)
{
    ASSERT(csd != NULL);

    if((csd->block == 0u) || (overlap >= csd->block)
       || !window_is_valid_kind(kind))
    {
        return false;
    }

    csd->overlap = overlap;
    csd->kind = kind;
    csd->parameter = parameter;

    window_build_with(csd->window, csd->block, kind, parameter);

    csd->window_power = REAL_C(0.0);

    for(uint32_t index = 0; index < csd->block; index++)
    {
        csd->window_power += csd->window[index] * csd->window[index];
    }

    csd->designed = true;

    return true;
}

uint32_t csd_block_count(const csd_t* csd, uint32_t size)
{
    ASSERT(csd != NULL);

    if(!csd->designed || (size < csd->block))
    {
        return 0u;
    }

    uint32_t step = csd->block - csd->overlap;

    return ((size - csd->block) / step) + 1u;
}

real_t csd_bin_frequency(const csd_t* csd, uint32_t bin, real_t sample_rate)
{
    ASSERT(csd != NULL);
    ASSERT(csd->block != 0u);

    return ((real_t)bin * sample_rate) / (real_t)csd->block;
}

// Walk both signals block by block and add up the three things every answer
// here is made of: what the two share, and what each holds on its own.
//
// The three are gathered in one pass because they must come from THE SAME
// blocks. Gathering them separately would let a block that is in one sum be
// missing from another, and the coherence would then read above 1, which is a
// thing that cannot be.
static bool csd_gather(csd_t* csd, const real_t* first, const real_t* second,
                       uint32_t size, uint32_t* blocks_out)
{
    if(!csd->designed)
    {
        return false;
    }

    uint32_t bins = CSD_BIN_COUNT(csd->block);
    uint32_t blocks = csd_block_count(csd, size);

    if(blocks < CSD_SMALLEST_BLOCK_COUNT)
    {
        return false;
    }

    uint32_t step = csd->block - csd->overlap;

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        csd->cross[bin] = cnum_zero();
        csd->first_power[bin] = REAL_C(0.0);
        csd->second_power[bin] = REAL_C(0.0);
    }

    for(uint32_t number = 0; number < blocks; number++)
    {
        window_apply(csd->window, &first[number * step], csd->windowed,
                     csd->block);
        fft_forward_real(&csd->fft, csd->windowed, csd->first);

        window_apply(csd->window, &second[number * step], csd->windowed,
                     csd->block);
        fft_forward_real(&csd->fft, csd->windowed, csd->second);

        for(uint32_t bin = 0; bin < bins; bin++)
        {
            // What the two share at this bin. The turn of the first is taken
            // the other way, thus the angle of the answer is how far the
            // second lags behind the first.
            csd->cross[bin] = cnum_add(csd->cross[bin],
                                       cnum_multiply(
                                           cnum_conjugate(csd->first[bin]),
                                           csd->second[bin]));

            csd->first_power[bin] +=
                cnum_magnitude_squared(csd->first[bin]);
            csd->second_power[bin] +=
                cnum_magnitude_squared(csd->second[bin]);
        }
    }

    *blocks_out = blocks;

    return true;
}

bool csd_estimate(csd_t* csd, const real_t* first, const real_t* second,
                  uint32_t size, real_t sample_rate, cnum_t* output)
{
    ASSERT(csd != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(output != NULL);

    if(sample_rate <= REAL_C(0.0))
    {
        return false;
    }

    uint32_t blocks = 0u;

    if(!csd_gather(csd, first, second, size, &blocks))
    {
        return false;
    }

    uint32_t bins = CSD_BIN_COUNT(csd->block);

    // The same three corrections the psd module makes, and for the same
    // reasons: the number of blocks, the window and the rate together, and the
    // mirrored half that holds the same power again.
    real_t divisor = (real_t)blocks * sample_rate * csd->window_power;

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        real_t scale = REAL_C(1.0) / divisor;

        if((bin != 0u) && (bin != (bins - 1u)))
        {
            scale *= REAL_C(2.0);
        }

        output[bin] = cnum_scale(csd->cross[bin], scale);
    }

    return true;
}

bool csd_coherence(csd_t* csd, const real_t* first, const real_t* second,
                   uint32_t size, real_t* output)
{
    ASSERT(csd != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(output != NULL);

    uint32_t blocks = 0u;

    if(!csd_gather(csd, first, second, size, &blocks))
    {
        return false;
    }

    uint32_t bins = CSD_BIN_COUNT(csd->block);

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        real_t below = csd->first_power[bin] * csd->second_power[bin];

        // A bin where one of the two signals holds nothing has nothing to
        // share, and no relation to report.
        if(below <= REAL_SMALLEST)
        {
            output[bin] = REAL_C(0.0);
        }
        else
        {
            // Every correction of the window and the rate stands both above
            // and below here, thus none of them is needed: the answer is a
            // ratio and they cancel.
            output[bin] = cnum_magnitude_squared(csd->cross[bin]) / below;

            // Rounding can lift the answer a hair above 1, which is a place it
            // cannot be.
            if(output[bin] > REAL_C(1.0))
            {
                output[bin] = REAL_C(1.0);
            }
        }
    }

    return true;
}

bool csd_transfer(csd_t* csd, const real_t* first, const real_t* second,
                  uint32_t size, cnum_t* output)
{
    ASSERT(csd != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(output != NULL);

    uint32_t blocks = 0u;

    if(!csd_gather(csd, first, second, size, &blocks))
    {
        return false;
    }

    uint32_t bins = CSD_BIN_COUNT(csd->block);

    for(uint32_t bin = 0; bin < bins; bin++)
    {
        // What the two share, divided by what the first holds. The window and
        // the rate cancel here as well.
        if(csd->first_power[bin] <= REAL_SMALLEST)
        {
            output[bin] = cnum_zero();
        }
        else
        {
            output[bin] = cnum_scale(csd->cross[bin],
                                     REAL_C(1.0) / csd->first_power[bin]);
        }
    }

    return true;
}

void csd_free(csd_t* csd)
{
    ASSERT(csd != NULL);

    fft_free(&csd->fft);

    if(csd->dynamic_alloc)
    {
        free(csd->window);
        free(csd->windowed);
        free(csd->first);
        free(csd->second);
        free(csd->cross);
        free(csd->first_power);
        free(csd->second_power);
    }

    csd->window = NULL;
    csd->windowed = NULL;
    csd->first = NULL;
    csd->second = NULL;
    csd->cross = NULL;
    csd->first_power = NULL;
    csd->second_power = NULL;
    csd->block = 0u;
    csd->designed = false;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int csd_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
