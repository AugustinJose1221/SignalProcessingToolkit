#ifndef TEST
#include <ffitt/transform/fft.h>
#include <ffitt/core/defs.h>
#else
#include "fft.h"
#include "defs.h"
#endif

#include <math.h>

#define FFT_PI      REAL_C(3.14159265358979323846)

static void fft_build_tables(fft_t* fft);
static uint32_t fft_reverse_bits(uint32_t value, uint32_t bits);
static uint32_t fft_count_bits(uint32_t size);
static void fft_transform(fft_t* fft, cnum_t* data);

bool fft_is_valid_size(uint32_t size)
{
    // A power of two holds one bit only. The value size-1 then holds every
    // lower bit, thus the two values share no bit.
    return (size >= 2) && ((size & (size - 1)) == 0);
}

fft_t fft_alloc(uint32_t size)
{
    ASSERT(fft_is_valid_size(size));

    fft_t fft;

    fft.size = size;
    fft.twiddle = (cnum_t*)malloc(sizeof(cnum_t)*FFT_TWIDDLE_COUNT(size));
    fft.reverse = (uint32_t*)malloc(sizeof(uint32_t)*FFT_REVERSE_COUNT(size));
    fft.dynamic_alloc = true;

    // The tables are written through both lists, thus they must not be built
    // with nothing to write to.
    if((fft.twiddle == NULL) || (fft.reverse == NULL))
    {
        fft_free(&fft);

        fft.size = 0;
        fft.dynamic_alloc = false;

        return fft;
    }

    fft_build_tables(&fft);

    return fft;
}

fft_t fft_static_alloc(uint32_t size, cnum_t* twiddle, uint32_t* reverse)
{
    ASSERT(fft_is_valid_size(size));
    ASSERT(twiddle != NULL);
    ASSERT(reverse != NULL);

    fft_t fft;

    fft.size = size;
    fft.twiddle = twiddle;
    fft.reverse = reverse;
    fft.dynamic_alloc = false;

    fft_build_tables(&fft);

    return fft;
}

void fft_forward(fft_t* fft, cnum_t* data)
{
    ASSERT(fft != NULL);
    ASSERT(data != NULL);

    fft_transform(fft, data);
}

void fft_inverse(fft_t* fft, cnum_t* data)
{
    ASSERT(fft != NULL);
    ASSERT(data != NULL);

    // The inverse transform is the forward transform of the conjugate, and
    // then the conjugate of that result, divided by the size. Thus the module
    // needs one set of turning factors only.
    for(uint32_t index = 0; index < fft->size; index++)
    {
        data[index] = cnum_conjugate(data[index]);
    }

    fft_transform(fft, data);

    real_t scale = REAL_C(1.0) / (real_t)fft->size;
    for(uint32_t index = 0; index < fft->size; index++)
    {
        data[index] = cnum_scale(cnum_conjugate(data[index]), scale);
    }
}

void fft_forward_real(fft_t* fft, const real_t* input, cnum_t* output)
{
    ASSERT(fft != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < fft->size; index++)
    {
        output[index] = cnum_make(input[index], REAL_C(0.0));
    }

    fft_transform(fft, output);
}

void fft_inverse_real(fft_t* fft, const cnum_t* input, real_t* output,
                      cnum_t* work)
{
    ASSERT(fft != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(work != NULL);

    uint32_t size = fft->size;
    uint32_t half = size / 2u;

    // Bin 0 and bin size/2 sit on top of their own mirror. A signal of real
    // values cannot give either of them an imaginary part, thus whatever is
    // there is dropped rather than carried into an answer that no real signal
    // could have.
    work[0] = cnum_make(cnum_real(input[0]), REAL_C(0.0));
    work[half] = cnum_make(cnum_real(input[half]), REAL_C(0.0));

    // Every other bin of the upper half is the mirror of one below it, turned
    // the other way.
    for(uint32_t index = 1; index < half; index++)
    {
        work[index] = input[index];
        work[size - index] = cnum_conjugate(input[index]);
    }

    fft_inverse(fft, work);

    // What comes back is real to the last digit the width can hold. The
    // imaginary part is rounding and it is dropped.
    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = cnum_real(work[index]);
    }
}

void fft_magnitude(const cnum_t* data, real_t* magnitude, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(magnitude != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        magnitude[index] = cnum_magnitude(data[index]);
    }
}

void fft_power(const cnum_t* data, real_t* power, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(power != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        power[index] = cnum_magnitude_squared(data[index]);
    }
}

real_t fft_bin_frequency(uint32_t index, uint32_t size, real_t sample_rate)
{
    ASSERT(size > 0);
    ASSERT(index < size);

    if(index <= (size/2))
    {
        return ((real_t)index * sample_rate) / (real_t)size;
    }

    // A bin above the middle mirrors a lower bin. Give the negative frequency
    // that it holds.
    return (((real_t)index - (real_t)size) * sample_rate) / (real_t)size;
}

void fft_free(fft_t* fft)
{
    ASSERT(fft != NULL);

    if(fft->dynamic_alloc)
    {
        free(fft->twiddle);
        free(fft->reverse);
        fft->twiddle = NULL;
        fft->reverse = NULL;
        fft->dynamic_alloc = false;
    }
}

// Give the number of bits that the size needs. A size of 8 needs 3 bits.
static uint32_t fft_count_bits(uint32_t size)
{
    uint32_t bits = 0;

    while((size >> bits) > 1)
    {
        bits++;
    }

    return bits;
}

// Give the value with the order of its bits turned around. The value 1 with 3
// bits gives 4, because 001 turned around is 100.
static uint32_t fft_reverse_bits(uint32_t value, uint32_t bits)
{
    uint32_t result = 0;

    for(uint32_t position = 0; position < bits; position++)
    {
        result <<= 1;
        result |= (value >> position) & 1u;
    }

    return result;
}

static void fft_build_tables(fft_t* fft)
{
    uint32_t bits = fft_count_bits(fft->size);

    for(uint32_t index = 0; index < fft->size; index++)
    {
        fft->reverse[index] = fft_reverse_bits(index, bits);
    }

    // The turning factor k is the point on the circle at the angle
    // -2*pi*k/size.
    for(uint32_t index = 0; index < FFT_TWIDDLE_COUNT(fft->size); index++)
    {
        real_t angle = (-REAL_C(2.0) * FFT_PI * (real_t)index) / (real_t)fft->size;
        fft->twiddle[index] = cnum_make(REAL_COS(angle), REAL_SIN(angle));
    }
}

// The transform of Cooley and Tukey, in place and without recursion.
//
// The method first puts the data into the order of the bit reversal. It then
// joins pairs of points into groups of 2, then groups of 4, and so on up to
// the full size. Each step needs one multiplication and one addition for each
// pair, which is the butterfly.
//
// WHY THE BUTTERFLY IS WRITTEN OUT ON THE PARTS AND NOT ON cnum_t.
//
// It was written on cnum_t, as cnum_multiply(lower, factor) and so on, which
// reads far better. Measured against the same transform written on the real
// and the imaginary parts, in microseconds for one transform of 8192 points
// with GCC 13.3 at -O2, both giving the SAME ANSWER TO EVERY BIT:
//
//   width           on cnum_t     on the parts
//   32 bit             220.9            103.5
//   64 bit            2078.3            143.0
//
// At 64 bits that is fourteen times. The cost was never the arithmetic; it was
// handing a cnum_t to a function and taking one back. At 32 bits the pair fits
// where the compiler wants it and the calls cost little. At 64 bits the pair
// is sixteen bytes, and what the optimiser then built was slower than the same
// code with the optimiser told to leave it alone: -O2 measured 2078 against
// -O1 at 326.
//
// The strided read of the turning factor was the first suspect and it was not
// the cause. Walking that table by a pointer that steps, instead of by a
// multiplied index, measured 2397 at 64 bits, which is worse than the form it
// was meant to mend.
//
// This is the one place in the library where a module reaches past cnum for
// the parts of a complex number. It buys a transform that is faster at BOTH
// widths, and every module that stands on the transform gets it.
static void fft_transform(fft_t* fft, cnum_t* data)
{
    uint32_t size = fft->size;

    for(uint32_t index = 0; index < size; index++)
    {
        uint32_t target = fft->reverse[index];
        if(target > index)
        {
            cnum_t value = data[index];
            data[index] = data[target];
            data[target] = value;
        }
    }

    for(uint32_t group = 2; group <= size; group *= 2)
    {
        uint32_t half = group / 2;
        uint32_t step = size / group;

        for(uint32_t start = 0; start < size; start += group)
        {
            for(uint32_t position = 0; position < half; position++)
            {
                uint32_t here = start + position;
                uint32_t there = here + half;

                real_t factor_re = fft->twiddle[position * step].re;
                real_t factor_im = fft->twiddle[position * step].im;

                real_t lower_re = data[there].re;
                real_t lower_im = data[there].im;

                // The turned lower half: a multiplication of two complex
                // numbers, written on the parts.
                real_t turned_re = (lower_re * factor_re) - (lower_im * factor_im);
                real_t turned_im = (lower_re * factor_im) + (lower_im * factor_re);

                real_t upper_re = data[here].re;
                real_t upper_im = data[here].im;

                data[here].re = upper_re + turned_re;
                data[here].im = upper_im + turned_im;
                data[there].re = upper_re - turned_re;
                data[there].im = upper_im - turned_im;
            }
        }
    }
}
