// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/cepstrum.h>
#include <ffitt/core/defs.h>
#else
#include "cepstrum.h"
#include "defs.h"
#endif

#include <stdlib.h>
#include <math.h>

bool cepstrum_is_valid_size(uint32_t size)
{
    return fft_is_valid_size(size);
}

// Lay the window down once, when the cepstrum is built.
//
// A HANN WINDOW AND NOT A CHOICE OF WINDOW. What is wanted here is a spectrum
// whose noise does not leak from one bin into every other, and the plain window
// of the transform leaks worst of all. A caller choosing a window would be
// choosing between answers that are right and answers that are not, which is not
// a choice worth offering.
static void cepstrum_lay_window(real_t* window, uint32_t size)
{
    for(uint32_t index = 0; index < size; index++)
    {
        window[index] = window_value(index, size, WINDOW_HANN, REAL_C(0.0));
    }
}

cepstrum_t cepstrum_alloc(uint32_t size)
{
    cepstrum_t cepstrum;

    cepstrum.size = size;
    cepstrum.work = NULL;
    cepstrum.window = NULL;
    cepstrum.windowed = NULL;
    cepstrum.dynamic_alloc = true;
    cepstrum.fft.size = 0;
    cepstrum.fft.twiddle = NULL;
    cepstrum.fft.reverse = NULL;
    cepstrum.fft.dynamic_alloc = false;

    // THE SIZE IS EXAMINED BEFORE THE TRANSFORM IS ASKED FOR, AND NOT AFTER.
    //
    // The header promises that a size this module cannot take gives a handle
    // that cannot be used. The transform promises nothing of the kind: it
    // asserts, because a size that is no power of two is a caller bug to it.
    // This asked for the transform first and examined the size afterwards,
    // thus the graceful answer this module promises could never be reached -
    // the assertion inside the transform stopped the program on the way. It
    // never fired only because the tests were built with assertions off.
    if(!cepstrum_is_valid_size(size))
    {
        return cepstrum;
    }

    cepstrum.fft = fft_alloc(size);

    cepstrum.work = (cnum_t*)calloc(size, sizeof(cnum_t));
    cepstrum.window = (real_t*)calloc(size, sizeof(real_t));
    cepstrum.windowed = (real_t*)calloc(size, sizeof(real_t));

    if((cepstrum.work == NULL) || (cepstrum.window == NULL)
       || (cepstrum.windowed == NULL))
    {
        cepstrum_free(&cepstrum);

        return cepstrum;
    }

    cepstrum_lay_window(cepstrum.window, size);

    return cepstrum;
}

cepstrum_t cepstrum_static_alloc(uint32_t size, cnum_t* work, real_t* window,
                                 real_t* windowed, fft_t fft)
{
    ASSERT(work != NULL);
    ASSERT(window != NULL);
    ASSERT(windowed != NULL);

    cepstrum_t cepstrum;

    cepstrum.size = size;
    cepstrum.work = work;
    cepstrum.window = window;
    cepstrum.windowed = windowed;
    cepstrum.fft = fft;
    cepstrum.dynamic_alloc = false;

    if(cepstrum_is_valid_size(size))
    {
        cepstrum_lay_window(window, size);
    }

    return cepstrum;
}

bool cepstrum_real(cepstrum_t* cepstrum, const real_t* input, real_t* output)
{
    ASSERT(cepstrum != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(!cepstrum_is_valid_size(cepstrum->size) || (cepstrum->work == NULL)
       || (cepstrum->window == NULL) || (cepstrum->windowed == NULL))
    {
        return false;
    }

    uint32_t size = cepstrum->size;

    // THE WINDOW FIRST, AND THE WHOLE METHOD RESTS ON IT. A note whose period
    // divides the block needs no window; the noise on that note does not divide
    // it, leaks across every bin, and its leakage has strong structure in the
    // logarithm. Measured without a window, a note with no fundamental under a
    // twentieth of noise came back at 255 where 64 was right.
    for(uint32_t index = 0; index < size; index++)
    {
        cepstrum->windowed[index] = input[index] * cepstrum->window[index];
    }

    fft_forward_real(&cepstrum->fft, cepstrum->windowed, cepstrum->work);

    // The loudest bin, which the floor is measured against.
    real_t loudest = REAL_C(0.0);

    for(uint32_t bin = 0; bin < size; bin++)
    {
        real_t loudness = cnum_magnitude(cepstrum->work[bin]);

        if(loudness > loudest)
        {
            loudest = loudness;
        }
    }

    real_t floor_of = loudest * CEPSTRUM_FLOOR;

    if(floor_of <= REAL_SMALLEST)
    {
        // Nothing in the block at all.
        floor_of = REAL_SMALLEST;
    }

    // THE LOGARITHM OF HOW LOUD EACH BIN IS, AND NOTHING OF ITS PHASE.
    //
    // Throwing the phase away is what makes this the REAL cepstrum, and it is
    // what a caller looking for a period wants: where the harmonics stand does
    // not depend on where in its turn each of them happens to be.
    for(uint32_t bin = 0; bin < size; bin++)
    {
        real_t loudness = cnum_magnitude(cepstrum->work[bin]);

        if(loudness < floor_of)
        {
            loudness = floor_of;
        }

        cepstrum->work[bin] = cnum_make(REAL_LOG(loudness), REAL_C(0.0));
    }

    // And the transform of that, which is where a row of evenly spaced peaks
    // becomes one peak. The answer is real because what went in is real and
    // even, thus only the real part is kept.
    fft_inverse(&cepstrum->fft, cepstrum->work);

    for(uint32_t place = 0; place < size; place++)
    {
        output[place] = cnum_real(cepstrum->work[place]);
    }

    return true;
}

uint32_t cepstrum_best_quefrency(const real_t* cepstrum, uint32_t size,
                                 uint32_t low, uint32_t high,
                                 real_t* strength)
{
    ASSERT(cepstrum != NULL);

    if(strength != NULL)
    {
        *strength = REAL_C(0.0);
    }

    // The answer is read from the first half only: the second half is the
    // mirror of the first and holds nothing new.
    if((low < 1u) || (high <= low) || (high >= (size / 2u)))
    {
        return 0u;
    }

    uint32_t best = low;

    for(uint32_t place = low + 1u; place <= high; place++)
    {
        if(cepstrum[place] > cepstrum[best])
        {
            best = place;
        }
    }

    if(strength != NULL)
    {
        // How far the peak stands above the ordinary run of the range, which is
        // what says whether it is a peak at all rather than merely the largest
        // of a flat set.
        real_t total = REAL_C(0.0);
        uint32_t counted = 0u;

        for(uint32_t place = low; place <= high; place++)
        {
            total += cepstrum[place];
            counted++;
        }

        *strength = cepstrum[best] - (total / (real_t)counted);
    }

    return best;
}

void cepstrum_free(cepstrum_t* cepstrum)
{
    ASSERT(cepstrum != NULL);

    if(!cepstrum->dynamic_alloc)
    {
        return;
    }

    fft_free(&cepstrum->fft);

    free(cepstrum->work);
    free(cepstrum->window);
    free(cepstrum->windowed);

    cepstrum->work = NULL;
    cepstrum->window = NULL;
    cepstrum->windowed = NULL;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int cepstrum_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
