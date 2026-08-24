#include "unity.h"
#include "real_assert.h"
#include "bluestein.h"
#include "fft.h"
#include "cnum.h"
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_bluestein_is_valid_size(void)
{
    // Any size, which is the point of the module.
    TEST_ASSERT_EQUAL(true, bluestein_is_valid_size(60));
    TEST_ASSERT_EQUAL(true, bluestein_is_valid_size(100));
    TEST_ASSERT_EQUAL(true, bluestein_is_valid_size(2));
    TEST_ASSERT_EQUAL(true, bluestein_is_valid_size(1440));
    TEST_ASSERT_EQUAL(true, bluestein_is_valid_size(BLUESTEIN_LARGEST_SIZE));

    // A size of one has no frequency to speak of.
    TEST_ASSERT_EQUAL(false, bluestein_is_valid_size(1));
    TEST_ASSERT_EQUAL(false, bluestein_is_valid_size(0));
    TEST_ASSERT_EQUAL(false,
                      bluestein_is_valid_size(BLUESTEIN_LARGEST_SIZE + 1u));
}

void test_bluestein_transform_size(void)
{
    // The smallest power of two that holds twice the size less one, because
    // the convolution runs that long and anything past the end wraps round.
    TEST_ASSERT_EQUAL(128, bluestein_transform_size(60));
    TEST_ASSERT_EQUAL(256, bluestein_transform_size(100));
    TEST_ASSERT_EQUAL(1024, bluestein_transform_size(360));
    TEST_ASSERT_EQUAL(2048, bluestein_transform_size(1000));

    // A size that is already a power of two still needs nearly twice the
    // room: 511 does not fit in 256.
    TEST_ASSERT_EQUAL(512, bluestein_transform_size(256));

    TEST_ASSERT_EQUAL(0, bluestein_transform_size(0));
}

void test_bluestein_gives_the_same_answer_as_the_fft(void)
{
    // Where both can be used, they must agree. This is the strongest thing
    // that can be said about the module, because the fft module is tested
    // against the transform worked out directly.
    const uint32_t size = 64u;
    fft_t fft = fft_alloc(size);
    bluestein_t bluestein = bluestein_alloc(size);
    cnum_t by_fft[64];
    cnum_t by_bluestein[64];

    for(uint32_t index = 0; index < size; index++)
    {
        real_t value = REAL_SIN(REAL_C(2.0) * REAL_PI * REAL_C(5.0)
                                * (real_t)index / (real_t)size)
                       + (REAL_C(0.4) * (real_t)(index % 7u));

        by_fft[index] = cnum_make(value, value / REAL_C(3.0));
        by_bluestein[index] = by_fft[index];
    }

    fft_forward(&fft, by_fft);
    bluestein_forward(&bluestein, by_bluestein);

    for(uint32_t index = 0; index < size; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), cnum_real(by_fft[index]),
                                cnum_real(by_bluestein[index]));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), cnum_imaginary(by_fft[index]),
                                cnum_imaginary(by_bluestein[index]));
    }

    fft_free(&fft);
    bluestein_free(&bluestein);
}

void test_bluestein_puts_a_tone_in_one_bin_at_a_size_no_power_of_two_holds(void)
{
    // The reason the module exists. A size of 60 at 3000 samples in a second
    // holds a whole number of periods of 50 hertz, thus the mains hum lands in
    // one bin instead of spreading across all of them.
    const uint32_t size = 60u;
    bluestein_t bluestein = bluestein_alloc(size);
    cnum_t data[60];

    for(uint32_t index = 0; index < size; index++)
    {
        // Exactly one period across the block.
        data[index] = cnum_make(REAL_COS(REAL_C(2.0) * REAL_PI * (real_t)index
                                         / (real_t)size), REAL_C(0.0));
    }

    bluestein_forward(&bluestein, data);

    // Half the size at bin 1 and at bin 59, and nothing anywhere else.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(30.0),
                            cnum_magnitude(data[1]));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(30.0),
                            cnum_magnitude(data[59]));

    for(uint32_t index = 0; index < size; index++)
    {
        if((index != 1u) && (index != 59u))
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                                    cnum_magnitude(data[index]));
        }
    }

    bluestein_free(&bluestein);
}

void test_bluestein_round_trip(void)
{
    const uint32_t size = 100u;
    bluestein_t bluestein = bluestein_alloc(size);
    cnum_t data[100];
    cnum_t first[100];

    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = cnum_make((real_t)((index * 13u) % 23u) - REAL_C(11.0),
                                (real_t)((index * 7u) % 11u) - REAL_C(5.0));
        first[index] = data[index];
    }

    bluestein_forward(&bluestein, data);
    bluestein_inverse(&bluestein, data);

    for(uint32_t index = 0; index < size; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), cnum_real(first[index]),
                                cnum_real(data[index]));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), cnum_imaginary(first[index]),
                                cnum_imaginary(data[index]));
    }

    bluestein_free(&bluestein);
}

void test_bluestein_holds_its_digits_where_the_square_of_the_index_is_large(void)
{
    // The fold, held true. Without it the turning factors of a size this large
    // are worked out from an angle that a number of 32 bits cannot hold, and
    // the false answer grows with the size. This is the test that fails if the
    // fold is ever taken out.
    const uint32_t size = 1000u;
    const uint32_t tone = 251u;
    bluestein_t bluestein = bluestein_alloc(size);
    static cnum_t data[1000];

    for(uint32_t index = 0; index < size; index++)
    {
        // Held down to one turn here as well, for the same reason.
        real_t angle = (REAL_C(2.0) * REAL_PI
                        * (real_t)((index * tone) % size)) / (real_t)size;

        data[index] = cnum_make(REAL_COS(angle), REAL_SIN(angle));
    }

    bluestein_forward(&bluestein, data);

    real_t worst = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        if(index != tone)
        {
            real_t leak = cnum_magnitude(data[index]);

            if(leak > worst) { worst = leak; }
        }
    }

    // The tone stands at the size, and every other bin holds a part in ten
    // thousand of it or less. Without the fold this is measured at more than a
    // part in a hundred.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), (real_t)size,
                            cnum_magnitude(data[tone]));
    TEST_ASSERT_TRUE(worst < ((real_t)size / REAL_C(10000.0)));

    bluestein_free(&bluestein);
}

void test_bluestein_bin_frequency(void)
{
    // 3000 samples in a second and a size of 60 puts 50 hertz in bin 1
    // exactly, which no power of two can do.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(50.0),
                            bluestein_bin_frequency(1, 60, REAL_C(3000.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            bluestein_bin_frequency(0, 60, REAL_C(3000.0)));

    // A bin above the middle mirrors a lower one, thus its frequency is given
    // as negative.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), -REAL_C(50.0),
                            bluestein_bin_frequency(59, 60, REAL_C(3000.0)));
}

void test_bluestein_static_alloc(void)
{
    // The same answer with no memory from the heap, for a device that has
    // none to give.
    const uint32_t size = 12u;
    const uint32_t larger = 32u;

    static cnum_t twiddle[16];
    static uint32_t reverse[32];
    static cnum_t chirp[12];
    static cnum_t kernel[32];
    static cnum_t first[32];
    static cnum_t second[32];
    cnum_t data[12];

    TEST_ASSERT_EQUAL(larger, bluestein_transform_size(size));
    TEST_ASSERT_EQUAL(16, FFT_TWIDDLE_COUNT(larger));

    bluestein_t bluestein = bluestein_static_alloc(size, twiddle, reverse,
                                                   chirp, kernel, first,
                                                   second);

    TEST_ASSERT_EQUAL(size, bluestein.size);

    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = cnum_make(REAL_C(1.0), REAL_C(0.0));
    }

    bluestein_forward(&bluestein, data);

    // A flat signal is the size at bin 0 and nothing anywhere else.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), (real_t)size,
                            cnum_magnitude(data[0]));

    for(uint32_t index = 1; index < size; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                                cnum_magnitude(data[index]));
    }

    bluestein_free(&bluestein);
}

void test_bluestein_alloc_refuses_a_size_it_cannot_serve(void)
{
    bluestein_t bluestein = bluestein_alloc(1);

    TEST_ASSERT_EQUAL(0, bluestein.size);

    // Freeing one that was refused is safe, and so is freeing it twice.
    bluestein_free(&bluestein);
    bluestein_free(&bluestein);
}
