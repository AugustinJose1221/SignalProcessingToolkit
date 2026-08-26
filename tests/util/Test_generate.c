#include "unity.h"
#include "real_assert.h"
#include "generate.h"
#include <math.h>

#define RATE        REAL_C(8000.0)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_generate_is_valid_kind(void)
{
    TEST_ASSERT_EQUAL(true, generate_is_valid_kind(GENERATE_SINE));
    TEST_ASSERT_EQUAL(true, generate_is_valid_kind(GENERATE_PINK_NOISE));
    TEST_ASSERT_EQUAL(false, generate_is_valid_kind((generate_kind_t)9));
}

void test_generate_is_valid_frequency(void)
{
    TEST_ASSERT_EQUAL(true, generate_is_valid_frequency(REAL_C(1000.0),
                                                        RATE));

    // At or above half the sample rate a frequency cannot be told from a
    // lower one, thus asking for it gives an answer about a frequency nobody
    // wanted.
    TEST_ASSERT_EQUAL(false, generate_is_valid_frequency(REAL_C(4000.0),
                                                         RATE));
    TEST_ASSERT_EQUAL(false, generate_is_valid_frequency(REAL_C(5000.0),
                                                         RATE));
    TEST_ASSERT_EQUAL(false, generate_is_valid_frequency(REAL_C(0.0), RATE));
    TEST_ASSERT_EQUAL(false, generate_is_valid_frequency(REAL_C(100.0),
                                                         REAL_C(0.0)));
}

void test_a_sine_is_a_sine(void)
{
    generate_t generate = generate_make(GENERATE_SINE);

    TEST_ASSERT_EQUAL(true, generate_design(&generate, REAL_C(1000.0), RATE));

    // Eight samples to the turn, thus the values are known exactly.
    const real_t wanted[8] = {REAL_C(0.0), REAL_C(0.70711), REAL_C(1.0),
                              REAL_C(0.70711), REAL_C(0.0), -REAL_C(0.70711),
                              -REAL_C(1.0), -REAL_C(0.70711)};

    for(uint32_t index = 0; index < 8u; index++)
    {
        real_t made = generate_sample(&generate);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), wanted[index], made);
    }
}

void test_a_square_sits_at_the_middle_of_its_own_step(void)
{
    // THE WHOLE OF THE BAND-LIMITING, SEEN IN ONE PLACE. A sample landing
    // exactly on the corner cannot be at either value: the step happened
    // between two samples and the only honest answer is the middle of it.
    generate_t generate = generate_make(GENERATE_SQUARE);

    generate_design(&generate, REAL_C(1000.0), RATE);

    const real_t wanted[8] = {REAL_C(0.0), REAL_C(1.0), REAL_C(1.0),
                              REAL_C(1.0), REAL_C(0.0), -REAL_C(1.0),
                              -REAL_C(1.0), -REAL_C(1.0)};

    for(uint32_t index = 0; index < 8u; index++)
    {
        real_t made = generate_sample(&generate);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), wanted[index], made);
    }
}

void test_a_sawtooth_rises_across_the_turn(void)
{
    generate_t generate = generate_make(GENERATE_SAWTOOTH);

    generate_design(&generate, REAL_C(1000.0), RATE);

    real_t held[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        held[index] = generate_sample(&generate);
    }

    // Past the corner it climbs steadily, by a quarter each sample.
    for(uint32_t index = 2; index < 8u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.25),
                                held[index] - held[index - 1u]);
    }
}

void test_a_triangle_has_no_step_at_all(void)
{
    // A triangle changes its slope but never jumps, thus it needs no
    // smoothing and every step between samples is the same size.
    generate_t generate = generate_make(GENERATE_TRIANGLE);

    generate_design(&generate, REAL_C(1000.0), RATE);

    real_t held[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        held[index] = generate_sample(&generate);
    }

    for(uint32_t index = 1; index < 8u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.5),
                                REAL_ABS(held[index] - held[index - 1u]));
    }
}

void test_every_wave_stays_inside_one_and_minus_one(void)
{
    const generate_kind_t kinds[4] = {GENERATE_SINE, GENERATE_SQUARE,
                                      GENERATE_SAWTOOTH, GENERATE_TRIANGLE};

    for(uint32_t which = 0; which < 4u; which++)
    {
        generate_t generate = generate_make(kinds[which]);

        // An awkward frequency, so that the samples land all over the turn.
        generate_design(&generate, REAL_C(997.0), RATE);

        for(uint32_t index = 0; index < 4000u; index++)
        {
            real_t value = generate_sample(&generate);

            TEST_ASSERT_TRUE(value <= REAL_C(1.001));
            TEST_ASSERT_TRUE(value >= -REAL_C(1.001));
        }
    }
}

void test_the_phase_is_carried_and_never_grows(void)
{
    // Working the angle out from the sample number loses its digits over a
    // long run, as the bluestein module records. Carrying and folding it does
    // not, and this holds the wave to that across a million samples.
    generate_t generate = generate_make(GENERATE_SINE);

    generate_design(&generate, REAL_C(1000.0), RATE);

    for(uint32_t index = 0; index < 1000000u; index++)
    {
        generate_sample(&generate);

        real_t phase = generate_get_phase(&generate);

        TEST_ASSERT_TRUE(phase >= REAL_C(0.0));
        TEST_ASSERT_TRUE(phase < REAL_C(1.0));
    }

    // A million samples at eight to the turn is a whole number of turns, thus
    // the wave must stand exactly where it started.
    real_t back_at_the_start = generate_sample(&generate);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), back_at_the_start);
}

void test_a_sweep_arrives_where_it_was_sent(void)
{
    // A chirp is the most useful test signal there is, and one that does not
    // arrive at the frequency it was sent to is not a chirp.
    generate_t generate = generate_make(GENERATE_SINE);

    TEST_ASSERT_EQUAL(true, generate_design_sweep(&generate, REAL_C(100.0),
                                                  REAL_C(2000.0), RATE,
                                                  1000u));

    for(uint32_t index = 0; index < 1000u; index++)
    {
        generate_sample(&generate);
    }

    // Count the turns across the next hundred samples to see the frequency it
    // arrived at.
    real_t before = generate_get_phase(&generate);

    generate_sample(&generate);

    real_t after = generate_get_phase(&generate);
    real_t moved = after - before;

    if(moved < REAL_C(0.0))
    {
        moved += REAL_C(1.0);
    }

    // 2000 Hz at 8000 is a quarter of a turn each sample.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.25), moved);
}

void test_the_same_seed_gives_the_same_noise(void)
{
    // A TEST THAT CANNOT BE REPEATED IS NOT A TEST.
    generate_t first = generate_make(GENERATE_WHITE_NOISE);
    generate_t second = generate_make(GENERATE_WHITE_NOISE);

    generate_design(&first, REAL_C(0.0), RATE);
    generate_design(&second, REAL_C(0.0), RATE);

    generate_set_seed(&first, 12345u);
    generate_set_seed(&second, 12345u);

    // THE CALLS STAND OUTSIDE THE ASSERTION AND NOT INSIDE IT. The macro uses
    // what it is given more than once, thus a call that moves something on
    // would be made more than once and the two makers would fall out of step
    // with each other rather than with the test.
    for(uint32_t index = 0; index < 1000u; index++)
    {
        real_t from_first = generate_sample(&first);
        real_t from_second = generate_sample(&second);

        TEST_ASSERT_EQUAL_REAL(from_first, from_second);
    }

    // And a different seed gives different values.
    generate_set_seed(&second, 999u);

    uint32_t apart = 0u;

    for(uint32_t index = 0; index < 100u; index++)
    {
        real_t from_first = generate_sample(&first);
        real_t from_second = generate_sample(&second);

        if(from_first != from_second)
        {
            apart++;
        }
    }

    TEST_ASSERT_TRUE(apart > 90u);
}

void test_white_noise_spreads_evenly_about_nothing(void)
{
    generate_t generate = generate_make(GENERATE_WHITE_NOISE);

    generate_design(&generate, REAL_C(0.0), RATE);
    generate_set_seed(&generate, 7u);

    real_t total = REAL_C(0.0);
    real_t power = REAL_C(0.0);
    real_t largest = REAL_C(0.0);

    for(uint32_t index = 0; index < 20000u; index++)
    {
        real_t value = generate_sample(&generate);

        total += value;
        power += value * value;

        if(REAL_ABS(value) > largest) { largest = REAL_ABS(value); }

        TEST_ASSERT_TRUE(value <= REAL_C(1.0));
        TEST_ASSERT_TRUE(value >= -REAL_C(1.0));
    }

    // It sits about nothing.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), REAL_C(0.0),
                            total / REAL_C(20000.0));

    // Spread evenly from -1 to 1, the mean of the squares is a third.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0) / REAL_C(3.0),
                            power / REAL_C(20000.0));

    // And it really reaches most of the way to both ends.
    TEST_ASSERT_TRUE(largest > REAL_C(0.99));
}

void test_pink_noise_holds_more_of_its_power_low_down(void)
{
    // What parts pink noise from white: the low frequencies hold more.
    generate_t pink = generate_make(GENERATE_PINK_NOISE);

    generate_design(&pink, REAL_C(0.0), RATE);
    generate_set_seed(&pink, 3u);

    // How much it wanders slowly is measured by how alike two samples in a
    // row are. White noise has nothing in common from one sample to the next;
    // pink noise has a great deal.
    real_t previous = generate_sample(&pink);
    real_t together = REAL_C(0.0);
    real_t power = REAL_C(0.0);

    for(uint32_t index = 0; index < 20000u; index++)
    {
        real_t value = generate_sample(&pink);

        together += previous * value;
        power += value * value;
        previous = value;
    }

    TEST_ASSERT_TRUE(power > REAL_C(0.0));

    // Well over half of its power is shared with the sample before it.
    TEST_ASSERT_TRUE((together / power) > REAL_C(0.5));
}

void test_generate_block_gives_the_same_as_one_at_a_time(void)
{
    generate_t one = generate_make(GENERATE_SAWTOOTH);
    generate_t many = generate_make(GENERATE_SAWTOOTH);

    generate_design(&one, REAL_C(997.0), RATE);
    generate_design(&many, REAL_C(997.0), RATE);

    real_t block[64];

    TEST_ASSERT_EQUAL(true, generate_block(&many, block, 64u));

    for(uint32_t index = 0; index < 64u; index++)
    {
        real_t one_at_a_time = generate_sample(&one);

        TEST_ASSERT_EQUAL_REAL(one_at_a_time, block[index]);
    }
}

void test_the_phase_can_be_read_and_set(void)
{
    // Two shapes keeping step with each other is what this is for.
    generate_t sine = generate_make(GENERATE_SINE);
    generate_t square = generate_make(GENERATE_SQUARE);

    generate_design(&sine, REAL_C(1000.0), RATE);
    generate_design(&square, REAL_C(1000.0), RATE);

    generate_set_phase(&sine, REAL_C(0.25));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.25),
                            generate_get_phase(&sine));

    // At a quarter of the turn a sine stands at 1.
    real_t at_a_quarter = generate_sample(&sine);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), at_a_quarter);

    // A phase outside one turn is folded into it rather than refused.
    generate_set_phase(&square, REAL_C(2.75));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.75),
                            generate_get_phase(&square));

    generate_set_phase(&square, -REAL_C(0.25));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.75),
                            generate_get_phase(&square));
}

void test_generate_refuses_what_it_cannot_make(void)
{
    generate_t generate = generate_make(GENERATE_SINE);

    TEST_ASSERT_EQUAL(false, generate_design(&generate, REAL_C(5000.0),
                                             RATE));

    // Nothing has been designed, thus nothing is made.
    real_t nothing = generate_sample(&generate);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), nothing);

    real_t block[4];
    TEST_ASSERT_EQUAL(false, generate_block(&generate, block, 4u));

    // A sweep of a noise is not a thing.
    generate_t noise = generate_make(GENERATE_WHITE_NOISE);
    TEST_ASSERT_EQUAL(false, generate_design_sweep(&noise, REAL_C(100.0),
                                                   REAL_C(200.0), RATE,
                                                   100u));

    // And a noise reads no frequency at all, thus it designs whatever it is
    // given.
    TEST_ASSERT_EQUAL(true, generate_design(&noise, REAL_C(9999.0), RATE));

    TEST_ASSERT_EQUAL(false, generate_design_sweep(&generate, REAL_C(100.0),
                                                   REAL_C(200.0), RATE, 0u));
}

void test_generate_reset_puts_it_back_to_the_start(void)
{
    generate_t generate = generate_make(GENERATE_SINE);

    generate_design(&generate, REAL_C(997.0), RATE);

    real_t first[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        first[index] = generate_sample(&generate);
    }

    generate_reset(&generate);

    for(uint32_t index = 0; index < 8u; index++)
    {
        real_t again = generate_sample(&generate);

        TEST_ASSERT_EQUAL_REAL(first[index], again);
    }
}
