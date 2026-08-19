#include "unity.h"
#include "dcblock.h"
#include "iir.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.0001f
#define PI          3.14159265358979323846f

void setUp(void)
{

}

void tearDown(void)
{

}

void test_dcblock_is_valid_cutoff(void)
{
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(0.1f));
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(DCBLOCK_MIN_CUTOFF));

    // A thousand times lower than a section in single precision can hold.
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(0.00001f));

    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(0.0f));
    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(0.0000001f));
    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(0.5f));
}

void test_dcblock_primes_itself_on_the_first_sample(void)
{
    // A filter that starts from zero would answer the first sample as a step
    // of eight million counts. This one must give nothing instead.
    dcblock_t dcblock = dcblock_init(0.001f);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f,
                             dcblock_process_sample(&dcblock, 8300000.0f));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 8300000.0f, dcblock_get_level(&dcblock));
}

void test_dcblock_takes_a_steady_level_away(void)
{
    dcblock_t dcblock = dcblock_init(0.01f);

    for(uint32_t index = 0; index < 1000u; index++)
    {
        float result = dcblock_process_sample(&dcblock, 8300000.0f);
        TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, result);
    }
}

void test_dcblock_keeps_a_signal_that_moves_faster_than_the_cutoff(void)
{
    // A wave far above the cutoff must come through with its size kept.
    dcblock_t dcblock = dcblock_init(0.001f);
    float largest = 0.0f;

    for(uint32_t index = 0; index < 4000u; index++)
    {
        float wave = 1000.0f * sinf(2.0f * PI * 0.05f * (float)index);
        float result = dcblock_process_sample(&dcblock, 8300000.0f + wave);

        if((index > 1000u) && (fabsf(result) > largest))
        {
            largest = fabsf(result);
        }
    }

    TEST_ASSERT_FLOAT_WITHIN(20.0f, 1000.0f, largest);
}

void test_dcblock_does_not_care_how_large_the_level_is(void)
{
    // This is the whole reason the module exists. The answer must be the same
    // whether the wave sits at nothing or at eight million.
    float level[4] = {0.0f, 1000.0f, 100000.0f, 8300000.0f};
    float error[4];

    for(uint32_t which = 0; which < 4u; which++)
    {
        dcblock_t dcblock = dcblock_init(0.001f);
        double total = 0.0;
        uint32_t counted = 0;

        for(uint32_t index = 0; index < 20000u; index++)
        {
            float wanted = 1000.0f * sinf(2.0f * PI * 0.02f * (float)index);
            float result = dcblock_process_sample(&dcblock, level[which] + wanted);

            if(index > 5000u)
            {
                total += fabs((double)result - (double)wanted);
                counted++;
            }
        }
        error[which] = (float)(total / (double)counted);
    }

    // The error at a level of eight million must be no worse than the error at
    // a level of nothing.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, error[0], error[3]);
}

// Give the mean error of a filter against the wave it should have kept, for a
// wave carried on the given level.
//
// Every filter has an error of its own that comes from its shape, and that
// error is there at a level of nothing. What matters here is what the LEVEL
// adds on top of that, thus each measurement is made twice and the difference
// is the answer.
static float error_of_iir(float level)
{
    iir_t iir = iir_alloc(1);
    iir_design_high_pass(&iir, 0.001f);

    double total = 0.0;
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        float wanted = 1000.0f * sinf(2.0f * PI * 0.02f * (float)index);
        float result = iir_process_sample(&iir, level + wanted);

        if(index > 10000u)
        {
            total += fabs((double)result - (double)wanted);
            counted++;
        }
    }
    iir_free(&iir);

    return (float)(total / (double)counted);
}

static float error_of_dcblock(float level)
{
    dcblock_t dcblock = dcblock_init(0.001f);

    double total = 0.0;
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        float wanted = 1000.0f * sinf(2.0f * PI * 0.02f * (float)index);
        float result = dcblock_process_sample(&dcblock, level + wanted);

        if(index > 10000u)
        {
            total += fabs((double)result - (double)wanted);
            counted++;
        }
    }

    return (float)(total / (double)counted);
}

void test_dcblock_holds_up_where_a_section_in_single_precision_does_not(void)
{
    // The measurement that the header sets out, and the reason the module
    // exists. A wave of 1000 counts is carried first on nothing and then on
    // eight million. The answer should not depend on the level at all.
    float iir_added = error_of_iir(8300000.0f) - error_of_iir(0.0f);
    float dcblock_added = error_of_dcblock(8300000.0f) - error_of_dcblock(0.0f);

    // The section gains a false signal near a tenth of the wave.
    TEST_ASSERT_TRUE(iir_added > 50.0f);

    // This module gains nothing that can be measured.
    TEST_ASSERT_TRUE(dcblock_added < 1.0f);
}

void test_dcblock_holds_a_cutoff_far_below_what_a_section_can(void)
{
    // IIR_MIN_CUTOFF is 0.001. This is a hundred times lower, and the level
    // must still be taken away completely.
    dcblock_t dcblock = dcblock_init(0.00001f);

    for(uint32_t index = 0; index < 4000000u; index++)
    {
        dcblock_process_sample(&dcblock, 8300000.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f,
                             dcblock_process_sample(&dcblock, 8300000.0f));
}

void test_dcblock_of_a_cutoff_it_cannot_hold_passes_the_signal_through(void)
{
    // A tracker that cannot hold its cutoff must follow nothing rather than
    // follow at a rate that was not asked for.
    dcblock_t dcblock = dcblock_init(0.0000001f);

    // The first sample still sets the level, thus it gives nothing.
    dcblock_process_sample(&dcblock, 100.0f);
    // From then on the level never moves, thus the difference is the signal.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 50.0f,
                             dcblock_process_sample(&dcblock, 150.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 100.0f, dcblock_get_level(&dcblock));
}

void test_dcblock_set_level(void)
{
    dcblock_t dcblock = dcblock_init(0.001f);

    dcblock_set_level(&dcblock, 8300000.0f);

    // The tracker is settled at once, thus the very first sample gives the
    // signal and not a step. The level follows a little way towards the sample
    // in that one step, thus the answer is a little under 500.
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 497.0f,
                             dcblock_process_sample(&dcblock, 8300500.0f));
}

void test_dcblock_get_level_gives_the_slow_part_of_the_signal(void)
{
    // The level is worth reading on its own: it holds the drift.
    dcblock_t dcblock = dcblock_init(0.01f);

    for(uint32_t index = 0; index < 2000u; index++)
    {
        dcblock_process_sample(&dcblock, 100.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, dcblock_get_level(&dcblock));

    for(uint32_t index = 0; index < 2000u; index++)
    {
        dcblock_process_sample(&dcblock, 200.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 200.0f, dcblock_get_level(&dcblock));
}

void test_dcblock_reset(void)
{
    dcblock_t dcblock = dcblock_init(0.001f);

    dcblock_process_sample(&dcblock, 100.0f);
    dcblock_reset(&dcblock);

    // The next sample sets the level again, as the first one did.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f,
                             dcblock_process_sample(&dcblock, 9000.0f));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 9000.0f, dcblock_get_level(&dcblock));
}

void test_dcblock_process_block_can_write_over_its_input(void)
{
    dcblock_t dcblock = dcblock_init(0.001f);
    float signal[4] = {1000.0f, 1010.0f, 1020.0f, 1030.0f};

    dcblock_process_block(&dcblock, signal, signal, 4u);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, signal[0]);
    TEST_ASSERT_TRUE(signal[3] > 25.0f);
    TEST_ASSERT_TRUE(signal[3] < 31.0f);
}
