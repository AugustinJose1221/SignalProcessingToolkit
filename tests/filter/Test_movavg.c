#include "unity.h"
#include "real_assert.h"
#include "movavg.h"
#include "fir.h"
#include "window.h"
#include "ringbuf.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_movavg_alloc(void)
{
    movavg_t movavg = movavg_alloc(8);

    TEST_ASSERT_EQUAL(8, movavg.window.size);
    TEST_ASSERT_EQUAL(0, movavg_count(&movavg));
    TEST_ASSERT_EQUAL(false, movavg_is_full(&movavg));

    movavg_free(&movavg);
}

void test_movavg_static_alloc(void)
{
    real_t data[4];
    movavg_t movavg = movavg_static_alloc(4, data);

    TEST_ASSERT_EQUAL(4, movavg.window.size);
    TEST_ASSERT_EQUAL(false, movavg.window.dynamic_alloc);

    movavg_free(&movavg);
    TEST_ASSERT_EQUAL_PTR(data, movavg.window.data);
}

void test_movavg_is_right_while_the_window_still_fills(void)
{
    // The mean must be taken over the samples that have arrived and not over
    // the whole size, or the answer would start low and creep up.
    movavg_t movavg = movavg_alloc(4);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), movavg_process_sample(&movavg, REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), movavg_process_sample(&movavg, REAL_C(4.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), movavg_process_sample(&movavg, REAL_C(6.0)));

    movavg_free(&movavg);
}

void test_movavg_holds_the_mean_of_the_last_samples(void)
{
    movavg_t movavg = movavg_alloc(3);

    movavg_process_sample(&movavg, REAL_C(1.0));
    movavg_process_sample(&movavg, REAL_C(2.0));
    movavg_process_sample(&movavg, REAL_C(3.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), movavg_get_mean(&movavg));

    // The 1 falls off the end and the 4 comes in, thus the window is 2, 3, 4.
    movavg_process_sample(&movavg, REAL_C(4.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), movavg_get_mean(&movavg));

    movavg_free(&movavg);
}

void test_movavg_gives_the_same_answer_as_a_filter_of_equal_coefficients(void)
{
    // This is the filter that the module replaces. A finite impulse response
    // whose coefficients are all 1/length gives the mean of its window. The
    // two must agree once the window is full, and this module must reach that
    // answer in a fixed time where the other one reads every coefficient.
    const uint32_t length = 16u;
    movavg_t movavg = movavg_alloc(length);
    fir_t fir = fir_alloc(length);

    for(uint32_t index = 0; index < length; index++)
    {
        fir_set_coefficient(&fir, index, REAL_C(1.0) / (real_t)length);
    }

    for(uint32_t index = 0; index < 200u; index++)
    {
        real_t sample = REAL_SIN(REAL_C(0.15) * (real_t)index) + (REAL_C(0.5) * REAL_COS(REAL_C(0.02) * (real_t)index));

        real_t fast = movavg_process_sample(&movavg, sample);
        real_t slow = fir_process_sample(&fir, sample);

        if(index >= length)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), slow, fast);
        }
    }

    movavg_free(&movavg);
    fir_free(&fir);
}

void test_movavg_holds_its_accuracy_over_a_long_run(void)
{
    // A running total that is added to and taken away from for ever gathers a
    // small error at every step, and the error walks rather than cancels. The
    // module builds the totals again from the window from time to time to stop
    // that, and this test holds that the walk is bounded at either width.
    //
    // These samples sit at eight million, where a float loses its low digits,
    // and the run is fifty times longer than the refresh.
    movavg_t movavg = movavg_alloc(64);

    for(uint32_t index = 0; index < 200000u; index++)
    {
        movavg_process_sample(&movavg, REAL_C(8000000.0) + (real_t)(index % 3u));
    }

    // The last 64 samples run over the pattern 0, 1, 2 again and again, whose
    // mean is 1. Thus the mean of the window must be near 8000001.
    //
    // In 32 bits it comes out at 8000000.5, which is half a count out and does
    // not grow with the length of the run. Without the refresh it would grow
    // without end.
#if defined(FFITT_REAL_64)
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(8000001.0),
                            movavg_get_mean(&movavg));
#else
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.75), REAL_C(8000001.0),
                            movavg_get_mean(&movavg));
#endif

    movavg_free(&movavg);
}

void test_movavg_rms_follows_the_level_and_the_deviation_follows_the_movement(void)
{
    // A signal that sits at 100 and moves by 1. The two measures answer two
    // different questions, and mixing them up is the usual fault.
    movavg_t movavg = movavg_alloc(4);

    movavg_process_sample(&movavg, REAL_C(99.0));
    movavg_process_sample(&movavg, REAL_C(100.0));
    movavg_process_sample(&movavg, REAL_C(100.0));
    movavg_process_sample(&movavg, REAL_C(101.0));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(100.0), movavg_get_rms(&movavg));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.7071), movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_deviation_on_a_large_offset_shows_what_the_width_costs(void)
{
    // The deviation reads the whole window and takes the mean away first,
    // which is the careful way. On a level of eight million the digits still
    // run out in 32 bits, and this test records by how much.
    movavg_t movavg = movavg_alloc(5);

    for(uint32_t index = 0; index < 5u; index++)
    {
        movavg_process_sample(&movavg, REAL_C(8000000.0) + (real_t)index);
    }

    // The samples are 8000000 to 8000004, whose deviation is the root of 2,
    // which is 1.4142.
#if defined(FFITT_REAL_64)
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_SQRT(REAL_C(2.0)),
                            movavg_get_deviation(&movavg));
#else
    // In 32 bits the answer comes out as 1.5, which is out by six percent.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.5),
                            movavg_get_deviation(&movavg));
#endif

    movavg_free(&movavg);
}

void test_movavg_of_a_steady_signal_does_not_move(void)
{
    movavg_t movavg = movavg_alloc(8);

    for(uint32_t index = 0; index < 100u; index++)
    {
        movavg_process_sample(&movavg, REAL_C(5.0));
    }

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), movavg_get_mean(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), movavg_get_rms(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_process_block(void)
{
    movavg_t movavg = movavg_alloc(2);
    real_t input[4] = {REAL_C(2.0), REAL_C(4.0), REAL_C(6.0), REAL_C(8.0)};
    real_t output[4];

    movavg_process_block(&movavg, input, output, 4u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), output[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), output[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), output[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), output[3]);

    movavg_free(&movavg);
}

void test_movavg_process_block_can_write_over_its_input(void)
{
    movavg_t movavg = movavg_alloc(2);
    real_t signal[4] = {REAL_C(2.0), REAL_C(4.0), REAL_C(6.0), REAL_C(8.0)};

    movavg_process_block(&movavg, signal, signal, 4u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), signal[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), signal[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), signal[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), signal[3]);

    movavg_free(&movavg);
}

void test_movavg_reset(void)
{
    movavg_t movavg = movavg_alloc(4);

    movavg_process_sample(&movavg, REAL_C(10.0));
    movavg_process_sample(&movavg, REAL_C(20.0));
    movavg_reset(&movavg);

    TEST_ASSERT_EQUAL(0, movavg_count(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_mean(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_rms(&movavg));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), movavg_process_sample(&movavg, REAL_C(3.0)));

    movavg_free(&movavg);
}

void test_movavg_of_an_empty_window_is_nothing(void)
{
    movavg_t movavg = movavg_alloc(4);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_mean(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_rms(&movavg));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_counts_up_to_its_size(void)
{
    movavg_t movavg = movavg_alloc(3);

    movavg_process_sample(&movavg, REAL_C(1.0));
    TEST_ASSERT_EQUAL(1, movavg_count(&movavg));
    movavg_process_sample(&movavg, REAL_C(1.0));
    movavg_process_sample(&movavg, REAL_C(1.0));
    TEST_ASSERT_EQUAL(true, movavg_is_full(&movavg));
    movavg_process_sample(&movavg, REAL_C(1.0));
    TEST_ASSERT_EQUAL(3, movavg_count(&movavg));

    movavg_free(&movavg);
}
