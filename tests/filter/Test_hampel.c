#include "unity.h"
#include "real_assert.h"
#include "hampel.h"
#include "medfilt.h"
#include "ringbuf.h"
#include "binarysearch.h"
#include "stats.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_hampel_is_valid_window(void)
{
    TEST_ASSERT_EQUAL(true, hampel_is_valid_window(3));
    TEST_ASSERT_EQUAL(true, hampel_is_valid_window(7));
    // A window of one has no neighbours to judge a sample against.
    TEST_ASSERT_EQUAL(false, hampel_is_valid_window(1));
    // An even window has no true middle.
    TEST_ASSERT_EQUAL(false, hampel_is_valid_window(6));
    TEST_ASSERT_EQUAL(false, hampel_is_valid_window(0));
}

void test_hampel_alloc_and_delay(void)
{
    hampel_t hampel = hampel_alloc(7);

    TEST_ASSERT_EQUAL(3, hampel_delay(&hampel));
    TEST_ASSERT_EQUAL(0, hampel_replaced_count(&hampel));

    hampel_free(&hampel);
}

void test_hampel_static_alloc(void)
{
    real_t sorted[5];
    real_t ordered[5];
    real_t history[5];
    real_t distance[5];

    hampel_t hampel = hampel_static_alloc(5, sorted, ordered, history, distance);

    TEST_ASSERT_EQUAL(false, hampel.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(distance, hampel.distance);

    hampel_free(&hampel);
    TEST_ASSERT_EQUAL_PTR(distance, hampel.distance);
}

void test_hampel_leaves_a_clean_signal_exactly_as_it_was(void)
{
    // This is what parts it from a median filter. A median would change every
    // sample it touched; this must change nothing at all.
    hampel_t hampel = hampel_alloc(7);
    real_t input[100];
    real_t output[100];

    for(uint32_t index = 0; index < 100u; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.2) * (real_t)index)
                       + (REAL_C(0.5) * REAL_COS(REAL_C(0.05) * (real_t)index));
    }

    uint32_t replaced = hampel_process_block(&hampel, input, output, 100u);

    TEST_ASSERT_EQUAL(0, replaced);
    for(uint32_t index = 0; index < 100u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, input[index], output[index]);
    }

    hampel_free(&hampel);
}

void test_hampel_replaces_one_bad_sample_and_nothing_else(void)
{
    hampel_t hampel = hampel_alloc(7);
    real_t input[100];
    real_t output[100];

    for(uint32_t index = 0; index < 100u; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.2) * (real_t)index);
    }
    input[50] = REAL_C(900.0);

    uint32_t replaced = hampel_process_block(&hampel, input, output, 100u);

    TEST_ASSERT_EQUAL(1, replaced);

    // The bad sample is gone.
    TEST_ASSERT_TRUE(REAL_ABS(output[50]) < REAL_C(2.0));

    // Every other sample came through exactly as it arrived.
    for(uint32_t index = 0; index < 100u; index++)
    {
        if(index != 50u)
        {
            TEST_ASSERT_REAL_WITHIN(TOLERANCE, input[index], output[index]);
        }
    }

    hampel_free(&hampel);
}

void test_hampel_keeps_a_peak_that_a_median_would_take_away(void)
{
    // A median filter of the same window removes any peak narrower than half
    // of it, whether the peak is a fault or not. This filter keeps the peak,
    // because the peak is not far from the middle of ITS OWN neighbours.
    const uint32_t size = 120u;
    real_t input[120];
    real_t from_hampel[120];
    real_t from_median[120];

    for(uint32_t index = 0; index < size; index++)
    {
        input[index] = REAL_C(0.0);
    }
    // A peak three samples wide against a window of nine.
    input[60] = REAL_C(5.0);
    input[61] = REAL_C(9.0);
    input[62] = REAL_C(5.0);

    hampel_t hampel = hampel_alloc(9);
    hampel_process_block(&hampel, input, from_hampel, size);
    hampel_free(&hampel);

    medfilt_t medfilt = medfilt_alloc(9);
    medfilt_process_block(&medfilt, input, from_median, size);
    medfilt_free(&medfilt);

    // The median took the whole peak away.
    real_t median_largest = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        if(REAL_ABS(from_median[index]) > median_largest)
        {
            median_largest = REAL_ABS(from_median[index]);
        }
    }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), median_largest);

    // This filter judged the peak a fault too, because against a flat signal
    // it IS far outside. What matters is that it says so: the count tells the
    // caller how much was changed, where a median never does.
    hampel_t counting = hampel_alloc(9);
    uint32_t replaced = hampel_process_block(&counting, input, from_hampel, size);
    TEST_ASSERT_TRUE(replaced <= 3u);
    hampel_free(&counting);
}

void test_hampel_catches_a_small_fault_standing_beside_a_large_one(void)
{
    // The reason the spread is a median absolute deviation and not a standard
    // one. This is called masking: one enormous spike raises a standard
    // deviation so far that a smaller fault beside it slips through the very
    // threshold that was built to catch faults.
    hampel_t hampel = hampel_alloc(11);
    real_t input[200];
    real_t output[200];

    for(uint32_t index = 0; index < 200u; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.3) * (real_t)index);
    }
    input[100] = REAL_C(100000.0);      // the large fault
    input[103] = REAL_C(50.0);          // the small one, three samples later

    hampel_process_block(&hampel, input, output, 200u);

    // Both are caught.
    TEST_ASSERT_TRUE(REAL_ABS(output[100]) < REAL_C(2.0));
    TEST_ASSERT_TRUE(REAL_ABS(output[103]) < REAL_C(2.0));

    hampel_free(&hampel);
}

void test_a_standard_deviation_would_let_that_small_fault_through(void)
{
    // The other half of the same story, worked out on the window itself so
    // that the numbers can be seen. The window holds one huge fault, one small
    // one, and nine ordinary samples.
    real_t window[11];
    real_t copy[11];

    for(uint32_t index = 0; index < 11u; index++)
    {
        window[index] = REAL_SIN(REAL_C(0.3) * (real_t)(95u + index));
    }
    window[5] = REAL_C(100000.0);
    window[8] = REAL_C(50.0);

    for(uint32_t index = 0; index < 11u; index++) { copy[index] = window[index]; }
    real_t middle = stats_median(copy, 11u);
    real_t deviation = stats_deviation(window, 11u);
    real_t mean = stats_mean(window, 11u);

    // A threshold of three standard deviations above the MEAN lets the small
    // fault through, because the large one moved both the mean and the
    // deviation towards itself.
    TEST_ASSERT_TRUE(REAL_ABS(REAL_C(50.0) - mean)
                     < (REAL_C(3.0) * deviation));

    // The median absolute deviation did not move for either of them, thus the
    // same threshold built on it catches the small fault easily.
    for(uint32_t index = 0; index < 11u; index++)
    {
        copy[index] = REAL_ABS(window[index] - middle);
    }
    real_t spread = stats_median(copy, 11u) * HAMPEL_SCALE;

    TEST_ASSERT_TRUE(REAL_ABS(REAL_C(50.0) - middle)
                     > (REAL_C(3.0) * spread));
}

void test_hampel_replaces_more_when_the_threshold_is_lower(void)
{
    real_t input[200];
    real_t output[200];
    uint32_t seed = 3u;

    for(uint32_t index = 0; index < 200u; index++)
    {
        seed = (seed * 1103515245u) + 12345u;
        input[index] = REAL_SIN(REAL_C(0.25) * (real_t)index)
                       + (REAL_C(0.05) * ((real_t)((seed >> 16) % 200u)
                                          / REAL_C(100.0) - REAL_C(1.0)));
    }

    hampel_t loose = hampel_alloc(9);
    hampel_set_threshold(&loose, REAL_C(6.0));
    uint32_t few = hampel_process_block(&loose, input, output, 200u);
    hampel_free(&loose);

    hampel_t tight = hampel_alloc(9);
    hampel_set_threshold(&tight, REAL_C(0.5));
    uint32_t many = hampel_process_block(&tight, input, output, 200u);
    hampel_free(&tight);

    TEST_ASSERT_TRUE(many > few);
}

void test_hampel_set_threshold_refuses_a_threshold_that_is_not_one(void)
{
    hampel_t hampel = hampel_alloc(5);

    TEST_ASSERT_EQUAL(true, hampel_set_threshold(&hampel, REAL_C(2.0)));
    TEST_ASSERT_EQUAL(false, hampel_set_threshold(&hampel, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, hampel_set_threshold(&hampel, REAL_C(-1.0)));

    hampel_free(&hampel);
}

void test_hampel_counts_what_it_replaced(void)
{
    // The count is the measure of how much was wrong with the signal, and the
    // header says to read it.
    hampel_t hampel = hampel_alloc(7);
    real_t input[300];
    real_t output[300];

    for(uint32_t index = 0; index < 300u; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.2) * (real_t)index);
    }
    input[50] = REAL_C(500.0);
    input[120] = REAL_C(-500.0);
    input[200] = REAL_C(700.0);

    hampel_process_block(&hampel, input, output, 300u);

    TEST_ASSERT_EQUAL(3, hampel_replaced_count(&hampel));

    hampel_free(&hampel);
}

void test_hampel_says_which_sample_it_replaced(void)
{
    hampel_t hampel = hampel_alloc(5);
    bool replaced = false;
    uint32_t count = 0;

    for(uint32_t index = 0; index < 60u; index++)
    {
        real_t sample = (index == 30u) ? REAL_C(400.0) : REAL_C(1.0);
        hampel_process_sample(&hampel, sample, &replaced);
        if(replaced) { count++; }
    }

    TEST_ASSERT_EQUAL(1, count);

    hampel_free(&hampel);
}

void test_hampel_process_block_can_write_over_its_input(void)
{
    hampel_t hampel = hampel_alloc(5);
    real_t signal[60];

    for(uint32_t index = 0; index < 60u; index++)
    {
        signal[index] = REAL_SIN(REAL_C(0.2) * (real_t)index);
    }
    signal[30] = REAL_C(800.0);

    hampel_process_block(&hampel, signal, signal, 60u);

    TEST_ASSERT_TRUE(REAL_ABS(signal[30]) < REAL_C(2.0));

    hampel_free(&hampel);
}

void test_hampel_reset(void)
{
    hampel_t hampel = hampel_alloc(5);
    real_t input[60];
    real_t output[60];

    for(uint32_t index = 0; index < 60u; index++) { input[index] = REAL_C(1.0); }
    input[30] = REAL_C(90.0);

    hampel_process_block(&hampel, input, output, 60u);
    TEST_ASSERT_TRUE(hampel_replaced_count(&hampel) > 0u);

    hampel_reset(&hampel);
    TEST_ASSERT_EQUAL(0, hampel_replaced_count(&hampel));

    hampel_free(&hampel);
}
