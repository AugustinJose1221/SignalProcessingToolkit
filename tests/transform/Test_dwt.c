#include "unity.h"
#include "real_assert.h"
#include "dwt.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_dwt_init_haar(void)
{
    dwt_t dwt = dwt_init(DWT_HAAR);

    TEST_ASSERT_EQUAL(DWT_HAAR, dwt.wavelet);
    TEST_ASSERT_EQUAL(2, dwt.length);
    // The two coefficients of Haar are both one divided by the square root of
    // two.
    real_t expected = REAL_C(1.0) / REAL_SQRT(REAL_C(2.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, expected, dwt.low[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, expected, dwt.low[1]);
}

void test_dwt_init_daubechies(void)
{
    dwt_t dwt = dwt_init(DWT_DAUBECHIES4);

    TEST_ASSERT_EQUAL(DWT_DAUBECHIES4, dwt.wavelet);
    TEST_ASSERT_EQUAL(4, dwt.length);
}

void test_dwt_the_filters_hold_the_energy(void)
{
    // The sum of the squares of the coefficients must be one for both filters.
    // A transform that holds this rule loses no energy.
    dwt_wavelet_t wavelets[2] = {DWT_HAAR, DWT_DAUBECHIES4};

    for(uint32_t which = 0; which < 2; which++)
    {
        dwt_t dwt = dwt_init(wavelets[which]);
        real_t low_sum = REAL_C(0.0);
        real_t high_sum = REAL_C(0.0);

        for(uint32_t index = 0; index < dwt.length; index++)
        {
            low_sum += dwt.low[index] * dwt.low[index];
            high_sum += dwt.high[index] * dwt.high[index];
        }

        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), low_sum);
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), high_sum);
    }
}

void test_dwt_is_valid_size(void)
{
    TEST_ASSERT_EQUAL(true, dwt_is_valid_size(8, 1));
    TEST_ASSERT_EQUAL(true, dwt_is_valid_size(8, 2));
    TEST_ASSERT_EQUAL(true, dwt_is_valid_size(8, 3));
    // A fourth level would leave one sample only.
    TEST_ASSERT_EQUAL(false, dwt_is_valid_size(8, 4));
    // An odd size cannot go through even one level.
    TEST_ASSERT_EQUAL(false, dwt_is_valid_size(7, 1));
    TEST_ASSERT_EQUAL(false, dwt_is_valid_size(12, 3));
    TEST_ASSERT_EQUAL(false, dwt_is_valid_size(8, 0));
}

void test_dwt_haar_gives_the_mean_and_the_difference(void)
{
    // The transform of Haar gives the sum and the difference of each pair,
    // both divided by the square root of two.
    dwt_t dwt = dwt_init(DWT_HAAR);
    real_t signal[4] = {REAL_C(1.0), REAL_C(3.0), REAL_C(5.0), REAL_C(9.0)};
    real_t approximation[2];
    real_t detail[2];

    dwt_forward(&dwt, signal, 4, approximation, detail);

    real_t root = REAL_SQRT(REAL_C(2.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0)/root, approximation[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(14.0)/root, approximation[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(2.0)/root, detail[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(4.0)/root, detail[1]);
}

void test_dwt_the_detail_of_a_signal_that_does_not_change_is_zero(void)
{
    // A signal with the same value at every place holds no fast part.
    dwt_wavelet_t wavelets[2] = {DWT_HAAR, DWT_DAUBECHIES4};

    for(uint32_t which = 0; which < 2; which++)
    {
        dwt_t dwt = dwt_init(wavelets[which]);
        real_t signal[8];
        real_t approximation[4];
        real_t detail[4];

        for(uint32_t index = 0; index < 8; index++)
        {
            signal[index] = REAL_C(5.0);
        }

        dwt_forward(&dwt, signal, 8, approximation, detail);

        for(uint32_t index = 0; index < 4; index++)
        {
            TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), detail[index]);
        }
    }
}

void test_dwt_the_inverse_gives_the_signal_again(void)
{
    dwt_wavelet_t wavelets[2] = {DWT_HAAR, DWT_DAUBECHIES4};

    for(uint32_t which = 0; which < 2; which++)
    {
        dwt_t dwt = dwt_init(wavelets[which]);
        real_t signal[16];
        real_t original[16];
        real_t approximation[8];
        real_t detail[8];

        for(uint32_t index = 0; index < 16; index++)
        {
            signal[index] = REAL_SIN(REAL_C(0.4)*(real_t)index) + (REAL_C(0.1)*(real_t)index);
            original[index] = signal[index];
        }

        dwt_forward(&dwt, signal, 16, approximation, detail);
        dwt_inverse(&dwt, approximation, detail, 16, signal);

        for(uint32_t index = 0; index < 16; index++)
        {
            TEST_ASSERT_REAL_WITHIN(TOLERANCE, original[index], signal[index]);
        }
    }
}

void test_dwt_the_transform_holds_the_energy(void)
{
    // The sum of the squares of the result must be the same as the sum of the
    // squares of the signal.
    dwt_t dwt = dwt_init(DWT_DAUBECHIES4);
    real_t signal[16];
    real_t approximation[8];
    real_t detail[8];
    real_t before = REAL_C(0.0);
    real_t after = REAL_C(0.0);

    for(uint32_t index = 0; index < 16; index++)
    {
        signal[index] = REAL_SIN(REAL_C(0.7)*(real_t)index);
        before += signal[index] * signal[index];
    }

    dwt_forward(&dwt, signal, 16, approximation, detail);

    for(uint32_t index = 0; index < 8; index++)
    {
        after += (approximation[index]*approximation[index])
                 + (detail[index]*detail[index]);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), before, after);
}

void test_dwt_the_detail_finds_where_a_step_lies(void)
{
    // A signal that jumps at one place. The detail must be large at that place
    // and small everywhere else.
    dwt_t dwt = dwt_init(DWT_HAAR);
    real_t signal[16];
    real_t approximation[8];
    real_t detail[8];

    // The step lies between the samples 6 and 7. The wavelet of Haar looks at
    // the pairs (0,1), (2,3) and so on, thus the step lies inside the pair 3.
    // A step between the samples 7 and 8 would lie between two pairs, where
    // both samples of every pair are the same and every detail is zero. Such a
    // step shows itself in the next level and not in this one.
    for(uint32_t index = 0; index < 16; index++)
    {
        signal[index] = (index < 7) ? REAL_C(0.0) : REAL_C(10.0);
    }

    dwt_forward(&dwt, signal, 16, approximation, detail);

    TEST_ASSERT_TRUE(REAL_ABS(detail[3]) > REAL_C(5.0));

    for(uint32_t index = 0; index < 8; index++)
    {
        if(index != 3)
        {
            TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), detail[index]);
        }
    }
}

void test_dwt_several_levels_and_back_give_the_signal_again(void)
{
    dwt_wavelet_t wavelets[2] = {DWT_HAAR, DWT_DAUBECHIES4};

    for(uint32_t which = 0; which < 2; which++)
    {
        dwt_t dwt = dwt_init(wavelets[which]);
        real_t signal[32];
        real_t original[32];
        real_t work[32];

        for(uint32_t index = 0; index < 32; index++)
        {
            signal[index] = REAL_COS(REAL_C(0.3)*(real_t)index) + (REAL_C(0.05)*(real_t)index*(real_t)index);
            original[index] = signal[index];
        }

        dwt_forward_multi(&dwt, signal, 32, 3, work);
        dwt_inverse_multi(&dwt, signal, 32, 3, work);

        for(uint32_t index = 0; index < 32; index++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), original[index], signal[index]);
        }
    }
}

void test_dwt_threshold_sets_the_small_values_to_zero(void)
{
    real_t data[5] = {REAL_C(0.1), -REAL_C(2.0), REAL_C(0.05), REAL_C(3.0), -REAL_C(0.2)};

    dwt_threshold(data, 5, REAL_C(0.5));

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), data[0]);
    TEST_ASSERT_EQUAL_REAL(-REAL_C(2.0), data[1]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), data[2]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(3.0), data[3]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), data[4]);
}

void test_dwt_the_threshold_takes_noise_out_and_keeps_the_step(void)
{
    // This is the main use of the transform. The signal holds a step and a
    // little noise. After the transform, the threshold and the inverse
    // transform, the result must lie nearer to the clean signal than the noisy
    // signal does.
    dwt_t dwt = dwt_init(DWT_HAAR);
    const uint32_t size = 64;
    real_t clean[64];
    real_t noisy[64];
    real_t work[64];
    real_t noise[8] = {REAL_C(0.3), -REAL_C(0.25), REAL_C(0.2), -REAL_C(0.3), REAL_C(0.28), -REAL_C(0.22), REAL_C(0.26), -REAL_C(0.27)};

    for(uint32_t index = 0; index < size; index++)
    {
        clean[index] = (index < 32) ? REAL_C(0.0) : REAL_C(5.0);
        noisy[index] = clean[index] + noise[index % 8];
    }

    real_t before = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        before += REAL_ABS(noisy[index] - clean[index]);
    }

    dwt_forward_multi(&dwt, noisy, size, 2, work);
    // Leave the approximation of the last level as it is, and clear the small
    // values of every detail.
    dwt_threshold(&noisy[size/4], size - (size/4), REAL_C(0.6));
    dwt_inverse_multi(&dwt, noisy, size, 2, work);

    real_t after = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        after += REAL_ABS(noisy[index] - clean[index]);
    }

    TEST_ASSERT_TRUE(after < before);
}
