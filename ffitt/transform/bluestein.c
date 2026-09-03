// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/bluestein.h>
#include <ffitt/core/defs.h>
#else
#include "bluestein.h"
#include "defs.h"
#endif

#include <math.h>
#include <stdlib.h>

bool bluestein_is_valid_size(uint32_t size)
{
    return (size >= 2u) && (size <= BLUESTEIN_LARGEST_SIZE);
}

uint32_t bluestein_transform_size(uint32_t size)
{
    if(!bluestein_is_valid_size(size))
    {
        return 0u;
    }

    // The convolution runs to twice the size less one. A transform works on a
    // signal that repeats for ever, thus anything hanging past the end wraps
    // round and adds itself to the start, and the room must be taken before
    // that can happen.
    uint32_t wanted = (2u * size) - 1u;
    uint32_t chosen = 2u;

    while(chosen < wanted)
    {
        chosen *= 2u;
    }

    return chosen;
}

// Give the turning factor for one index, folded into a single turn first.
//
// THIS FOLD IS THE WHOLE METHOD. The angle wanted is pi times the square of
// the index, divided by the size. The square of the index reaches a million
// for a size of a thousand, and a float given an angle of a million has spent
// every digit it owns on how many turns and has none left for where in the
// turn it lands.
//
// The square is therefore taken back into the first two turns before any angle
// is formed. Two turns and not one, because the angle is pi times the fraction
// and not two pi. The count is widened to 64 bits for the multiplication, since
// the square of a size of a million does not fit in 32.
static cnum_t bluestein_factor(uint32_t index, uint32_t size, bool forward)
{
    uint64_t square = (uint64_t)index * (uint64_t)index;
    uint64_t folded = square % ((uint64_t)2u * (uint64_t)size);

    real_t angle = (REAL_PI * (real_t)folded) / (real_t)size;

    if(forward)
    {
        angle = -angle;
    }

    return cnum_make(REAL_COS(angle), REAL_SIN(angle));
}

// Build the tables that depend on the size only.
static void bluestein_build(bluestein_t* bluestein)
{
    uint32_t size = bluestein->size;
    uint32_t larger = bluestein->fft.size;

    for(uint32_t index = 0; index < size; index++)
    {
        bluestein->chirp[index] = bluestein_factor(index, size, true);
    }

    // The shape to convolve with. It runs both ways from the middle, thus the
    // part below zero is laid at the top end where the wrap round of the
    // transform puts it.
    for(uint32_t index = 0; index < larger; index++)
    {
        bluestein->kernel[index] = cnum_zero();
    }

    for(uint32_t index = 0; index < size; index++)
    {
        cnum_t factor = bluestein_factor(index, size, false);

        bluestein->kernel[index] = factor;

        if(index > 0u)
        {
            bluestein->kernel[larger - index] = factor;
        }
    }

    // The shape is the same for every transform, thus it is transformed once
    // here and not again.
    fft_forward(&bluestein->fft, bluestein->kernel);
}

bluestein_t bluestein_alloc(uint32_t size)
{
    bluestein_t bluestein;

    bluestein.size = 0u;
    bluestein.chirp = NULL;
    bluestein.kernel = NULL;
    bluestein.first = NULL;
    bluestein.second = NULL;
    bluestein.dynamic_alloc = true;

    uint32_t larger = bluestein_transform_size(size);

    if(larger == 0u)
    {
        bluestein.fft = fft_alloc(2);
        fft_free(&bluestein.fft);
        return bluestein;
    }

    bluestein.fft = fft_alloc(larger);
    bluestein.chirp = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)size);
    bluestein.kernel = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)larger);
    bluestein.first = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)larger);
    bluestein.second = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)larger);

    if((bluestein.fft.size == 0u) || (bluestein.chirp == NULL)
       || (bluestein.kernel == NULL) || (bluestein.first == NULL)
       || (bluestein.second == NULL))
    {
        bluestein_free(&bluestein);
        return bluestein;
    }

    bluestein.size = size;
    bluestein_build(&bluestein);

    return bluestein;
}

bluestein_t bluestein_static_alloc(uint32_t size, cnum_t* twiddle,
                                   uint32_t* reverse, cnum_t* chirp,
                                   cnum_t* kernel, cnum_t* first,
                                   cnum_t* second)
{
    ASSERT(twiddle != NULL);
    ASSERT(reverse != NULL);
    ASSERT(chirp != NULL);
    ASSERT(kernel != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);

    bluestein_t bluestein;

    uint32_t larger = bluestein_transform_size(size);

    bluestein.size = (larger == 0u) ? 0u : size;
    bluestein.fft = fft_static_alloc((larger == 0u) ? 2u : larger, twiddle,
                                     reverse);
    bluestein.chirp = chirp;
    bluestein.kernel = kernel;
    bluestein.first = first;
    bluestein.second = second;
    bluestein.dynamic_alloc = false;

    if(bluestein.size != 0u)
    {
        bluestein_build(&bluestein);
    }

    return bluestein;
}

// The forward transform, and the inverse written through it.
static void bluestein_run(bluestein_t* bluestein, cnum_t* data)
{
    uint32_t size = bluestein->size;
    uint32_t larger = bluestein->fft.size;

    // Turn each point by its own factor, and fill the rest with nothing.
    for(uint32_t index = 0; index < size; index++)
    {
        bluestein->first[index] = cnum_multiply(data[index],
                                                bluestein->chirp[index]);
    }

    for(uint32_t index = size; index < larger; index++)
    {
        bluestein->first[index] = cnum_zero();
    }

    // The convolution, done the way a convolution is always done: multiply the
    // two transforms and bring the answer back.
    fft_forward(&bluestein->fft, bluestein->first);

    for(uint32_t index = 0; index < larger; index++)
    {
        bluestein->second[index] = cnum_multiply(bluestein->first[index],
                                                 bluestein->kernel[index]);
    }

    fft_inverse(&bluestein->fft, bluestein->second);

    // Turn each answer back by its own factor. Only the first points of the
    // convolution are wanted; the rest is the tail that the method does not
    // ask for.
    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = cnum_multiply(bluestein->second[index],
                                    bluestein->chirp[index]);
    }
}

void bluestein_forward(bluestein_t* bluestein, cnum_t* data)
{
    ASSERT(bluestein != NULL);
    ASSERT(data != NULL);
    ASSERT(bluestein->size != 0u);

    bluestein_run(bluestein, data);
}

void bluestein_inverse(bluestein_t* bluestein, cnum_t* data)
{
    ASSERT(bluestein != NULL);
    ASSERT(data != NULL);
    ASSERT(bluestein->size != 0u);

    uint32_t size = bluestein->size;

    // The inverse transform is the forward transform of the conjugate, and
    // then the conjugate of that, divided by the size. Thus one set of tables
    // serves both ways.
    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = cnum_conjugate(data[index]);
    }

    bluestein_run(bluestein, data);

    real_t scale = REAL_C(1.0) / (real_t)size;

    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = cnum_scale(cnum_conjugate(data[index]), scale);
    }
}

real_t bluestein_bin_frequency(uint32_t index, uint32_t size,
                               real_t sample_rate)
{
    ASSERT(size != 0u);

    // A bin above half the size holds a frequency above half the sample rate.
    // Such a bin mirrors a lower one, thus the frequency of the mirror is
    // given, which is negative.
    real_t place = (index > (size / 2u)) ? ((real_t)index - (real_t)size)
                                         : (real_t)index;

    return (place * sample_rate) / (real_t)size;
}

void bluestein_free(bluestein_t* bluestein)
{
    ASSERT(bluestein != NULL);

    fft_free(&bluestein->fft);

    if(bluestein->dynamic_alloc)
    {
        free(bluestein->chirp);
        free(bluestein->kernel);
        free(bluestein->first);
        free(bluestein->second);
    }

    bluestein->chirp = NULL;
    bluestein->kernel = NULL;
    bluestein->first = NULL;
    bluestein->second = NULL;
    bluestein->size = 0u;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int bluestein_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
