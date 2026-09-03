// This file is left out of the build when FFITT_NO_TRANSFORM is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_TRANSFORM

#ifndef TEST
#include <ffitt/transform/dct.h>
#include <ffitt/core/defs.h>
#else
#include "dct.h"
#include "defs.h"
#endif

#include <math.h>

#define DCT_PI      REAL_C(3.14159265358979323846)

bool dct_is_valid_size(uint32_t size)
{
    return (size >= 1u) && (size <= DCT_LARGEST_SIZE);
}

bool dct_forward(const real_t* input, real_t* output, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(input != output);

    if(!dct_is_valid_size(size))
    {
        return false;
    }

    // THE SAMPLES ARE READ HALF A STEP IN FROM WHERE THEY WOULD OTHERWISE
    // STAND, which is the half added below. That half is what makes the block
    // meet its own mirror smoothly rather than with a step, and the smoothness
    // is the whole reason to reach for this rather than for fft.
    for(uint32_t which = 0; which < size; which++)
    {
        real_t total = REAL_C(0.0);
        real_t turn = (DCT_PI * (real_t)which) / (real_t)size;

        for(uint32_t index = 0; index < size; index++)
        {
            total += input[index]
                     * REAL_COS(turn * ((real_t)index + REAL_C(0.5)));
        }

        // Scaled so that taking the transform and undoing it gives back what
        // went in, and so that the two directions are mirrors of each other.
        real_t weight = (which == 0u)
                        ? REAL_SQRT(REAL_C(1.0) / (real_t)size)
                        : REAL_SQRT(REAL_C(2.0) / (real_t)size);

        output[which] = weight * total;
    }

    return true;
}

bool dct_inverse(const real_t* input, real_t* output, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(input != output);

    if(!dct_is_valid_size(size))
    {
        return false;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        real_t total = REAL_C(0.0);

        for(uint32_t which = 0; which < size; which++)
        {
            real_t weight = (which == 0u)
                            ? REAL_SQRT(REAL_C(1.0) / (real_t)size)
                            : REAL_SQRT(REAL_C(2.0) / (real_t)size);

            real_t turn = (DCT_PI * (real_t)which) / (real_t)size;

            total += weight * input[which]
                     * REAL_COS(turn * ((real_t)index + REAL_C(0.5)));
        }

        output[index] = total;
    }

    return true;
}

uint32_t dct_count_for_share(const real_t* cosines, uint32_t size,
                             real_t share)
{
    ASSERT(cosines != NULL);

    if(!dct_is_valid_size(size) || (share <= REAL_C(0.0))
       || (share > REAL_C(1.0)))
    {
        return 0u;
    }

    real_t whole = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        whole += cosines[index] * cosines[index];
    }

    if(whole <= REAL_SMALLEST)
    {
        return 0u;
    }

    real_t wanted = share * whole;
    real_t gathered = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        gathered += cosines[index] * cosines[index];

        if(gathered >= wanted)
        {
            return index + 1u;
        }
    }

    return size;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int dct_is_not_in_this_build_t;

#endif//FFITT_NO_TRANSFORM
