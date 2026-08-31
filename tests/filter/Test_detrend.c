#include "unity.h"
#include "real_assert.h"
#include "detrend.h"
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_detrend_is_valid_kind(void)
{
    TEST_ASSERT_EQUAL(true, detrend_is_valid_kind(DETREND_CONSTANT));
    TEST_ASSERT_EQUAL(true, detrend_is_valid_kind(DETREND_LINEAR));
    TEST_ASSERT_EQUAL(false, detrend_is_valid_kind((detrend_kind_t)7));
}

void test_detrend_constant_takes_the_mean_away(void)
{
    real_t input[5] = {REAL_C(10.0), REAL_C(12.0), REAL_C(11.0), REAL_C(13.0),
                       REAL_C(14.0)};
    real_t output[5];

    TEST_ASSERT_EQUAL(true, detrend_block(input, output, 5u,
                                          DETREND_CONSTANT));

    // The mean is 12. What is left must sum to nothing.
    real_t total = REAL_C(0.0);
    for(uint32_t index = 0; index < 5u; index++)
    {
        total += output[index];
    }

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), total);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(2.0), output[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), output[4]);
}

void test_detrend_constant_leaves_the_shape_alone(void)
{
    // Only the level moves. The distance between any two samples must not.
    real_t input[4] = {REAL_C(100.0), REAL_C(103.0), REAL_C(101.0),
                       REAL_C(104.0)};
    real_t output[4];

    detrend_block(input, output, 4u, DETREND_CONSTANT);

    for(uint32_t index = 1; index < 4u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, input[index] - input[index - 1u],
                                output[index] - output[index - 1u]);
    }
}

void test_detrend_linear_empties_a_block_that_is_nothing_but_a_line(void)
{
    real_t input[16];
    real_t output[16];

    for(uint32_t index = 0; index < 16u; index++)
    {
        input[index] = REAL_C(5.0) + (REAL_C(0.25) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, detrend_block(input, output, 16u, DETREND_LINEAR));

    for(uint32_t index = 0; index < 16u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), output[index]);
    }
}

void test_detrend_linear_leaves_the_signal_and_takes_the_drift(void)
{
    // The whole point: a wave carried on a drift comes back as the wave.
    //
    // The wave is a cosine, which is even about the middle of the block, thus
    // the fit takes 3/(n+1) of it and no more. The next test holds what
    // happens to a wave that is not.
    real_t input[64];
    real_t output[64];
    real_t wave[64];
    const real_t taken = REAL_C(3.0) / REAL_C(65.0);

    for(uint32_t index = 0; index < 64u; index++)
    {
        wave[index] = REAL_COS(REAL_C(2.0) * REAL_PI * REAL_C(4.0)
                               * (real_t)index / REAL_C(64.0));
        input[index] = REAL_C(20.0) + (REAL_C(0.1) * (real_t)index)
                       + wave[index];
    }

    detrend_block(input, output, 64u, DETREND_LINEAR);

    // The level and the drift are gone, and the wave is there.
    for(uint32_t index = 0; index < 64u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(taken + REAL_C(0.001), wave[index],
                                output[index]);
    }
}

void test_how_much_of_a_wave_the_fit_takes_away(void)
{
    // THE TABLE IN THE HEADER, HELD TRUE, because it is the one thing about
    // this module that surprises people.
    //
    // A wave odd about the middle of the block loses about 3 divided by pi
    // times the number of periods it makes, and the length of the block does
    // not come into it. One period across the block loses nearly all of
    // itself, because one period across the block IS a drift as far as
    // anything looking at that block can tell.
    static real_t input[1024];
    static real_t output[1024];
    const uint32_t periods[3] = {1u, 4u, 32u};
    const real_t expected[3] = {REAL_C(0.955), REAL_C(0.239), REAL_C(0.030)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        for(uint32_t index = 0; index < 1024u; index++)
        {
            input[index] = REAL_SIN(REAL_C(2.0) * REAL_PI
                                    * (real_t)periods[which] * (real_t)index
                                    / REAL_C(1024.0));
        }

        detrend_block(input, output, 1024u, DETREND_LINEAR);

        real_t worst = REAL_C(0.0);
        for(uint32_t index = 0; index < 1024u; index++)
        {
            real_t gone = REAL_ABS(output[index] - input[index]);

            if(gone > worst) { worst = gone; }
        }

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.005), expected[which], worst);
    }
}

void test_detrend_trend_gives_the_drift_that_was_there(void)
{
    real_t input[100];
    real_t offset;
    real_t slope;

    for(uint32_t index = 0; index < 100u; index++)
    {
        input[index] = REAL_C(7.0) + (REAL_C(0.03) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, detrend_trend(input, 100u, DETREND_LINEAR, &offset,
                                          &slope));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.03), slope);

    // The offset is the value at the MIDDLE, which is the mean, and NOT the
    // value at the first sample.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                            REAL_C(7.0) + (REAL_C(0.03) * REAL_C(49.5)),
                            offset);
}

void test_detrend_trend_of_a_constant_has_no_slope(void)
{
    real_t input[8] = {REAL_C(3.0), REAL_C(1.0), REAL_C(4.0), REAL_C(1.0),
                       REAL_C(5.0), REAL_C(9.0), REAL_C(2.0), REAL_C(6.0)};
    real_t offset;
    real_t slope;

    TEST_ASSERT_EQUAL(true, detrend_trend(input, 8u, DETREND_CONSTANT, &offset,
                                          &slope));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), slope);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.875), offset);
}

void test_detrend_trend_at(void)
{
    real_t input[10];
    real_t offset;
    real_t slope;

    for(uint32_t index = 0; index < 10u; index++)
    {
        input[index] = REAL_C(2.0) + (REAL_C(0.5) * (real_t)index);
    }

    detrend_trend(input, 10u, DETREND_LINEAR, &offset, &slope);

    // Read at each sample, the trend must be the line that was put in.
    for(uint32_t index = 0; index < 10u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), input[index],
                                detrend_trend_at(offset, slope, 10u, index));
    }
}

void test_detrend_in_place(void)
{
    real_t block[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        block[index] = REAL_C(50.0) + (REAL_C(2.0) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, detrend_block(block, block, 8u, DETREND_LINEAR));

    for(uint32_t index = 0; index < 8u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), block[index]);
    }
}

void test_detrend_remove_uses_a_trend_found_from_another_block(void)
{
    // Why detrend_remove exists. The first block is quiet and gives the drift
    // of the sensor. The second block holds a signal that rises across it, and
    // finding the trend afresh there would take the signal away with it.
    real_t quiet[32];
    real_t loud[32];
    real_t output[32];
    real_t offset;
    real_t slope;

    for(uint32_t index = 0; index < 32u; index++)
    {
        quiet[index] = REAL_C(4.0) + (REAL_C(0.1) * (real_t)index);
        // The same drift, and a real rise on top of it that must be kept.
        loud[index] = quiet[index] + (REAL_C(0.5) * (real_t)index);
    }

    detrend_trend(quiet, 32u, DETREND_LINEAR, &offset, &slope);

    TEST_ASSERT_EQUAL(true, detrend_remove(loud, output, 32u, offset, slope));

    // What is left is the rise that belongs to the signal, whole. Only the
    // trend of the quiet block went, thus the rise keeps the place it had.
    for(uint32_t index = 0; index < 32u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.5) * (real_t)index,
                                output[index]);
    }

    // Taking the trend of the loud block instead would have taken the rise
    // away as well, which is the mistake this function is here to avoid.
    detrend_block(loud, output, 32u, DETREND_LINEAR);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), output[31]);
}

void test_detrend_refuses_a_block_too_short_to_have_a_direction(void)
{
    real_t one[1] = {REAL_C(5.0)};
    real_t output[1];
    real_t offset;
    real_t slope;

    // One sample has a level but no direction.
    TEST_ASSERT_EQUAL(true, detrend_block(one, output, 1u, DETREND_CONSTANT));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), output[0]);

    TEST_ASSERT_EQUAL(false, detrend_block(one, output, 1u, DETREND_LINEAR));
    TEST_ASSERT_EQUAL(false, detrend_trend(one, 1u, DETREND_LINEAR, &offset,
                                           &slope));

    TEST_ASSERT_EQUAL(false, detrend_block(one, output, 0u, DETREND_CONSTANT));
}

void test_detrend_refuses_a_kind_it_does_not_know(void)
{
    real_t input[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};
    real_t output[4];
    real_t offset;
    real_t slope;

    TEST_ASSERT_EQUAL(false, detrend_block(input, output, 4u,
                                           (detrend_kind_t)9));
    TEST_ASSERT_EQUAL(false, detrend_trend(input, 4u, (detrend_kind_t)9,
                                           &offset, &slope));
}

void test_numbering_from_the_middle_holds_its_digits(void)
{
    // The table in the header, held true at the level where it shows.
    //
    // A block of a rising level, numbered from the middle. The same fit
    // written from the start of the block loses several times as much, and at
    // 32 bits that is the difference between a usable slope and a guess.
    const uint32_t size = 1000u;
    static real_t input[1000];
    real_t offset;
    real_t slope;

    for(uint32_t index = 0; index < size; index++)
    {
        input[index] = REAL_C(1000.0) + (REAL_C(0.01) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, detrend_trend(input, size, DETREND_LINEAR, &offset,
                                          &slope));

    // A thousandth of the slope, on a block whose level is a hundred thousand
    // times the rise of one sample.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.00001), REAL_C(0.01), slope);
}

void test_removing_a_trend_from_a_signal_of_no_samples_is_refused(void)
{
    real_t nothing[1] = {REAL_C(0.0)};
    real_t room[1];

    TEST_ASSERT_FALSE(detrend_remove(nothing, room, 0u, REAL_C(0.0),
                                     REAL_C(0.0)));
}
