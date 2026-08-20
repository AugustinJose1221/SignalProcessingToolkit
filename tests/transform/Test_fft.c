#include "unity.h"
#include "real_assert.h"
#include "fft.h"
#include "cnum.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.001)
#define PI          REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

static void fill_sine(real_t* signal, uint32_t size, real_t cycles, real_t amplitude)
{
    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = amplitude * REAL_SIN((REAL_C(2.0)*PI*cycles*(real_t)index) / (real_t)size);
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
        data[index] = cnum_make(REAL_C(2.0), REAL_C(0.0));
    }

    fft_forward(&fft, data);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(32.0), data[0].re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), data[0].im);
    for(uint32_t index = 1; index < 16; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), cnum_magnitude(data[index]));
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
    real_t signal[64];
    cnum_t spectrum[64];
    real_t magnitude[64];

    fill_sine(signal, size, REAL_C(3.0), REAL_C(1.0));
    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, size);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(32.0), magnitude[3]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(32.0), magnitude[size-3]);

    for(uint32_t index = 0; index < size; index++)
    {
        if(index != 3 && index != (size-3))
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), magnitude[index]);
        }
    }

    fft_free(&fft);
}

void test_fft_finds_the_frequency_of_the_strongest_bin(void)
{
    const uint32_t size = 128;
    fft_t fft = fft_alloc(size);
    real_t signal[128];
    cnum_t spectrum[128];
    real_t magnitude[128];

    fill_sine(signal, size, REAL_C(10.0), REAL_C(1.0));
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
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(10.0),
                             fft_bin_frequency(strongest, size, REAL_C(128.0)));

    fft_free(&fft);
}

void test_fft_the_inverse_gives_the_first_signal_again(void)
{
    const uint32_t size = 64;
    fft_t fft = fft_alloc(size);
    real_t signal[64];
    cnum_t data[64];

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = REAL_SIN((real_t)index) + (REAL_C(0.5)*REAL_COS(REAL_C(3.0)*(real_t)index));
        data[index] = cnum_make(signal[index], REAL_C(0.0));
    }

    fft_forward(&fft, data);
    fft_inverse(&fft, data);

    for(uint32_t index = 0; index < size; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, signal[index], data[index].re);
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), data[index].im);
    }

    fft_free(&fft);
}

void test_fft_of_two_sines_holds_both_frequencies(void)
{
    const uint32_t size = 128;
    fft_t fft = fft_alloc(size);
    real_t signal[128];
    cnum_t spectrum[128];
    real_t magnitude[128];

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = REAL_SIN((REAL_C(2.0)*PI*REAL_C(5.0)*(real_t)index)/(real_t)size)
                        + (REAL_C(0.5)*REAL_SIN((REAL_C(2.0)*PI*REAL_C(20.0)*(real_t)index)/(real_t)size));
    }

    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, size);

    // The first sine has two times the amplitude of the second one, thus its
    // bin is two times as large.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(64.0), magnitude[5]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(32.0), magnitude[20]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(0.0), magnitude[12]);

    fft_free(&fft);
}

void test_fft_power_is_the_square_of_the_magnitude(void)
{
    cnum_t data[4];
    real_t magnitude[4];
    real_t power[4];

    data[0] = cnum_make(REAL_C(3.0), REAL_C(4.0));
    data[1] = cnum_make(REAL_C(0.0), REAL_C(0.0));
    data[2] = cnum_make(-REAL_C(1.0), REAL_C(0.0));
    data[3] = cnum_make(REAL_C(0.0), REAL_C(2.0));

    fft_magnitude(data, magnitude, 4);
    fft_power(data, power, 4);

    for(uint32_t index = 0; index < 4; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, magnitude[index]*magnitude[index],
                                 power[index]);
    }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), magnitude[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(25.0), power[0]);
}

void test_fft_bin_frequency(void)
{
    // A window of 8 points at 8 hertz holds one second.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), fft_bin_frequency(0, 8, REAL_C(8.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), fft_bin_frequency(1, 8, REAL_C(8.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), fft_bin_frequency(4, 8, REAL_C(8.0)));
    // The bin above the middle mirrors a lower bin.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(3.0), fft_bin_frequency(5, 8, REAL_C(8.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(1.0), fft_bin_frequency(7, 8, REAL_C(8.0)));
}

void test_fft_the_smallest_size_works(void)
{
    fft_t fft = fft_alloc(2);
    cnum_t data[2];

    data[0] = cnum_make(REAL_C(1.0), REAL_C(0.0));
    data[1] = cnum_make(REAL_C(3.0), REAL_C(0.0));

    fft_forward(&fft, data);

    // For two points the result is the sum and the difference.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), data[0].re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(2.0), data[1].re);

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
        real_t value = REAL_SIN(REAL_C(0.3)*(real_t)index) + (real_t)index;
        first[index] = cnum_make(value, REAL_C(0.0));
        second[index] = cnum_make(value, REAL_C(0.0));
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
