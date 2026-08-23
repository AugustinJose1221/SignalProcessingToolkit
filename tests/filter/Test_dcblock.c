#include "unity.h"
#include "real_assert.h"
#include "dcblock.h"
#include "iir.h"
#include "iir.h"
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

void test_dcblock_is_valid_cutoff(void)
{
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(REAL_C(0.1)));
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(DCBLOCK_MIN_CUTOFF));

    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(REAL_C(0.5)));
}

void test_the_limit_of_the_tracker_follows_the_width_of_the_build(void)
{
    // It holds a cutoff a thousand times lower than a section does, at either
    // width, because one pole has no cancelling sums in it.
    TEST_ASSERT_TRUE(DCBLOCK_MIN_CUTOFF < (IIR_MIN_CUTOFF / REAL_C(100.0)));

#if defined(SPTK_REAL_64)
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(REAL_C(0.000000001)));
    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(REAL_C(0.0000000001)));
#else
    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(REAL_C(0.000001)));
    TEST_ASSERT_EQUAL(false, dcblock_is_valid_cutoff(REAL_C(0.0000001)));
#endif
}

void test_the_tracker_reaches_the_level_at_its_lowest_cutoff(void)
{
    // The limit is set where the tracker still reaches the level it follows.
    // Below it the step becomes smaller than one step of the number itself and
    // the level stops short.
    //
    // The cutoff here is the lowest one the build allows, held to a value that
    // settles in a reasonable number of samples.
    real_t cutoff = REAL_C(0.00001);
    dcblock_t dcblock = dcblock_init(cutoff);

    TEST_ASSERT_EQUAL(true, dcblock_is_valid_cutoff(cutoff));

    dcblock_set_level(&dcblock, REAL_C(0.0));
    for(uint32_t index = 0; index < 2000000u; index++)
    {
        dcblock_process_sample(&dcblock, REAL_C(8300000.0));
    }

    // Within a part in a thousand of where it belongs.
    TEST_ASSERT_REAL_WITHIN(REAL_C(8300.0), REAL_C(8300000.0),
                            dcblock_get_level(&dcblock));
}

void test_dcblock_primes_itself_on_the_first_sample(void)
{
    // A filter that starts from zero would answer the first sample as a step
    // of eight million counts. This one must give nothing instead.
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                             dcblock_process_sample(&dcblock, REAL_C(8300000.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0), REAL_C(8300000.0), dcblock_get_level(&dcblock));
}

void test_dcblock_takes_a_steady_level_away(void)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.01));

    for(uint32_t index = 0; index < 1000u; index++)
    {
        real_t result = dcblock_process_sample(&dcblock, REAL_C(8300000.0));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), REAL_C(0.0), result);
    }
}

void test_dcblock_keeps_a_signal_that_moves_faster_than_the_cutoff(void)
{
    // A wave far above the cutoff must come through with its size kept.
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));
    real_t largest = REAL_C(0.0);

    for(uint32_t index = 0; index < 4000u; index++)
    {
        real_t wave = REAL_C(1000.0) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.05) * (real_t)index);
        real_t result = dcblock_process_sample(&dcblock, REAL_C(8300000.0) + wave);

        if((index > 1000u) && (REAL_ABS(result) > largest))
        {
            largest = REAL_ABS(result);
        }
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(20.0), REAL_C(1000.0), largest);
}

void test_dcblock_does_not_care_how_large_the_level_is(void)
{
    // This is the whole reason the module exists. The answer must be the same
    // whether the wave sits at nothing or at eight million.
    real_t level[4] = {REAL_C(0.0), REAL_C(1000.0), REAL_C(100000.0), REAL_C(8300000.0)};
    real_t error[4];

    for(uint32_t which = 0; which < 4u; which++)
    {
        dcblock_t dcblock = dcblock_init(REAL_C(0.001));
        real_t total = 0.0;
        uint32_t counted = 0;

        for(uint32_t index = 0; index < 20000u; index++)
        {
            real_t wanted = REAL_C(1000.0) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02) * (real_t)index);
            real_t result = dcblock_process_sample(&dcblock, level[which] + wanted);

            if(index > 5000u)
            {
                total += REAL_ABS((real_t)result - (real_t)wanted);
                counted++;
            }
        }
        error[which] = (real_t)(total / (real_t)counted);
    }

    // The error at a level of eight million must be no worse than the error at
    // a level of nothing.
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0), error[0], error[3]);
}

// Give the mean error of a filter against the wave it should have kept, for a
// wave carried on the given level.
//
// Every filter has an error of its own that comes from its shape, and that
// error is there at a level of nothing. What matters here is what the LEVEL
// adds on top of that, thus each measurement is made twice and the difference
// is the answer.
static real_t error_of_iir(real_t level)
{
    iir_t iir = iir_alloc(1);
    iir_design_high_pass(&iir, REAL_C(0.001));

    real_t total = 0.0;
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        real_t wanted = REAL_C(1000.0) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02) * (real_t)index);
        real_t result = iir_process_sample(&iir, level + wanted);

        if(index > 10000u)
        {
            total += REAL_ABS((real_t)result - (real_t)wanted);
            counted++;
        }
    }
    iir_free(&iir);

    return (real_t)(total / (real_t)counted);
}

static real_t error_of_dcblock(real_t level)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));

    real_t total = 0.0;
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        real_t wanted = REAL_C(1000.0) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02) * (real_t)index);
        real_t result = dcblock_process_sample(&dcblock, level + wanted);

        if(index > 10000u)
        {
            total += REAL_ABS((real_t)result - (real_t)wanted);
            counted++;
        }
    }

    return (real_t)(total / (real_t)counted);
}

void test_dcblock_stands_against_a_large_level_better_than_a_section_does(void)
{
    // The measurement that the header sets out. A wave of 1000 counts is
    // carried first on nothing and then on eight million. The answer should
    // not depend on the level at all, and what each filter ADDS when the level
    // rises is the measure of how well it holds up.
    real_t iir_added = error_of_iir(REAL_C(8300000.0)) - error_of_iir(REAL_C(0.0));
    real_t dcblock_added = error_of_dcblock(REAL_C(8300000.0))
                           - error_of_dcblock(REAL_C(0.0));

    // This module gains almost nothing at either width, because it is one pole
    // and has no two nearly equal numbers to subtract.
    TEST_ASSERT_TRUE(dcblock_added < REAL_C(1.0));

#if defined(SPTK_REAL_64)
    // At 64 bits the section has digits to spare, thus it gains nothing
    // either. The two are then alike in accuracy and differ only in shape.
    TEST_ASSERT_TRUE(iir_added < REAL_C(1.0));
#else
    // At 32 bits the section gains about 99 counts against a wave of 1000,
    // which is a tenth of the answer. This module gains about 0.1, thus it is
    // some eight hundred times better. THAT GAP IS WHY THIS MODULE EXISTS at
    // the default width.
    TEST_ASSERT_TRUE(iir_added > REAL_C(50.0));
    TEST_ASSERT_TRUE(dcblock_added < (iir_added / REAL_C(100.0)));
#endif
}

void test_dcblock_holds_a_cutoff_far_below_what_a_section_can(void)
{
    // IIR_MIN_CUTOFF is 0.001. This is a hundred times lower, and the level
    // must still be taken away completely.
    dcblock_t dcblock = dcblock_init(REAL_C(0.00001));

    for(uint32_t index = 0; index < 4000000u; index++)
    {
        dcblock_process_sample(&dcblock, REAL_C(8300000.0));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), REAL_C(0.0),
                             dcblock_process_sample(&dcblock, REAL_C(8300000.0)));
}

void test_dcblock_of_a_cutoff_it_cannot_hold_passes_the_signal_through(void)
{
    // A tracker that cannot hold its cutoff must follow nothing rather than
    // follow at a rate that was not asked for.
    dcblock_t dcblock = dcblock_init(REAL_C(0.0000001));

    // The first sample still sets the level, thus it gives nothing.
    dcblock_process_sample(&dcblock, REAL_C(100.0));
    // From then on the level never moves, thus the difference is the signal.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(50.0),
                             dcblock_process_sample(&dcblock, REAL_C(150.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(100.0), dcblock_get_level(&dcblock));
}

void test_dcblock_set_level(void)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));

    dcblock_set_level(&dcblock, REAL_C(8300000.0));

    // The tracker is settled at once, thus the very first sample gives the
    // signal and not a step. The level follows a little way towards the sample
    // in that one step, thus the answer is a little under 500.
    TEST_ASSERT_REAL_WITHIN(REAL_C(5.0), REAL_C(497.0),
                             dcblock_process_sample(&dcblock, REAL_C(8300500.0)));
}

void test_dcblock_get_level_gives_the_slow_part_of_the_signal(void)
{
    // The level is worth reading on its own: it holds the drift.
    dcblock_t dcblock = dcblock_init(REAL_C(0.01));

    for(uint32_t index = 0; index < 2000u; index++)
    {
        dcblock_process_sample(&dcblock, REAL_C(100.0));
    }
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0), REAL_C(100.0), dcblock_get_level(&dcblock));

    for(uint32_t index = 0; index < 2000u; index++)
    {
        dcblock_process_sample(&dcblock, REAL_C(200.0));
    }
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0), REAL_C(200.0), dcblock_get_level(&dcblock));
}

void test_dcblock_reset(void)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));

    dcblock_process_sample(&dcblock, REAL_C(100.0));
    dcblock_reset(&dcblock);

    // The next sample sets the level again, as the first one did.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                             dcblock_process_sample(&dcblock, REAL_C(9000.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0), REAL_C(9000.0), dcblock_get_level(&dcblock));
}

void test_dcblock_process_block_can_write_over_its_input(void)
{
    dcblock_t dcblock = dcblock_init(REAL_C(0.001));
    real_t signal[4] = {REAL_C(1000.0), REAL_C(1010.0), REAL_C(1020.0), REAL_C(1030.0)};

    dcblock_process_block(&dcblock, signal, signal, 4u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), signal[0]);
    TEST_ASSERT_TRUE(signal[3] > REAL_C(25.0));
    TEST_ASSERT_TRUE(signal[3] < REAL_C(31.0));
}
