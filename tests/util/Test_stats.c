#include "unity.h"
#include "real_assert.h"
#include "stats.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_stats_sum_and_mean(void)
{
    real_t data[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(15.0), stats_sum(data, 5u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), stats_mean(data, 5u));
}

void test_stats_variance_and_deviation(void)
{
    // The distances from the mean are -2, -1, 0, 1, 2, thus the sum of the
    // squares is 10 and the variance is 2.
    real_t data[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), stats_variance(data, 5u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_SQRT(REAL_C(2.0)), stats_deviation(data, 5u));
}

void test_stats_variance_on_a_large_offset_shows_what_the_width_costs(void)
{
    // These samples sit at eight million and move by one, thus the variance is
    // exactly 2. Working that out needs more digits than the samples
    // themselves carry, and this test records what each width really gives.
    //
    // The variance already takes the mean away before it squares, which is the
    // careful way. The digits still run out in 32 bits, and they run out in
    // the SUM: adding five samples near eight million gives a total near forty
    // million, where one step of a float is 4.
    real_t data[5] = {REAL_C(8000000.0), REAL_C(8000001.0), REAL_C(8000002.0),
                      REAL_C(8000003.0), REAL_C(8000004.0)};

#if defined(SPTK_REAL_64)
    // Sixteen digits are enough, thus the answer is right.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(2.0), stats_variance(data, 5u));
#else
    // Seven digits are not enough. The answer comes out as 2.25, which is out
    // by an eighth. THIS IS WHAT A BUILD IN 64 BITS BUYS, and a caller whose
    // readings sit far from zero should know it before choosing a width.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.25), stats_variance(data, 5u));
#endif
}

void test_stats_rms_is_not_the_deviation(void)
{
    // A signal that sits at 100 and moves by 1. The root mean square follows
    // the level, the deviation follows the movement.
    real_t data[4] = {REAL_C(99.0), REAL_C(100.0), REAL_C(100.0), REAL_C(101.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(100.0), stats_rms(data, 4u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.7071), stats_deviation(data, 4u));
}

void test_stats_min_and_max(void)
{
    real_t data[5] = {REAL_C(3.0), -REAL_C(1.0), REAL_C(4.0), REAL_C(1.0), -REAL_C(5.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(5.0), stats_min(data, 5u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), stats_max(data, 5u));
}

void test_stats_median_of_an_odd_list(void)
{
    real_t data[5] = {REAL_C(5.0), REAL_C(1.0), REAL_C(3.0), REAL_C(2.0), REAL_C(4.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), stats_median(data, 5u));
}

void test_stats_median_of_an_even_list(void)
{
    // The two middle samples are 3 and 4, thus the median is 3.5.
    real_t data[6] = {REAL_C(5.0), REAL_C(1.0), REAL_C(3.0), REAL_C(2.0), REAL_C(4.0), REAL_C(6.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.5), stats_median(data, 6u));
}

void test_stats_median_of_one_sample(void)
{
    real_t data[1] = {REAL_C(7.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), stats_median(data, 1u));
}

void test_stats_median_works_on_a_list_that_is_already_in_order(void)
{
    // A list in order is the worst case of the method that the select uses if
    // the pivot is badly chosen. The module takes the middle sample for that
    // reason, and this test holds it.
    real_t rising[9] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5.0), REAL_C(6.0), REAL_C(7.0), REAL_C(8.0), REAL_C(9.0)};
    real_t falling[9] = {REAL_C(9.0), REAL_C(8.0), REAL_C(7.0), REAL_C(6.0), REAL_C(5.0), REAL_C(4.0), REAL_C(3.0), REAL_C(2.0), REAL_C(1.0)};
    real_t same[9] = {REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), stats_median(rising, 9u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), stats_median(falling, 9u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), stats_median(same, 9u));
}

void test_stats_median_stands_against_one_bad_sample(void)
{
    // This is the whole reason the robust measures are here. One sample is a
    // thousand times too large. The mean follows it and the median does not.
    real_t forMean[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5000.0)};
    real_t forMedian[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5000.0)};

    TEST_ASSERT_TRUE(stats_mean(forMean, 5u) > REAL_C(1000.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), stats_median(forMedian, 5u));
}

void test_stats_percentile(void)
{
    real_t data[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5.0)};
    real_t copy[5];

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), stats_percentile(copy, 5u, REAL_C(0.0)));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), stats_percentile(copy, 5u, REAL_C(0.5)));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), stats_percentile(copy, 5u, REAL_C(1.0)));

    for(uint32_t index = 0; index < 5u; index++) { copy[index] = data[index]; }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), stats_percentile(copy, 5u, REAL_C(0.25)));
}

void test_stats_percentile_stands_between_two_samples(void)
{
    // The place asked for is 0.125*3 = 0.375, thus the answer lies between the
    // first and the second sample, at 0.375 of the way.
    real_t data[4] = {REAL_C(0.0), REAL_C(8.0), REAL_C(16.0), REAL_C(24.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), stats_percentile(data, 4u, REAL_C(0.125)));
}

void test_stats_mad(void)
{
    // The median is 3. The distances are 2, 1, 0, 1, 2, whose median is 1.
    real_t data[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0), REAL_C(5.0)};
    real_t work[5];

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), stats_mad(data, 5u, work));
}

void test_stats_mad_leaves_the_data_as_it_was(void)
{
    // The median and the percentile reorder what they are given. This one must
    // not, because it has a work list of its own.
    real_t data[5] = {REAL_C(5.0), REAL_C(1.0), REAL_C(3.0), REAL_C(2.0), REAL_C(4.0)};
    real_t work[5];

    stats_mad(data, 5u, work);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), data[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), data[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), data[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), data[3]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), data[4]);
}

void test_stats_mad_stands_against_bad_samples(void)
{
    // Two samples of five are wrong. The deviation is thrown far off; the
    // median absolute deviation, scaled, still reports the spread of the rest.
    real_t clean[5] = {REAL_C(10.0), REAL_C(11.0), REAL_C(12.0), REAL_C(13.0), REAL_C(14.0)};
    real_t dirty[5] = {REAL_C(10.0), REAL_C(11.0), REAL_C(12.0), REAL_C(900.0), REAL_C(1000.0)};
    real_t work[5];

    real_t clean_mad = stats_mad(clean, 5u, work) * STATS_MAD_TO_DEVIATION;
    real_t dirty_mad = stats_mad(dirty, 5u, work) * STATS_MAD_TO_DEVIATION;

    TEST_ASSERT_TRUE(stats_deviation(dirty, 5u) > (REAL_C(20.0) * stats_deviation(clean, 5u)));
    TEST_ASSERT_TRUE(dirty_mad < (REAL_C(3.0) * clean_mad));
}

void test_stats_of_an_empty_list_is_nothing(void)
{
    real_t data[1] = {REAL_C(1.0)};
    real_t work[1];

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_sum(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_mean(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_variance(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_deviation(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_rms(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_min(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_max(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_median(data, 0u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_percentile(data, 0u, REAL_C(0.5)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), stats_mad(data, 0u, work));
}

void test_a_percentile_of_one_reading_is_that_reading(void)
{
    // One reading has no spread. Every percentile of it is the reading itself,
    // and the arithmetic that finds a place between two readings has no second
    // reading to reach for.
    real_t only[1] = {REAL_C(7.5)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(7.5), stats_percentile(only, 1u,
                                                          REAL_C(0.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.5), stats_percentile(only, 1u,
                                                          REAL_C(0.5)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.5), stats_percentile(only, 1u,
                                                          REAL_C(1.0)));
}

void test_a_percentile_reads_the_same_whatever_order_the_readings_arrive_in(void)
{
    // The readings are NOT sorted first. The place is found by counting how
    // many stand below it and then reaching for the nearest one above, thus
    // the answer must not depend on the order the readings were written in.
    real_t rising[7] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0),
                        REAL_C(5.0), REAL_C(6.0), REAL_C(7.0)};
    real_t jumbled[7] = {REAL_C(5.0), REAL_C(1.0), REAL_C(7.0), REAL_C(3.0),
                         REAL_C(6.0), REAL_C(2.0), REAL_C(4.0)};

    for(uint32_t step = 0; step <= 10u; step++)
    {
        real_t part = REAL_C(0.1) * (real_t)step;

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                stats_percentile(rising, 7u, part),
                                stats_percentile(jumbled, 7u, part));
    }
}
