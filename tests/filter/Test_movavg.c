#include "unity.h"
#include "movavg.h"
#include "fir.h"
#include "ringbuf.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.0001f

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
    float data[4];
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

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, movavg_process_sample(&movavg, 2.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, movavg_process_sample(&movavg, 4.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, movavg_process_sample(&movavg, 6.0f));

    movavg_free(&movavg);
}

void test_movavg_holds_the_mean_of_the_last_samples(void)
{
    movavg_t movavg = movavg_alloc(3);

    movavg_process_sample(&movavg, 1.0f);
    movavg_process_sample(&movavg, 2.0f);
    movavg_process_sample(&movavg, 3.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, movavg_get_mean(&movavg));

    // The 1 falls off the end and the 4 comes in, thus the window is 2, 3, 4.
    movavg_process_sample(&movavg, 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, movavg_get_mean(&movavg));

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
        fir_set_coefficient(&fir, index, 1.0f / (float)length);
    }

    for(uint32_t index = 0; index < 200u; index++)
    {
        float sample = sinf(0.15f * (float)index) + (0.5f * cosf(0.02f * (float)index));

        float fast = movavg_process_sample(&movavg, sample);
        float slow = fir_process_sample(&fir, sample);

        if(index >= length)
        {
            TEST_ASSERT_FLOAT_WITHIN(0.001f, slow, fast);
        }
    }

    movavg_free(&movavg);
    fir_free(&fir);
}

void test_movavg_holds_its_accuracy_over_a_long_run(void)
{
    // A running total that is added to and taken away from for ever gathers a
    // small error at every step, and the error walks. The module builds the
    // totals again from the window from time to time to stop that.
    //
    // These samples sit at eight million, which is where a float loses its low
    // digits, and the run is far longer than the refresh.
    movavg_t movavg = movavg_alloc(64);

    for(uint32_t index = 0; index < 200000u; index++)
    {
        float sample = 8000000.0f + (float)(index % 3u);
        movavg_process_sample(&movavg, sample);
    }

    // The last 64 samples run over the pattern 0, 1, 2 again and again, whose
    // mean is 1. Thus the mean of the window must be near 8000001.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 8000001.0f, movavg_get_mean(&movavg));

    movavg_free(&movavg);
}

void test_movavg_rms_follows_the_level_and_the_deviation_follows_the_movement(void)
{
    // A signal that sits at 100 and moves by 1. The two measures answer two
    // different questions, and mixing them up is the usual fault.
    movavg_t movavg = movavg_alloc(4);

    movavg_process_sample(&movavg, 99.0f);
    movavg_process_sample(&movavg, 100.0f);
    movavg_process_sample(&movavg, 100.0f);
    movavg_process_sample(&movavg, 101.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, movavg_get_rms(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.7071f, movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_deviation_holds_up_on_a_large_offset(void)
{
    // The deviation reads the whole window and takes the mean away first. The
    // shorter way, which is the mean of the squares less the square of the
    // mean, would lose this answer completely.
    movavg_t movavg = movavg_alloc(5);

    for(uint32_t index = 0; index < 5u; index++)
    {
        movavg_process_sample(&movavg, 8000000.0f + (float)index);
    }

    // The samples are 8000000 to 8000004, whose deviation is the root of 2.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, sqrtf(2.0f), movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_of_a_steady_signal_does_not_move(void)
{
    movavg_t movavg = movavg_alloc(8);

    for(uint32_t index = 0; index < 100u; index++)
    {
        movavg_process_sample(&movavg, 5.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, movavg_get_mean(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, movavg_get_rms(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_process_block(void)
{
    movavg_t movavg = movavg_alloc(2);
    float input[4] = {2.0f, 4.0f, 6.0f, 8.0f};
    float output[4];

    movavg_process_block(&movavg, input, output, 4u);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 7.0f, output[3]);

    movavg_free(&movavg);
}

void test_movavg_process_block_can_write_over_its_input(void)
{
    movavg_t movavg = movavg_alloc(2);
    float signal[4] = {2.0f, 4.0f, 6.0f, 8.0f};

    movavg_process_block(&movavg, signal, signal, 4u);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, signal[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, signal[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, signal[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 7.0f, signal[3]);

    movavg_free(&movavg);
}

void test_movavg_reset(void)
{
    movavg_t movavg = movavg_alloc(4);

    movavg_process_sample(&movavg, 10.0f);
    movavg_process_sample(&movavg, 20.0f);
    movavg_reset(&movavg);

    TEST_ASSERT_EQUAL(0, movavg_count(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_mean(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_rms(&movavg));

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, movavg_process_sample(&movavg, 3.0f));

    movavg_free(&movavg);
}

void test_movavg_of_an_empty_window_is_nothing(void)
{
    movavg_t movavg = movavg_alloc(4);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_mean(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_rms(&movavg));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, movavg_get_deviation(&movavg));

    movavg_free(&movavg);
}

void test_movavg_counts_up_to_its_size(void)
{
    movavg_t movavg = movavg_alloc(3);

    movavg_process_sample(&movavg, 1.0f);
    TEST_ASSERT_EQUAL(1, movavg_count(&movavg));
    movavg_process_sample(&movavg, 1.0f);
    movavg_process_sample(&movavg, 1.0f);
    TEST_ASSERT_EQUAL(true, movavg_is_full(&movavg));
    movavg_process_sample(&movavg, 1.0f);
    TEST_ASSERT_EQUAL(3, movavg_count(&movavg));

    movavg_free(&movavg);
}
