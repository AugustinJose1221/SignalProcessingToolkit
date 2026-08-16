#ifndef TEST
#include <sptk/transform/fft.h>
#include <sptk/core/defs.h>
#else
#include "fft.h"
#include "defs.h"
#endif

#include <math.h>

#define FFT_PI      3.14159265358979323846f

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

    float scale = 1.0f / (float)fft->size;
    for(uint32_t index = 0; index < fft->size; index++)
    {
        data[index] = cnum_scale(cnum_conjugate(data[index]), scale);
    }
}

void fft_forward_real(fft_t* fft, const float* input, cnum_t* output)
{
    ASSERT(fft != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < fft->size; index++)
    {
        output[index] = cnum_make(input[index], 0.0f);
    }

    fft_transform(fft, output);
}

void fft_magnitude(const cnum_t* data, float* magnitude, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(magnitude != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        magnitude[index] = cnum_magnitude(data[index]);
    }
}

void fft_power(const cnum_t* data, float* power, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(power != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        power[index] = cnum_magnitude_squared(data[index]);
    }
}

float fft_bin_frequency(uint32_t index, uint32_t size, float sample_rate)
{
    ASSERT(size > 0);
    ASSERT(index < size);

    if(index <= (size/2))
    {
        return ((float)index * sample_rate) / (float)size;
    }

    // A bin above the middle mirrors a lower bin. Give the negative frequency
    // that it holds.
    return (((float)index - (float)size) * sample_rate) / (float)size;
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
        float angle = (-2.0f * FFT_PI * (float)index) / (float)fft->size;
        fft->twiddle[index] = cnum_make(cosf(angle), sinf(angle));
    }
}

// The transform of Cooley and Tukey, in place and without recursion.
//
// The method first puts the data into the order of the bit reversal. It then
// joins pairs of points into groups of 2, then groups of 4, and so on up to
// the full size. Each step needs one multiplication and one addition for each
// pair, which is the butterfly.
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
                cnum_t factor = fft->twiddle[position * step];
                cnum_t upper = data[start + position];
                cnum_t lower = cnum_multiply(data[start + position + half], factor);

                data[start + position] = cnum_add(upper, lower);
                data[start + position + half] = cnum_subtract(upper, lower);
            }
        }
    }
}
