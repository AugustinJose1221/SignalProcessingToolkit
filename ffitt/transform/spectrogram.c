// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/spectrogram.h>
#include <ffitt/core/defs.h>
#include <ffitt/transform/window.h>
#else
#include "spectrogram.h"
#include "defs.h"
#include "window.h"
#endif

#include <math.h>

bool spectrogram_is_valid_kind(spectrogram_kind_t kind)
{
    return (kind >= SPECTROGRAM_AMPLITUDE) && (kind <= SPECTROGRAM_DECIBEL);
}

uint32_t spectrogram_value_count(const stft_t* stft, uint32_t frame_count)
{
    ASSERT(stft != NULL);

    if(stft->block == 0u)
    {
        return 0u;
    }

    return frame_count * STFT_BIN_COUNT(stft->block);
}

// Give the power of one bin, corrected so that a wave of amplitude A reads
// A*A/2 whatever the block and the window.
//
// Three corrections, and leaving any one out gives an answer that looks
// reasonable and is wrong by a factor nobody notices:
//
//   THE WINDOW makes the signal smaller. For an amplitude the correction is
//   the sum of the window, because that is what a steady wave is multiplied
//   by.
//   THE BLOCK. A longer block gives a larger number for the same wave, and
//   the sum of the window carries that too.
//   THE MIRRORED HALF holds the same power again, at the negative
//   frequencies. Every bin but the first and the last stands for two, thus it
//   is doubled. The first and the last are their own mirror and are not.
static real_t spectrogram_amplitude_of(const cnum_t* frames, uint32_t index,
                                       uint32_t bin, uint32_t bins,
                                       real_t window_sum)
{
    real_t size = cnum_magnitude(frames[index]) / window_sum;

    if((bin != 0u) && (bin != (bins - 1u)))
    {
        size *= REAL_C(2.0);
    }

    return size;
}

bool spectrogram_build(const stft_t* stft, const cnum_t* frames,
                       uint32_t frame_count, spectrogram_kind_t kind,
                       real_t sample_rate, real_t* output, uint32_t room)
{
    ASSERT(stft != NULL);
    ASSERT(frames != NULL);
    ASSERT(output != NULL);

    if(!stft->designed || !spectrogram_is_valid_kind(kind)
       || (frame_count == 0u))
    {
        return false;
    }

    uint32_t bins = STFT_BIN_COUNT(stft->block);
    uint32_t count = frame_count * bins;

    if(room < count)
    {
        return false;
    }

    if((kind == SPECTROGRAM_DENSITY) && (sample_rate <= REAL_C(0.0)))
    {
        return false;
    }

    // The sum of the window, for an amplitude, and the sum of its squares, for
    // a density. They are different corrections because an amplitude follows
    // the window and a power follows its square.
    real_t window_sum = REAL_C(0.0);
    real_t window_power = REAL_C(0.0);

    for(uint32_t index = 0; index < stft->block; index++)
    {
        window_sum += stft->window[index];
        window_power += stft->window[index] * stft->window[index];
    }

    if(window_sum <= REAL_SMALLEST)
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        uint32_t bin = index % bins;
        real_t size = spectrogram_amplitude_of(frames, index, bin, bins,
                                               window_sum);

        // The mean power of a wave is half the square of its amplitude,
        // because it spends half its time below the middle.
        real_t power = (size * size) / REAL_C(2.0);

        if(kind == SPECTROGRAM_AMPLITUDE)
        {
            output[index] = size;
        }
        else if(kind == SPECTROGRAM_POWER)
        {
            output[index] = power;
        }
        else if(kind == SPECTROGRAM_DENSITY)
        {
            // A density follows the SQUARE of the window and not the window
            // itself, thus it is worked out from the bin afresh rather than
            // from the amplitude above.
            real_t density = cnum_magnitude_squared(frames[index])
                             / (sample_rate * window_power);

            if((bin != 0u) && (bin != (bins - 1u)))
            {
                density *= REAL_C(2.0);
            }

            output[index] = density;
        }
        else
        {
            // A bin that holds nothing has no logarithm, and a bin that holds
            // nothing is a thing that happens.
            output[index] = (power <= REAL_SMALLEST)
                            ? SPECTROGRAM_FLOOR_DECIBEL
                            : (REAL_C(10.0) * REAL_LOG10(power));

            if(output[index] < SPECTROGRAM_FLOOR_DECIBEL)
            {
                output[index] = SPECTROGRAM_FLOOR_DECIBEL;
            }
        }
    }

    return true;
}

real_t spectrogram_largest(const real_t* values, uint32_t count)
{
    ASSERT(values != NULL);

    if(count == 0u)
    {
        return REAL_C(0.0);
    }

    real_t largest = values[0];

    for(uint32_t index = 1; index < count; index++)
    {
        if(values[index] > largest)
        {
            largest = values[index];
        }
    }

    return largest;
}

bool spectrogram_against_the_largest(const real_t* values, uint32_t count,
                                     real_t* output)
{
    ASSERT(values != NULL);
    ASSERT(output != NULL);

    if(count == 0u)
    {
        return false;
    }

    real_t largest = spectrogram_largest(values, count);

    // In decibels a ratio is a subtraction, thus this is one pass and no
    // division. The floor is held, so that a bin that was already at the floor
    // does not fall through it.
    for(uint32_t index = 0; index < count; index++)
    {
        real_t value = values[index] - largest;

        output[index] = (value < SPECTROGRAM_FLOOR_DECIBEL)
                        ? SPECTROGRAM_FLOOR_DECIBEL : value;
    }

    return true;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int spectrogram_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
