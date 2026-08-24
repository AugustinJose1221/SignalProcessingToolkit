#ifndef TEST
#include <sptk/transform/convolve.h>
#include <sptk/core/defs.h>
#else
#include "convolve.h"
#include "defs.h"
#endif

#include <math.h>

bool convolve_is_valid_mode(convolve_mode_t mode)
{
    return (mode >= CONVOLVE_FULL) && (mode <= CONVOLVE_VALID);
}

uint32_t convolve_output_size(uint32_t signal_size, uint32_t shape_size,
                              convolve_mode_t mode)
{
    if((signal_size == 0u) || (shape_size == 0u)
       || !convolve_is_valid_mode(mode))
    {
        return 0u;
    }

    switch(mode)
    {
        case CONVOLVE_SAME:
            return signal_size;

        case CONVOLVE_VALID:
            // A shape longer than the signal never lies wholly inside it,
            // thus there is no place where nothing has to be assumed.
            if(shape_size > signal_size)
            {
                return 0u;
            }
            return (signal_size - shape_size) + 1u;

        case CONVOLVE_FULL:
        default:
            return (signal_size + shape_size) - 1u;
    }
}

// Where in the full answer the wanted part begins.
static uint32_t convolve_offset(uint32_t shape_size, convolve_mode_t mode)
{
    switch(mode)
    {
        case CONVOLVE_SAME:
            // The middle of the full answer. For an even shape there are two
            // middles; this takes the earlier one, which is what every other
            // library does and what a caller comparing answers expects.
            return (shape_size - 1u) / 2u;

        case CONVOLVE_VALID:
            // The first place where the shape lies wholly inside the signal.
            return shape_size - 1u;

        case CONVOLVE_FULL:
        default:
            return 0u;
    }
}

bool convolve_direct(const real_t* signal, uint32_t signal_size,
                     const real_t* shape, uint32_t shape_size,
                     real_t* output, convolve_mode_t mode)
{
    ASSERT(signal != NULL);
    ASSERT(shape != NULL);
    ASSERT(output != NULL);

    uint32_t count = convolve_output_size(signal_size, shape_size, mode);

    if(count == 0u)
    {
        return false;
    }

    uint32_t offset = convolve_offset(shape_size, mode);

    for(uint32_t index = 0; index < count; index++)
    {
        uint32_t place = index + offset;
        real_t total = REAL_C(0.0);

        // The shape is turned round before it is slid along. That one
        // difference is the whole of what parts a convolution from a
        // correlation, and for a shape that is not the same forwards and
        // backwards it changes the answer.
        for(uint32_t k = 0; k < shape_size; k++)
        {
            // Where the signal is read. Outside itself the signal is taken to
            // be nothing, which is what makes the ends of the full and the
            // same modes partly assumed rather than measured.
            if((place >= k) && ((place - k) < signal_size))
            {
                total += signal[place - k] * shape[k];
            }
        }

        output[index] = total;
    }

    return true;
}

uint32_t convolve_transform_size(uint32_t signal_size, uint32_t shape_size)
{
    if((signal_size == 0u) || (shape_size == 0u))
    {
        return 0u;
    }

    // At least as long as the whole answer. A transform works on a signal that
    // repeats for ever, thus anything hanging past the end wraps round and
    // adds itself to the start.
    uint32_t wanted = (signal_size + shape_size) - 1u;
    uint32_t chosen = 2u;

    while(chosen < wanted)
    {
        uint32_t next = chosen * 2u;
        if(next < chosen)
        {
            return 0u;
        }
        chosen = next;
    }

    return fft_is_valid_size(chosen) ? chosen : 0u;
}

bool convolve_by_transform(const real_t* signal, uint32_t signal_size,
                           const real_t* shape, uint32_t shape_size,
                           real_t* output, convolve_mode_t mode,
                           fft_t* fft, cnum_t* first, cnum_t* second,
                           real_t* work)
{
    ASSERT(signal != NULL);
    ASSERT(shape != NULL);
    ASSERT(output != NULL);
    ASSERT(fft != NULL);
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(work != NULL);

    uint32_t count = convolve_output_size(signal_size, shape_size, mode);

    if(count == 0u)
    {
        return false;
    }

    uint32_t transform = convolve_transform_size(signal_size, shape_size);

    if((transform == 0u) || (fft->size != transform))
    {
        return false;
    }

    // Both signals, with zeros after them up to the size of the transform.
    for(uint32_t index = 0; index < transform; index++)
    {
        work[index] = (index < signal_size) ? signal[index] : REAL_C(0.0);
    }
    fft_forward_real(fft, work, first);

    for(uint32_t index = 0; index < transform; index++)
    {
        work[index] = (index < shape_size) ? shape[index] : REAL_C(0.0);
    }
    fft_forward_real(fft, work, second);

    // A convolution in time is a multiplication in frequency, bin by bin. No
    // conjugate is taken here, and that is exactly what parts this from a
    // correlation: taking one would turn the shape round in time.
    for(uint32_t bin = 0; bin < transform; bin++)
    {
        first[bin] = cnum_multiply(first[bin], second[bin]);
    }

    fft_inverse(fft, first);

    uint32_t offset = convolve_offset(shape_size, mode);

    for(uint32_t index = 0; index < count; index++)
    {
        output[index] = cnum_real(first[index + offset]);
    }

    return true;
}
