#include "unity.h"
#include "real_assert.h"
#include "savgol.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_savgol_alloc(void)
{
    savgol_t savgol = savgol_alloc(5);

    TEST_ASSERT_EQUAL(5, savgol.window);
    TEST_ASSERT_EQUAL(true, savgol.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(savgol.coefficient);

    savgol_free(&savgol);
}

void test_savgol_static_alloc(void)
{
    real_t coefficient[7];
    savgol_t savgol = savgol_static_alloc(7, coefficient);

    TEST_ASSERT_EQUAL(7, savgol.window);
    TEST_ASSERT_EQUAL(false, savgol.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(coefficient, savgol.coefficient);

    savgol_free(&savgol);
    TEST_ASSERT_EQUAL_PTR(coefficient, savgol.coefficient);
}

void test_savgol_is_valid(void)
{
    TEST_ASSERT_EQUAL(true, savgol_is_valid(5, 2, 0));
    TEST_ASSERT_EQUAL(true, savgol_is_valid(5, 2, 2));
    // An even window has no middle.
    TEST_ASSERT_EQUAL(false, savgol_is_valid(4, 2, 0));
    // The order must be below the size of the window.
    TEST_ASSERT_EQUAL(false, savgol_is_valid(3, 3, 0));
    // The derivative must not be above the order.
    TEST_ASSERT_EQUAL(false, savgol_is_valid(5, 2, 3));
}

void test_savgol_design_refuses_a_window_that_does_not_fit(void)
{
    savgol_t savgol = savgol_alloc(3);

    TEST_ASSERT_EQUAL(false, savgol_design(&savgol, 3, 0));
    TEST_ASSERT_EQUAL(false, savgol_design(&savgol, 2, 3));
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 0));

    savgol_free(&savgol);
}

void test_savgol_the_known_coefficients_of_a_window_of_five(void)
{
    // For a window of 5 and a polynomial of the second order the coefficients
    // are -3, 12, 17, 12, -3, all divided by 35. These values stand in every
    // book about this filter.
    savgol_t savgol = savgol_alloc(5);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 0));

    real_t expected[5] = {-REAL_C(3.0)/REAL_C(35.0), REAL_C(12.0)/REAL_C(35.0), REAL_C(17.0)/REAL_C(35.0), REAL_C(12.0)/REAL_C(35.0), -REAL_C(3.0)/REAL_C(35.0)};

    for(uint32_t index = 0; index < 5; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, expected[index],
                                 savgol_get_coefficient(&savgol, index));
    }

    savgol_free(&savgol);
}

void test_savgol_the_coefficients_add_up_to_one(void)
{
    // A filter that smooths must let a signal that does not change pass
    // unchanged, thus its coefficients must add up to one.
    uint32_t windows[3] = {5, 9, 15};

    for(uint32_t which = 0; which < 3; which++)
    {
        savgol_t savgol = savgol_alloc(windows[which]);
        TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 3, 0));

        real_t sum = REAL_C(0.0);
        for(uint32_t index = 0; index < windows[which]; index++)
        {
            sum += savgol_get_coefficient(&savgol, index);
        }

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), sum);
        savgol_free(&savgol);
    }
}

void test_savgol_keeps_a_polynomial_of_its_own_order_unchanged(void)
{
    // This is the rule that defines the filter. A signal that is a polynomial
    // of the order of the filter must go through it unchanged.
    savgol_t savgol = savgol_alloc(7);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 0));

    real_t window[7];
    for(uint32_t index = 0; index < 7; index++)
    {
        real_t x = (real_t)index - REAL_C(3.0);
        window[index] = REAL_C(2.0) + (REAL_C(3.0)*x) + (REAL_C(4.0)*x*x);
    }

    // The value of that polynomial at the middle, where x is zero, is 2.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.0), savgol_apply(&savgol, window));

    savgol_free(&savgol);
}

void test_savgol_the_first_derivative_of_a_straight_line_is_its_slope(void)
{
    savgol_t savgol = savgol_alloc(7);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 1));

    real_t window[7];
    for(uint32_t index = 0; index < 7; index++)
    {
        real_t x = (real_t)index - REAL_C(3.0);
        window[index] = REAL_C(10.0) + (REAL_C(2.5)*x);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.5), savgol_apply(&savgol, window));

    savgol_free(&savgol);
}

void test_savgol_the_second_derivative_of_a_square(void)
{
    // For the polynomial a*x*x the second derivative is 2*a.
    savgol_t savgol = savgol_alloc(9);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 2));

    real_t window[9];
    for(uint32_t index = 0; index < 9; index++)
    {
        real_t x = (real_t)index - REAL_C(4.0);
        window[index] = REAL_C(3.0) * x * x;
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(6.0), savgol_apply(&savgol, window));

    savgol_free(&savgol);
}

void test_savgol_the_derivative_of_a_signal_that_does_not_change_is_zero(void)
{
    savgol_t savgol = savgol_alloc(5);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 1));

    real_t window[5] = {REAL_C(7.0), REAL_C(7.0), REAL_C(7.0), REAL_C(7.0), REAL_C(7.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), savgol_apply(&savgol, window));

    savgol_free(&savgol);
}

void test_savgol_takes_noise_out_and_keeps_the_peak(void)
{
    // This is the reason to use this filter. A plain mean makes a peak lower.
    // This filter must keep the height of the peak much better, and it must
    // still take the noise away.
    const uint32_t size = 41;
    real_t clean[41];
    real_t noisy[41];
    real_t smoothed[41];
    real_t noise[8] = {REAL_C(0.2), -REAL_C(0.18), REAL_C(0.15), -REAL_C(0.21), REAL_C(0.19), -REAL_C(0.16), REAL_C(0.22), -REAL_C(0.2)};

    for(uint32_t index = 0; index < size; index++)
    {
        real_t x = ((real_t)index - REAL_C(20.0)) / REAL_C(5.0);
        clean[index] = REAL_C(10.0) * REAL_EXP(-x*x);
        noisy[index] = clean[index] + noise[index % 8];
    }

    savgol_t savgol = savgol_alloc(9);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 3, 0));
    savgol_process_block(&savgol, noisy, smoothed, size);

    real_t before = REAL_C(0.0);
    real_t after = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        before += REAL_ABS(noisy[index] - clean[index]);
        after += REAL_ABS(smoothed[index] - clean[index]);
    }

    // The result must lie nearer to the clean signal.
    TEST_ASSERT_TRUE(after < before);
    // The height of the peak must stay, inside a small error.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.3), REAL_C(10.0), smoothed[20]);

    savgol_free(&savgol);
}

void test_savgol_process_block_writes_every_value(void)
{
    savgol_t savgol = savgol_alloc(5);
    TEST_ASSERT_EQUAL(true, savgol_design(&savgol, 2, 0));

    real_t input[10];
    real_t output[10];
    for(uint32_t index = 0; index < 10; index++)
    {
        input[index] = REAL_C(3.0);
        output[index] = -REAL_C(1.0);
    }

    savgol_process_block(&savgol, input, output, 10);

    // A signal that does not change must come out unchanged everywhere, also
    // at the two ends where the window reaches outside the signal.
    for(uint32_t index = 0; index < 10; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(3.0), output[index]);
    }

    savgol_free(&savgol);
}

void test_savgol_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    real_t coefficient[7];

    savgol_t dynamic_savgol = savgol_alloc(7);
    savgol_t static_savgol = savgol_static_alloc(7, coefficient);

    TEST_ASSERT_EQUAL(true, savgol_design(&dynamic_savgol, 3, 0));
    TEST_ASSERT_EQUAL(true, savgol_design(&static_savgol, 3, 0));

    for(uint32_t index = 0; index < 7; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE,
                                 savgol_get_coefficient(&dynamic_savgol, index),
                                 savgol_get_coefficient(&static_savgol, index));
    }

    savgol_free(&dynamic_savgol);
    savgol_free(&static_savgol);
}

void test_savgol_free_releases_a_dynamic_filter(void)
{
    savgol_t savgol = savgol_alloc(5);

    savgol_free(&savgol);

    TEST_ASSERT_NULL(savgol.coefficient);
    TEST_ASSERT_EQUAL(false, savgol.dynamic_alloc);

    savgol_free(&savgol);
    TEST_ASSERT_NULL(savgol.coefficient);
}
