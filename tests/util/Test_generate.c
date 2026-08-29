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
    TEST_ASSERT_EQUAL(true, generate_is_valid_kind(GENERATE_GAUSSIAN_NOISE));
    TEST_ASSERT_EQUAL(true, generate_is_valid_kind(GENERATE_IMPULSE));

    // One past the last of them, whatever the last of them is.
    TEST_ASSERT_EQUAL(false,
                      generate_is_valid_kind((generate_kind_t)
                                             (GENERATE_IMPULSE + 1)));
    TEST_ASSERT_EQUAL(false, generate_is_valid_kind((generate_kind_t)-1));
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

// A CORNER REACHED FROM JUST BEFORE IT MUST BE MOVED THE WAY THAT SIDE IS
// MOVED.
//
// How far a sample stands past a corner was held from nothing to one, thus a
// sample standing a hair BEFORE the corner had a distance a hair below nothing
// and one was added to bring it round. At 64 bits a hair below nothing plus one
// rounds to EXACTLY one, and exactly one was then read as a hair AFTER the
// corner. The two sides are moved in opposite directions, thus the sample moved
// by one the wrong way and the wave jumped by two: a pulse at 100 Hz in 8000
// with a part of an eighth gave 2.0 at sample 10.
//
// The square wave and the sawtooth ran the same risk and were saved only by
// which numbers their corners happened to land on, thus all three are examined
// here.
void test_generate_a_corner_reached_from_either_side_stays_in_range(void)
{
    generate_kind_t kinds[3] = {GENERATE_PULSE, GENERATE_SQUARE,
                                GENERATE_SAWTOOTH};

    // Frequencies whose step divides into the places the corners stand, which
    // is what puts a sample exactly on a corner.
    real_t frequencies[4] = {REAL_C(100.0), REAL_C(125.0), REAL_C(200.0),
                             REAL_C(500.0)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        for(uint32_t index = 0; index < 4u; index++)
        {
            generate_t maker = generate_make(kinds[which]);

            TEST_ASSERT_EQUAL(true, generate_design(&maker,
                                                    frequencies[index], RATE));
            TEST_ASSERT_EQUAL(true, generate_set_part(&maker,
                                                      REAL_C(0.125)));

            for(uint32_t step = 0; step < 4000u; step++)
            {
                real_t value = generate_sample(&maker);

                TEST_ASSERT_TRUE(value <= REAL_C(1.0001));
                TEST_ASSERT_TRUE(value >= -REAL_C(1.0001));
            }
        }
    }
}

void test_generate_is_valid_part(void)
{
    TEST_ASSERT_EQUAL(true, generate_is_valid_part(REAL_C(0.5)));
    TEST_ASSERT_EQUAL(true, generate_is_valid_part(REAL_C(0.01)));
    // A pulse filling none of the turn or all of it has no corners.
    TEST_ASSERT_EQUAL(false, generate_is_valid_part(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, generate_is_valid_part(REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, generate_is_valid_part(-REAL_C(0.1)));

    generate_t maker = generate_make(GENERATE_PULSE);

    // The default is a half, which is what makes the pulse a square wave.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5),
                            generate_get_part(&maker));

    TEST_ASSERT_EQUAL(false, generate_set_part(&maker, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5),
                            generate_get_part(&maker));

    TEST_ASSERT_EQUAL(true, generate_set_part(&maker, REAL_C(0.25)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.25),
                            generate_get_part(&maker));
}

// A pulse filling half of each turn IS the square wave, thus the two must agree
// sample for sample. If they did not, one of them would be wrong.
void test_generate_a_pulse_of_half_a_turn_is_the_square_wave(void)
{
    generate_t pulse = generate_make(GENERATE_PULSE);
    generate_t square = generate_make(GENERATE_SQUARE);

    TEST_ASSERT_EQUAL(true, generate_design(&pulse, REAL_C(300.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_design(&square, REAL_C(300.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_set_part(&pulse, REAL_C(0.5)));

    for(uint32_t step = 0; step < 2000u; step++)
    {
        real_t one = generate_sample(&pulse);
        real_t other = generate_sample(&square);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), other, one);
    }
}

// The part of the turn a pulse is high for is the part of the turn it is high
// for, which shows in what it adds up to.
void test_generate_a_pulse_is_high_for_the_part_it_was_given(void)
{
    real_t parts[3] = {REAL_C(0.125), REAL_C(0.25), REAL_C(0.75)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        generate_t maker = generate_make(GENERATE_PULSE);

        TEST_ASSERT_EQUAL(true, generate_design(&maker, REAL_C(100.0), RATE));
        TEST_ASSERT_EQUAL(true, generate_set_part(&maker, parts[which]));

        real_t total = REAL_C(0.0);
        const uint32_t count = 8000u;

        for(uint32_t step = 0; step < count; step++)
        {
            total += generate_sample(&maker);
        }

        // High at one for that part of the turn and low at minus one for the
        // rest, thus the mean is the part less what is left of the turn.
        real_t expected = parts[which] - (REAL_C(1.0) - parts[which]);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), expected,
                                total / (real_t)count);
    }
}

// THE ONE THE REST OF THE LIBRARY ASSUMES. Its spread must be one, and its
// tails must really be there: a spread with no tails cannot examine a threshold
// that asks what happens past four of them.
void test_generate_gaussian_noise_has_the_spread_and_the_tails(void)
{
    generate_t maker = generate_make(GENERATE_GAUSSIAN_NOISE);

    TEST_ASSERT_EQUAL(true, generate_design(&maker, REAL_C(100.0), RATE));
    generate_set_seed(&maker, 12345u);

    const uint32_t count = 200000u;
    real_t total = REAL_C(0.0);
    real_t squared = REAL_C(0.0);
    real_t furthest = REAL_C(0.0);
    uint32_t past_two = 0u;
    uint32_t past_three = 0u;

    for(uint32_t step = 0; step < count; step++)
    {
        real_t value = generate_sample(&maker);

        total += value;
        squared += value * value;

        real_t size = REAL_ABS(value);

        if(size > furthest) { furthest = size; }
        if(size > REAL_C(2.0)) { past_two++; }
        if(size > REAL_C(3.0)) { past_three++; }
    }

    real_t mean = total / (real_t)count;
    real_t spread = REAL_SQRT((squared / (real_t)count) - (mean * mean));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.0), mean);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0), spread);

    // A normal spread puts 4.55 in every hundred past two standard deviations
    // and 0.27 in every hundred past three. THESE ARE THE NUMBERS THAT SAY IT
    // IS NORMAL: an even spread put through the same test gives none at all
    // past two, and the sum of a dozen even draws gives far too few past three.
    real_t share_two = (real_t)past_two / (real_t)count;
    real_t share_three = (real_t)past_three / (real_t)count;

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.005), REAL_C(0.0455), share_two);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0027), share_three);

    // And it must reach well past three, which is the whole reason it is here.
    TEST_ASSERT_TRUE(furthest > REAL_C(3.5));
}

// A random walk. It wanders, and what is added is scaled so that the spread of
// the walk matches the spread of the white noise that built it.
void test_generate_brown_noise_wanders_and_keeps_its_spread(void)
{
    generate_t brown = generate_make(GENERATE_BROWN_NOISE);
    generate_t white = generate_make(GENERATE_WHITE_NOISE);

    TEST_ASSERT_EQUAL(true, generate_design(&brown, REAL_C(100.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_design(&white, REAL_C(100.0), RATE));
    generate_set_seed(&brown, 99u);
    generate_set_seed(&white, 99u);

    const uint32_t count = 200000u;
    real_t brown_squared = REAL_C(0.0);
    real_t white_squared = REAL_C(0.0);

    // How much the signal changes from one sample to the next. A walk changes
    // far less than the noise that drives it, which is what a slope of four
    // times the power in each halving of frequency MEANS.
    real_t brown_moved = REAL_C(0.0);
    real_t white_moved = REAL_C(0.0);
    real_t brown_last = REAL_C(0.0);
    real_t white_last = REAL_C(0.0);

    for(uint32_t step = 0; step < count; step++)
    {
        real_t one = generate_sample(&brown);
        real_t other = generate_sample(&white);

        brown_squared += one * one;
        white_squared += other * other;
        brown_moved += (one - brown_last) * (one - brown_last);
        white_moved += (other - white_last) * (other - white_last);
        brown_last = one;
        white_last = other;
    }

    // The spreads match within a tenth.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1) * REAL_SQRT(white_squared),
                            REAL_SQRT(white_squared),
                            REAL_SQRT(brown_squared));

    // And the walk moves far less from sample to sample than the noise does.
    TEST_ASSERT_TRUE(brown_moved < (REAL_C(0.05) * white_moved));
}

// The mirror of pink: it moves MORE from sample to sample than the pink noise
// it is made from, which is what a rising slope means.
void test_generate_blue_noise_is_the_mirror_of_pink(void)
{
    generate_t blue = generate_make(GENERATE_BLUE_NOISE);
    generate_t pink = generate_make(GENERATE_PINK_NOISE);

    TEST_ASSERT_EQUAL(true, generate_design(&blue, REAL_C(100.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_design(&pink, REAL_C(100.0), RATE));
    generate_set_seed(&blue, 4u);
    generate_set_seed(&pink, 4u);

    const uint32_t count = 100000u;
    real_t blue_moved = REAL_C(0.0);
    real_t pink_moved = REAL_C(0.0);
    real_t blue_last = REAL_C(0.0);
    real_t pink_last = REAL_C(0.0);

    for(uint32_t step = 0; step < count; step++)
    {
        real_t one = generate_sample(&blue);
        real_t other = generate_sample(&pink);

        blue_moved += (one - blue_last) * (one - blue_last);
        pink_moved += (other - pink_last) * (other - pink_last);
        blue_last = one;
        pink_last = other;
    }

    TEST_ASSERT_TRUE(blue_moved > (REAL_C(2.0) * pink_moved));
}

// A bump once each turn, standing between nothing and one and never below.
void test_generate_a_gaussian_pulse_is_a_bump_once_each_turn(void)
{
    generate_t maker = generate_make(GENERATE_GAUSSIAN_PULSE);

    TEST_ASSERT_EQUAL(true, generate_design(&maker, REAL_C(100.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_set_part(&maker, REAL_C(0.05)));

    // One turn is 80 samples at 100 Hz in 8000, and the bump stands in the
    // middle of it.
    real_t largest = REAL_C(0.0);
    uint32_t where = 0u;

    for(uint32_t step = 0; step < 80u; step++)
    {
        real_t value = generate_sample(&maker);

        TEST_ASSERT_TRUE(value >= REAL_C(0.0));
        TEST_ASSERT_TRUE(value <= REAL_C(1.0));

        if(value > largest) { largest = value; where = step; }
    }

    // It reaches one at the middle of the turn and has died away at the ends.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), largest);
    TEST_ASSERT_TRUE((where >= 38u) && (where <= 42u));

    // A wider bump reaches further out but still tops out at one.
    generate_t wider = generate_make(GENERATE_GAUSSIAN_PULSE);

    TEST_ASSERT_EQUAL(true, generate_design(&wider, REAL_C(100.0), RATE));
    TEST_ASSERT_EQUAL(true, generate_set_part(&wider, REAL_C(0.15)));

    real_t narrow_total = REAL_C(0.0);
    real_t wide_total = REAL_C(0.0);

    generate_reset(&maker);

    for(uint32_t step = 0; step < 80u; step++)
    {
        narrow_total += generate_sample(&maker);
        wide_total += generate_sample(&wider);
    }

    TEST_ASSERT_TRUE(wide_total > narrow_total);
}

// One sample of one at the start of each turn and nothing between them. Give it
// a frequency low enough and a block holds exactly one, which is what an
// impulse response is measured with.
void test_generate_an_impulse_stands_once_each_turn(void)
{
    generate_t maker = generate_make(GENERATE_IMPULSE);

    TEST_ASSERT_EQUAL(true, generate_design(&maker, REAL_C(100.0), RATE));

    uint32_t found = 0u;
    uint32_t first = 0u;

    for(uint32_t step = 0; step < 800u; step++)
    {
        real_t value = generate_sample(&maker);

        if(value != REAL_C(0.0))
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0), value);

            if(found == 0u) { first = step; }

            found++;
        }
    }

    // Ten turns in 800 samples at 100 Hz in 8000, the first at the start.
    TEST_ASSERT_EQUAL(10u, found);
    TEST_ASSERT_EQUAL(0u, first);

    // And a turn longer than the block gives exactly one.
    generate_t alone = generate_make(GENERATE_IMPULSE);

    TEST_ASSERT_EQUAL(true, generate_design(&alone, REAL_C(4.0), RATE));

    found = 0u;

    for(uint32_t step = 0; step < 1000u; step++)
    {
        if(generate_sample(&alone) != REAL_C(0.0)) { found++; }
    }

    TEST_ASSERT_EQUAL(1u, found);
}

// Every new kind must be repeatable and must come back to where it started.
void test_generate_the_new_kinds_reset_and_repeat(void)
{
    generate_kind_t kinds[6] = {GENERATE_BROWN_NOISE, GENERATE_BLUE_NOISE,
                                GENERATE_GAUSSIAN_NOISE, GENERATE_PULSE,
                                GENERATE_GAUSSIAN_PULSE, GENERATE_IMPULSE};

    for(uint32_t which = 0; which < 6u; which++)
    {
        generate_t maker = generate_make(kinds[which]);

        TEST_ASSERT_EQUAL(true, generate_design(&maker, REAL_C(220.0), RATE));
        TEST_ASSERT_EQUAL(true, generate_set_part(&maker, REAL_C(0.2)));
        generate_set_seed(&maker, 77u);

        real_t first[200];

        for(uint32_t step = 0; step < 200u; step++)
        {
            first[step] = generate_sample(&maker);
        }

        generate_reset(&maker);
        generate_set_seed(&maker, 77u);

        for(uint32_t step = 0; step < 200u; step++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), first[step],
                                    generate_sample(&maker));
        }
    }
}

void test_a_shape_that_does_not_exist_cannot_be_designed(void)
{
    generate_t generate = generate_make((generate_kind_t)200);

    TEST_ASSERT_FALSE(generate_is_valid_kind((generate_kind_t)200));
    TEST_ASSERT_FALSE(generate_design(&generate, REAL_C(100.0),
                                      REAL_C(1000.0)));
}

void test_a_phase_set_below_nothing_is_brought_back_into_the_turn(void)
{
    // The phase says where in the turn the maker stands, from 0 to 1. A caller
    // that keeps two makers in step by copying the phase from one to the other
    // can hand over a number outside that range, and the maker must bring it
    // back rather than read past the end of its own table.
    generate_t generate = generate_make(GENERATE_SINE);
    TEST_ASSERT_TRUE(generate_design(&generate, REAL_C(100.0),
                                     REAL_C(1000.0)));

    generate_set_phase(&generate, REAL_C(-0.25));

    real_t value = generate_sample(&generate);
    real_t phase = generate_get_phase(&generate);

    TEST_ASSERT_TRUE(phase >= REAL_C(0.0));
    TEST_ASSERT_TRUE(phase < REAL_C(1.0));
    TEST_ASSERT_TRUE(value >= REAL_C(-1.5));
    TEST_ASSERT_TRUE(value <= REAL_C(1.5));
}
