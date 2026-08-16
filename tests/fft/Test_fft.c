#include "unity.h"
#include "fft.h"
#include "cnum.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.001f
#define PI          3.14159265358979323846f

void setUp(void)
{

}

void tearDown(void)
{

}

static void fill_sine(float* signal, uint32_t size, float cycles, float amplitude)
{
    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = amplitude * sinf((2.0f*PI*cycles*(float)index) / (float)size);
    }
}

void test_fft_is_valid_size(void)
{
    TEST_ASSERT_EQUAL(true, fft_is_valid_size(2));
    TEST_ASSERT_EQUAL(true, fft_is_valid_size(8));
    TEST_ASSERT_EQUAL(true, fft_is_valid_size(1024));
    TEST_ASSERT_EQUAL(false, fft_is_valid_size(0));
    TEST_ASSERT_EQUAL(false, fft_is_valid_size(1));
    TEST_ASSERT_EQUAL(false, fft_is_valid_size(3));
    TEST_ASSERT_EQUAL(false, fft_is_valid_size(100));
}

void test_fft_alloc(void)
{
    fft_t fft = fft_alloc(16);

    TEST_ASSERT_EQUAL(16, fft.size);
    TEST_ASSERT_EQUAL(true, fft.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(fft.twiddle);
    TEST_ASSERT_NOT_NULL(fft.reverse);

    fft_free(&fft);
}

void test_fft_static_alloc(void)
{
    cnum_t twiddle[FFT_TWIDDLE_COUNT(8)];
    uint32_t reverse[FFT_REVERSE_COUNT(8)];

    fft_t fft = fft_static_alloc(8, twiddle, reverse);

    TEST_ASSERT_EQUAL(8, fft.size);
    TEST_ASSERT_EQUAL(false, fft.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(twiddle, fft.twiddle);
    TEST_ASSERT_EQUAL_PTR(reverse, fft.reverse);

    fft_free(&fft);

    // The memory belongs to the caller, thus the pointers do not change.
    TEST_ASSERT_EQUAL_PTR(twiddle, fft.twiddle);
}

void test_fft_the_bit_reversal_table_of_size_eight(void)
{
    // With 3 bits: 0,1,2,3,4,5,6,7 becomes 0,4,2,6,1,5,3,7.
    fft_t fft = fft_alloc(8);
    uint32_t expected[8] = {0, 4, 2, 6, 1, 5, 3, 7};

    for(uint32_t index = 0; index < 8; index++)
    {
        TEST_ASSERT_EQUAL(expected[index], fft.reverse[index]);
    }

    fft_free(&fft);
}

void test_fft_of_a_constant_signal_holds_only_the_first_bin(void)
{
    // A signal that does not change holds one frequency only, which is zero.
    // The first bin then holds the sum of every sample.
    fft_t fft = fft_alloc(16);
    cnum_t data[16];

    for(uint32_t index = 0; index < 16; index++)
    {
        data[index] = cnum_make(2.0f, 0.0f);
    }

    fft_forward(&fft, data);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 32.0f, data[0].re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, data[0].im);
    for(uint32_t index = 1; index < 16; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, cnum_magnitude(data[index]));
    }

    fft_free(&fft);
}

void test_fft_of_a_single_sine_holds_two_bins(void)
{
    // A sine of 3 cycles over the window gives a peak at the bin 3 and its
    // mirror at the bin size-3. Each peak holds half of the amplitude, times
    // the size, thus 32*1/2 = 16 for an amplitude of 1.
    const uint32_t size = 64;
    fft_t fft = fft_alloc(size);
    float signal[64];
    cnum_t spectrum[64];
    float magnitude[64];

    fill_sine(signal, size, 3.0f, 1.0f);
    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, size);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, magnitude[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, magnitude[size-3]);

    for(uint32_t index = 0; index < size; index++)
    {
        if(index != 3 && index != (size-3))
        {
            TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, magnitude[index]);
        }
    }

    fft_free(&fft);
}

void test_fft_finds_the_frequency_of_the_strongest_bin(void)
{
    const uint32_t size = 128;
    fft_t fft = fft_alloc(size);
    float signal[128];
    cnum_t spectrum[128];
    float magnitude[128];

    fill_sine(signal, size, 10.0f, 1.0f);
    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, size);

    uint32_t strongest = 0;
    for(uint32_t index = 1; index <= (size/2); index++)
    {
        if(magnitude[index] > magnitude[strongest])
        {
            strongest = index;
        }
    }

    TEST_ASSERT_EQUAL(10, strongest);
    // With a sample rate of 128 hertz the window holds one second, thus the
    // bin 10 is 10 hertz.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 10.0f,
                             fft_bin_frequency(strongest, size, 128.0f));

    fft_free(&fft);
}

void test_fft_the_inverse_gives_the_first_signal_again(void)
{
    const uint32_t size = 64;
    fft_t fft = fft_alloc(size);
    float signal[64];
    cnum_t data[64];

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = sinf((float)index) + (0.5f*cosf(3.0f*(float)index));
        data[index] = cnum_make(signal[index], 0.0f);
    }

    fft_forward(&fft, data);
    fft_inverse(&fft, data);

    for(uint32_t index = 0; index < size; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, signal[index], data[index].re);
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, data[index].im);
    }

    fft_free(&fft);
}

void test_fft_of_two_sines_holds_both_frequencies(void)
{
    const uint32_t size = 128;
    fft_t fft = fft_alloc(size);
    float signal[128];
    cnum_t spectrum[128];
    float magnitude[128];

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = sinf((2.0f*PI*5.0f*(float)index)/(float)size)
                        + (0.5f*sinf((2.0f*PI*20.0f*(float)index)/(float)size));
    }

    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, size);

    // The first sine has two times the amplitude of the second one, thus its
    // bin is two times as large.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 64.0f, magnitude[5]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 32.0f, magnitude[20]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, magnitude[12]);

    fft_free(&fft);
}

void test_fft_power_is_the_square_of_the_magnitude(void)
{
    cnum_t data[4];
    float magnitude[4];
    float power[4];

    data[0] = cnum_make(3.0f, 4.0f);
    data[1] = cnum_make(0.0f, 0.0f);
    data[2] = cnum_make(-1.0f, 0.0f);
    data[3] = cnum_make(0.0f, 2.0f);

    fft_magnitude(data, magnitude, 4);
    fft_power(data, power, 4);

    for(uint32_t index = 0; index < 4; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, magnitude[index]*magnitude[index],
                                 power[index]);
    }
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, magnitude[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 25.0f, power[0]);
}

void test_fft_bin_frequency(void)
{
    // A window of 8 points at 8 hertz holds one second.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, fft_bin_frequency(0, 8, 8.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, fft_bin_frequency(1, 8, 8.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, fft_bin_frequency(4, 8, 8.0f));
    // The bin above the middle mirrors a lower bin.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -3.0f, fft_bin_frequency(5, 8, 8.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, fft_bin_frequency(7, 8, 8.0f));
}

void test_fft_the_smallest_size_works(void)
{
    fft_t fft = fft_alloc(2);
    cnum_t data[2];

    data[0] = cnum_make(1.0f, 0.0f);
    data[1] = cnum_make(3.0f, 0.0f);

    fft_forward(&fft, data);

    // For two points the result is the sum and the difference.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, data[0].re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -2.0f, data[1].re);

    fft_free(&fft);
}

void test_fft_a_static_transform_gives_the_same_result_as_a_dynamic_one(void)
{
    const uint32_t size = 32;
    cnum_t twiddle[FFT_TWIDDLE_COUNT(32)];
    uint32_t reverse[FFT_REVERSE_COUNT(32)];

    fft_t dynamic_fft = fft_alloc(size);
    fft_t static_fft = fft_static_alloc(size, twiddle, reverse);

    cnum_t first[32];
    cnum_t second[32];

    for(uint32_t index = 0; index < size; index++)
    {
        float value = sinf(0.3f*(float)index) + (float)index;
        first[index] = cnum_make(value, 0.0f);
        second[index] = cnum_make(value, 0.0f);
    }

    fft_forward(&dynamic_fft, first);
    fft_forward(&static_fft, second);

    for(uint32_t index = 0; index < size; index++)
    {
        TEST_ASSERT_EQUAL(true, cnum_is_near(first[index], second[index], TOLERANCE));
    }

    fft_free(&dynamic_fft);
    fft_free(&static_fft);
}

void test_fft_free_releases_a_dynamic_transform(void)
{
    fft_t fft = fft_alloc(8);

    fft_free(&fft);

    TEST_ASSERT_NULL(fft.twiddle);
    TEST_ASSERT_EQUAL(false, fft.dynamic_alloc);

    // A second call must do nothing.
    fft_free(&fft);
    TEST_ASSERT_NULL(fft.twiddle);
}
