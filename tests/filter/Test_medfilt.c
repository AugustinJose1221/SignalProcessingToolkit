#include "unity.h"
#include "real_assert.h"
#include "medfilt.h"
#include "ringbuf.h"
#include "binarysearch.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_medfilt_alloc(void)
{
    medfilt_t medfilt = medfilt_alloc(5);

    TEST_ASSERT_EQUAL(5, medfilt.window.size);
    TEST_ASSERT_EQUAL(0, medfilt_count(&medfilt));
    TEST_ASSERT_NOT_NULL(medfilt.sorted);

    medfilt_free(&medfilt);
}

void test_medfilt_static_alloc(void)
{
    real_t window[3];
    real_t sorted[3];
    medfilt_t medfilt = medfilt_static_alloc(3, window, sorted);

    TEST_ASSERT_EQUAL(false, medfilt.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(sorted, medfilt.sorted);

    medfilt_free(&medfilt);
    TEST_ASSERT_EQUAL_PTR(sorted, medfilt.sorted);
}

void test_medfilt_gives_the_middle_of_the_window(void)
{
    medfilt_t medfilt = medfilt_alloc(3);

    // One sample: the median is that sample.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0),
                             medfilt_process_sample(&medfilt, REAL_C(5.0)));
    // Two samples, 1 and 5: the median lies between them.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0),
                             medfilt_process_sample(&medfilt, REAL_C(1.0)));
    // Three samples, 1, 5 and 9 in order: the middle one is 5.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0),
                             medfilt_process_sample(&medfilt, REAL_C(9.0)));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_medfilt_is_right_while_the_window_still_fills(void)
{
    medfilt_t medfilt = medfilt_alloc(5);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0),
                             medfilt_process_sample(&medfilt, REAL_C(4.0)));
    // Two samples, thus the median lies between them.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0),
                             medfilt_process_sample(&medfilt, REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0),
                             medfilt_process_sample(&medfilt, REAL_C(6.0)));

    medfilt_free(&medfilt);
}

void test_medfilt_removes_one_bad_sample_completely(void)
{
    // This is the whole reason the filter exists. One sample is a thousand
    // times too large. Every answer must be as if it had never arrived.
    medfilt_t medfilt = medfilt_alloc(5);
    real_t input[11] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0),
                       REAL_C(1000.0),
                       REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};
    real_t output[11];

    medfilt_process_block(&medfilt, input, output, 11u);

    for(uint32_t index = 0; index < 11u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[index]);
    }

    medfilt_free(&medfilt);
}

void test_medfilt_keeps_an_edge_that_a_mean_would_round(void)
{
    // A step from 0 to 10. A mean would climb through the step over the whole
    // width of its window. The median steps over at once, in the middle.
    medfilt_t medfilt = medfilt_alloc(5);
    real_t input[12] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                       REAL_C(10.0), REAL_C(10.0), REAL_C(10.0), REAL_C(10.0), REAL_C(10.0), REAL_C(10.0)};
    real_t output[12];

    medfilt_process_block(&medfilt, input, output, 12u);

    // Every answer is either 0 or 10, never a value between them.
    for(uint32_t index = 0; index < 12u; index++)
    {
        bool low = REAL_ABS(output[index]) < TOLERANCE;
        bool high = REAL_ABS(output[index] - REAL_C(10.0)) < TOLERANCE;
        TEST_ASSERT_TRUE(low || high);
    }

    medfilt_free(&medfilt);
}

void test_medfilt_takes_out_a_peak_narrower_than_half_the_window(void)
{
    // The trap that the header warns about. A real peak that is narrower than
    // half the window goes away with the faults.
    medfilt_t medfilt = medfilt_alloc(9);
    real_t input[20];
    real_t output[20];

    for(uint32_t index = 0; index < 20u; index++)
    {
        input[index] = REAL_C(0.0);
    }
    // A peak of three samples, against a window of nine.
    input[9] = REAL_C(5.0);
    input[10] = REAL_C(5.0);
    input[11] = REAL_C(5.0);

    medfilt_process_block(&medfilt, input, output, 20u);

    for(uint32_t index = 14; index < 20u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), output[index]);
    }

    medfilt_free(&medfilt);
}

void test_medfilt_handles_the_same_value_many_times(void)
{
    // The ordered list holds one place for each sample, thus a value that
    // stands more than once must be put in and taken out the right number of
    // times. Getting this wrong loses samples quietly.
    medfilt_t medfilt = medfilt_alloc(5);

    for(uint32_t index = 0; index < 20u; index++)
    {
        medfilt_process_sample(&medfilt, REAL_C(7.0));
    }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), medfilt_get_median(&medfilt));

    // Now push them all out with another value that also repeats.
    for(uint32_t index = 0; index < 5u; index++)
    {
        medfilt_process_sample(&medfilt, REAL_C(2.0));
    }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_medfilt_handles_a_rising_signal(void)
{
    // Every new sample is larger than every sample held. This drives the
    // insertion to the end of the ordered list every time, which is the case
    // that the search of the library reports differently and that the module
    // must put right.
    medfilt_t medfilt = medfilt_alloc(5);

    for(uint32_t index = 1; index <= 20u; index++)
    {
        medfilt_process_sample(&medfilt, (real_t)index);
    }

    // The window holds 16 to 20, whose middle is 18.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(18.0), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_medfilt_handles_a_falling_signal(void)
{
    // Every new sample is smaller than every sample held, thus the insertion
    // always goes to the front.
    medfilt_t medfilt = medfilt_alloc(5);

    for(uint32_t index = 20u; index >= 1u; index--)
    {
        medfilt_process_sample(&medfilt, (real_t)index);
    }

    // The window holds 1 to 5, whose middle is 3.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_medfilt_of_an_even_window_gives_the_mean_of_the_two_middle(void)
{
    medfilt_t medfilt = medfilt_alloc(4);

    medfilt_process_sample(&medfilt, REAL_C(1.0));
    medfilt_process_sample(&medfilt, REAL_C(2.0));
    medfilt_process_sample(&medfilt, REAL_C(3.0));
    medfilt_process_sample(&medfilt, REAL_C(4.0));

    // The two middle samples are 2 and 3.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.5), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_medfilt_get_percentile(void)
{
    medfilt_t medfilt = medfilt_alloc(5);

    for(uint32_t index = 1; index <= 5u; index++)
    {
        medfilt_process_sample(&medfilt, (real_t)index);
    }

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), medfilt_get_percentile(&medfilt, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), medfilt_get_percentile(&medfilt, REAL_C(0.25)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), medfilt_get_percentile(&medfilt, REAL_C(0.5)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), medfilt_get_percentile(&medfilt, REAL_C(1.0)));

    medfilt_free(&medfilt);
}

void test_medfilt_process_block_can_write_over_its_input(void)
{
    medfilt_t medfilt = medfilt_alloc(3);
    real_t signal[5] = {REAL_C(1.0), REAL_C(100.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};

    medfilt_process_block(&medfilt, signal, signal, 5u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), signal[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), signal[4]);

    medfilt_free(&medfilt);
}

void test_medfilt_reset(void)
{
    medfilt_t medfilt = medfilt_alloc(3);

    medfilt_process_sample(&medfilt, REAL_C(10.0));
    medfilt_process_sample(&medfilt, REAL_C(20.0));
    medfilt_reset(&medfilt);

    TEST_ASSERT_EQUAL(0, medfilt_count(&medfilt));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), medfilt_get_median(&medfilt));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0),
                             medfilt_process_sample(&medfilt, REAL_C(4.0)));

    medfilt_free(&medfilt);
}

void test_medfilt_of_one_sample_passes_the_signal_through(void)
{
    medfilt_t medfilt = medfilt_alloc(1);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0),
                             medfilt_process_sample(&medfilt, REAL_C(3.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(900.0),
                             medfilt_process_sample(&medfilt, REAL_C(900.0)));

    medfilt_free(&medfilt);
}

void test_medfilt_is_full_and_count(void)
{
    medfilt_t medfilt = medfilt_alloc(3);

    TEST_ASSERT_EQUAL(false, medfilt_is_full(&medfilt));
    medfilt_process_sample(&medfilt, REAL_C(1.0));
    TEST_ASSERT_EQUAL(1, medfilt_count(&medfilt));
    medfilt_process_sample(&medfilt, REAL_C(1.0));
    medfilt_process_sample(&medfilt, REAL_C(1.0));
    TEST_ASSERT_EQUAL(true, medfilt_is_full(&medfilt));
    medfilt_process_sample(&medfilt, REAL_C(1.0));
    TEST_ASSERT_EQUAL(3, medfilt_count(&medfilt));

    medfilt_free(&medfilt);
}

void test_a_percentile_of_a_filter_that_has_seen_nothing_is_nothing(void)
{
    medfilt_t medfilt = medfilt_alloc(5u);

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           medfilt_get_percentile(&medfilt, REAL_C(0.5)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), medfilt_get_median(&medfilt));

    medfilt_free(&medfilt);
}

void test_a_percentile_of_one_sample_is_that_sample_whatever_is_asked(void)
{
    medfilt_t medfilt = medfilt_alloc(5u);

    medfilt_process_sample(&medfilt, REAL_C(7.0));

    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           medfilt_get_percentile(&medfilt, REAL_C(0.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           medfilt_get_percentile(&medfilt, REAL_C(0.5)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           medfilt_get_percentile(&medfilt, REAL_C(1.0)));

    medfilt_free(&medfilt);
}

void test_the_ends_of_the_range_give_the_smallest_and_the_largest(void)
{
    // A part of 0 asks for the bottom of the window and a part of 1 for the
    // top. A part outside that range has no meaning and is held to the nearest
    // end rather than reaching past the list.
    medfilt_t medfilt = medfilt_alloc(5u);
    real_t samples[5] = {REAL_C(3.0), REAL_C(1.0), REAL_C(4.0), REAL_C(5.0),
                         REAL_C(2.0)};

    for(uint32_t index = 0; index < 5u; index++)
    {
        medfilt_process_sample(&medfilt, samples[index]);
    }

    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0),
                           medfilt_get_percentile(&medfilt, REAL_C(0.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(5.0),
                           medfilt_get_percentile(&medfilt, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0),
                           medfilt_get_percentile(&medfilt, REAL_C(-1.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(5.0),
                           medfilt_get_percentile(&medfilt, REAL_C(2.0)));

    medfilt_free(&medfilt);
}
