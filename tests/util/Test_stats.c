#include "unity.h"
#include "stats.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.0001f

void setUp(void)
{

}

void tearDown(void)
{

}

void test_stats_sum_and_mean(void)
{
    float data[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 15.0f, stats_sum(data, 5u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, stats_mean(data, 5u));
}

void test_stats_variance_and_deviation(void)
{
    // The distances from the mean are -2, -1, 0, 1, 2, thus the sum of the
    // squares is 10 and the variance is 2.
    float data[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, stats_variance(data, 5u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, sqrtf(2.0f), stats_deviation(data, 5u));
}

void test_stats_variance_holds_up_on_a_large_offset(void)
{
    // This is why the variance takes the mean away before it squares.
    //
    // These samples sit at eight million and move by one. The one pass method,
    // which is the mean of the squares less the square of the mean, works out
    // two numbers near 64 000 000 000 000 whose difference is the answer. A
    // float holds seven digits, thus that answer would be lost.
    float data[5] = {8000000.0f, 8000001.0f, 8000002.0f, 8000003.0f, 8000004.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, stats_variance(data, 5u));
}

void test_stats_rms_is_not_the_deviation(void)
{
    // A signal that sits at 100 and moves by 1. The root mean square follows
    // the level, the deviation follows the movement.
    float data[4] = {99.0f, 100.0f, 100.0f, 101.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, stats_rms(data, 4u));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.7071f, stats_deviation(data, 4u));
}

void test_stats_min_and_max(void)
{
    float data[5] = {3.0f, -1.0f, 4.0f, 1.0f, -5.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -5.0f, stats_min(data, 5u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, stats_max(data, 5u));
}

void test_stats_median_of_an_odd_list(void)
{
    float data[5] = {5.0f, 1.0f, 3.0f, 2.0f, 4.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, stats_median(data, 5u));
}

void test_stats_median_of_an_even_list(void)
{
    // The two middle samples are 3 and 4, thus the median is 3.5.
    float data[6] = {5.0f, 1.0f, 3.0f, 2.0f, 4.0f, 6.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.5f, stats_median(data, 6u));
}

void test_stats_median_of_one_sample(void)
{
    float data[1] = {7.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 7.0f, stats_median(data, 1u));
}

void test_stats_median_works_on_a_list_that_is_already_in_order(void)
{
    // A list in order is the worst case of the method that the select uses if
    // the pivot is badly chosen. The module takes the middle sample for that
    // reason, and this test holds it.
    float rising[9] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    float falling[9] = {9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
    float same[9] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, stats_median(rising, 9u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, stats_median(falling, 9u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, stats_median(same, 9u));
}

void test_stats_median_stands_against_one_bad_sample(void)
{
    // This is the whole reason the robust measures are here. One sample is a
    // thousand times too large. The mean follows it and the median does not.
    float forMean[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5000.0f};
    float forMedian[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5000.0f};

    TEST_ASSERT_TRUE(stats_mean(forMean, 5u) > 1000.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, stats_median(forMedian, 5u));
}

void test_stats_percentile(void)
{
    float data[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float copy[5];

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, stats_percentile(copy, 5u, 0.0f));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, stats_percentile(copy, 5u, 0.5f));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, stats_percentile(copy, 5u, 1.0f));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, stats_percentile(copy, 5u, 0.25f));
}

void test_stats_percentile_stands_between_two_samples(void)
{
    // The place asked for is 0.125*3 = 0.375, thus the answer lies between the
    // first and the second sample, at 0.375 of the way.
    float data[4] = {0.0f, 8.0f, 16.0f, 24.0f};

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, stats_percentile(data, 4u, 0.125f));
}

void test_stats_mad(void)
{
    // The median is 3. The distances are 2, 1, 0, 1, 2, whose median is 1.
    float data[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float work[5];

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, stats_mad(data, 5u, work));
}

void test_stats_mad_leaves_the_data_as_it_was(void)
{
    // The median and the percentile reorder what they are given. This one must
    // not, because it has a work list of its own.
    float data[5] = {5.0f, 1.0f, 3.0f, 2.0f, 4.0f};
    float work[5];

    stats_mad(data, 5u, work);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, data[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, data[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, data[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, data[3]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, data[4]);
}

void test_stats_mad_stands_against_bad_samples(void)
{
    // Two samples of five are wrong. The deviation is thrown far off; the
    // median absolute deviation, scaled, still reports the spread of the rest.
    float clean[5] = {10.0f, 11.0f, 12.0f, 13.0f, 14.0f};
    float dirty[5] = {10.0f, 11.0f, 12.0f, 900.0f, 1000.0f};
    float work[5];

    float clean_mad = stats_mad(clean, 5u, work) * STATS_MAD_TO_DEVIATION;
    float dirty_mad = stats_mad(dirty, 5u, work) * STATS_MAD_TO_DEVIATION;

    TEST_ASSERT_TRUE(stats_deviation(dirty, 5u) > (20.0f * stats_deviation(clean, 5u)));
    TEST_ASSERT_TRUE(dirty_mad < (3.0f * clean_mad));
}

void test_stats_of_an_empty_list_is_nothing(void)
{
    float data[1] = {1.0f};
    float work[1];

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_sum(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_mean(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_variance(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_deviation(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_rms(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_min(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_max(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_median(data, 0u));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_percentile(data, 0u, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, stats_mad(data, 0u, work));
}
