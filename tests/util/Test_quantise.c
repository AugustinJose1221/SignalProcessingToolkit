#include "unity.h"
#include "real_assert.h"
#include "quantise.h"
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_quantise_is_valid_way_and_bits(void)
{
    TEST_ASSERT_EQUAL(true, quantise_is_valid_way(QUANTISE_PLAIN));
    TEST_ASSERT_EQUAL(true, quantise_is_valid_way(QUANTISE_SHAPED));
    TEST_ASSERT_EQUAL(false, quantise_is_valid_way((quantise_way_t)9));

    TEST_ASSERT_EQUAL(true, quantise_is_valid_bits(1));
    TEST_ASSERT_EQUAL(true, quantise_is_valid_bits(16));
    TEST_ASSERT_EQUAL(true, quantise_is_valid_bits(QUANTISE_LARGEST_BITS));
    TEST_ASSERT_EQUAL(false, quantise_is_valid_bits(0));
    TEST_ASSERT_EQUAL(false,
                      quantise_is_valid_bits(QUANTISE_LARGEST_BITS + 1u));
}

void test_the_step_is_the_reach_divided_by_the_steps_there_are(void)
{
    quantise_t quantise = quantise_make();

    // 8 bits across plus and minus 1 gives 128 steps each way.
    TEST_ASSERT_EQUAL(true, quantise_design(&quantise, QUANTISE_PLAIN, 8u,
                                            REAL_C(1.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0) / REAL_C(128.0),
                            quantise_step_of(&quantise));

    quantise_design(&quantise, QUANTISE_PLAIN, 12u, REAL_C(10.0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(10.0) / REAL_C(2048.0),
                            quantise_step_of(&quantise));
}

void test_every_answer_lands_on_a_step(void)
{
    // Whatever goes in, what comes out must be a whole number of steps. That
    // is the whole of what a quantiser is.
    quantise_t quantise = quantise_make();

    quantise_design(&quantise, QUANTISE_PLAIN, 6u, REAL_C(1.0));

    real_t step = quantise_step_of(&quantise);

    for(uint32_t index = 0; index < 1000u; index++)
    {
        real_t going_in = -REAL_C(1.0)
                          + (REAL_C(2.0) * (real_t)index / REAL_C(999.0));
        real_t answer = quantise_sample(&quantise, going_in);

        real_t steps = answer / step;

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_FLOOR(steps + REAL_C(0.5)),
                                steps);
    }
}

void test_rounding_plainly_never_moves_a_sample_by_more_than_half_a_step(void)
{
    quantise_t quantise = quantise_make();

    quantise_design(&quantise, QUANTISE_PLAIN, 8u, REAL_C(1.0));

    real_t half = quantise_step_of(&quantise) / REAL_C(2.0);

    for(uint32_t index = 0; index < 2000u; index++)
    {
        real_t going_in = -REAL_C(0.9)
                          + (REAL_C(1.8) * (real_t)index / REAL_C(1999.0));
        real_t answer = quantise_sample(&quantise, going_in);

        TEST_ASSERT_TRUE(REAL_ABS(answer - going_in)
                         <= (half + REAL_C(0.000001)));
    }
}

void test_a_signal_beyond_the_reach_is_held_and_never_wraps(void)
{
    // A SIGNAL THAT WRAPS DOES NOT SOUND LOUD; IT SOUNDS BROKEN, and one
    // sample of it can undo a whole measurement.
    quantise_t quantise = quantise_make();

    quantise_design(&quantise, QUANTISE_PLAIN, 8u, REAL_C(1.0));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            quantise_sample(&quantise, REAL_C(5.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), -REAL_C(1.0),
                            quantise_sample(&quantise, -REAL_C(5.0)));

    // And it is held for every way, not only the plain one.
    quantise_design(&quantise, QUANTISE_SHAPED, 8u, REAL_C(1.0));

    for(uint32_t index = 0; index < 100u; index++)
    {
        real_t answer = quantise_sample(&quantise, REAL_C(100.0));

        TEST_ASSERT_TRUE(answer <= REAL_C(1.0));
        TEST_ASSERT_TRUE(answer >= -REAL_C(1.0));
    }
}

void test_dither_breaks_the_pattern_that_plain_rounding_leaves(void)
{
    // THE MEASUREMENT THE MODULE EXISTS FOR.
    //
    // A quiet sine crosses the same few steps over and over. Rounded plainly,
    // the error repeats with the signal and becomes a false tone that no
    // averaging removes. With dither the error is different every turn, and
    // what is different every turn averages away.
    //
    // Here that is measured by how alike the error is one whole turn later. A
    // repeating error is very alike; noise is not alike at all.
    const uint32_t turn = 20u;
    const real_t amplitude = REAL_C(0.01);

    real_t together[2];

    for(uint32_t which = 0; which < 2u; which++)
    {
        quantise_t quantise = quantise_make();

        quantise_design(&quantise,
                        (which == 0u) ? QUANTISE_PLAIN : QUANTISE_DITHER,
                        8u, REAL_C(1.0));
        quantise_set_seed(&quantise, 4242u);

        real_t held[4000];
        real_t total = REAL_C(0.0);
        real_t power = REAL_C(0.0);

        for(uint32_t index = 0; index < 4000u; index++)
        {
            real_t clean = amplitude
                           * REAL_SIN((REAL_C(2.0) * REAL_C(3.14159265)
                                       * (real_t)index) / (real_t)turn);

            held[index] = quantise_sample(&quantise, clean) - clean;
        }

        for(uint32_t index = turn; index < 4000u; index++)
        {
            total += held[index] * held[index - turn];
            power += held[index] * held[index];
        }

        together[which] = total / power;
    }

    // Rounded plainly the error is nearly the same one turn later.
    TEST_ASSERT_TRUE(together[0] > REAL_C(0.8));

    // With dither it is not.
    TEST_ASSERT_TRUE(together[1] < REAL_C(0.3));
}

void test_shaping_moves_the_noise_up_out_of_the_way(void)
{
    // NOISE SHAPING DOES NOT REMOVE NOISE. IT MOVES IT. What comes out holds
    // less of its error low down and more of it high up, and that is measured
    // by how much the error changes from one sample to the next: an error that
    // sits high up changes a great deal.
    const real_t amplitude = REAL_C(0.01);

    real_t roughness[2];

    for(uint32_t which = 0; which < 2u; which++)
    {
        quantise_t quantise = quantise_make();

        quantise_design(&quantise,
                        (which == 0u) ? QUANTISE_DITHER : QUANTISE_SHAPED,
                        8u, REAL_C(1.0));
        quantise_set_seed(&quantise, 99u);

        real_t previous = REAL_C(0.0);
        real_t changes = REAL_C(0.0);
        real_t power = REAL_C(0.0);

        for(uint32_t index = 0; index < 4000u; index++)
        {
            real_t clean = amplitude
                           * REAL_SIN((REAL_C(2.0) * REAL_C(3.14159265)
                                       * (real_t)index) / REAL_C(20.0));

            real_t error = quantise_sample(&quantise, clean) - clean;

            if(index > 0u)
            {
                real_t moved = error - previous;

                changes += moved * moved;
            }

            power += error * error;
            previous = error;
        }

        roughness[which] = changes / power;
    }

    // The shaped error changes far more from sample to sample, which is what
    // sitting high up means.
    TEST_ASSERT_TRUE(roughness[1] > roughness[0]);
    TEST_ASSERT_TRUE(roughness[1] > REAL_C(2.0));
}

void test_quantise_noise_floor(void)
{
    // The number every converter is sold on: about six decibels for each bit.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), -REAL_C(49.9),
                            quantise_noise_floor(8u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), -REAL_C(98.1),
                            quantise_noise_floor(16u));

    // Each bit is worth about six more.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(6.02),
                            quantise_noise_floor(8u)
                            - quantise_noise_floor(9u));
}

void test_the_same_seed_gives_the_same_dither(void)
{
    quantise_t first = quantise_make();
    quantise_t second = quantise_make();

    quantise_design(&first, QUANTISE_DITHER, 8u, REAL_C(1.0));
    quantise_design(&second, QUANTISE_DITHER, 8u, REAL_C(1.0));
    quantise_set_seed(&first, 7u);
    quantise_set_seed(&second, 7u);

    for(uint32_t index = 0; index < 500u; index++)
    {
        real_t going_in = REAL_C(0.003) * (real_t)(index % 17u);

        real_t from_first = quantise_sample(&first, going_in);
        real_t from_second = quantise_sample(&second, going_in);

        TEST_ASSERT_EQUAL_REAL(from_first, from_second);
    }
}

void test_quantise_block_gives_the_same_as_one_at_a_time(void)
{
    quantise_t one = quantise_make();
    quantise_t many = quantise_make();

    quantise_design(&one, QUANTISE_SHAPED, 6u, REAL_C(1.0));
    quantise_design(&many, QUANTISE_SHAPED, 6u, REAL_C(1.0));
    quantise_set_seed(&one, 11u);
    quantise_set_seed(&many, 11u);

    real_t going_in[64];
    real_t block[64];

    for(uint32_t index = 0; index < 64u; index++)
    {
        going_in[index] = REAL_C(0.4)
                          * REAL_SIN(REAL_C(0.3) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, quantise_block(&many, going_in, block, 64u));

    for(uint32_t index = 0; index < 64u; index++)
    {
        real_t one_at_a_time = quantise_sample(&one, going_in[index]);

        TEST_ASSERT_EQUAL_REAL(one_at_a_time, block[index]);
    }
}

void test_quantise_block_may_write_over_its_input(void)
{
    quantise_t apart = quantise_make();
    quantise_t together = quantise_make();

    quantise_design(&apart, QUANTISE_DITHER, 8u, REAL_C(1.0));
    quantise_design(&together, QUANTISE_DITHER, 8u, REAL_C(1.0));
    quantise_set_seed(&apart, 5u);
    quantise_set_seed(&together, 5u);

    real_t going_in[32];
    real_t output[32];
    real_t in_place[32];

    for(uint32_t index = 0; index < 32u; index++)
    {
        going_in[index] = REAL_C(0.5)
                          * REAL_SIN(REAL_C(0.2) * (real_t)index);
        in_place[index] = going_in[index];
    }

    quantise_block(&apart, going_in, output, 32u);
    quantise_block(&together, in_place, in_place, 32u);

    for(uint32_t index = 0; index < 32u; index++)
    {
        TEST_ASSERT_EQUAL_REAL(output[index], in_place[index]);
    }
}

void test_quantise_refuses_what_it_cannot_do(void)
{
    quantise_t quantise = quantise_make();

    TEST_ASSERT_EQUAL(false, quantise_design(&quantise, (quantise_way_t)9,
                                             8u, REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, quantise_design(&quantise, QUANTISE_PLAIN, 0u,
                                             REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, quantise_design(&quantise, QUANTISE_PLAIN, 8u,
                                             REAL_C(0.0)));

    // Nothing designed, thus the sample passes through untouched.
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.1234),
                           quantise_sample(&quantise, REAL_C(0.1234)));

    real_t block[4];
    TEST_ASSERT_EQUAL(false, quantise_block(&quantise, block, block, 4u));
}

void test_a_quantiser_of_one_bit_gives_two_values(void)
{
    // The edge of what the module takes, and it must still behave.
    quantise_t quantise = quantise_make();

    TEST_ASSERT_EQUAL(true, quantise_design(&quantise, QUANTISE_PLAIN, 1u,
                                            REAL_C(1.0)));

    for(uint32_t index = 0; index < 100u; index++)
    {
        real_t going_in = -REAL_C(1.0)
                          + (REAL_C(2.0) * (real_t)index / REAL_C(99.0));
        real_t answer = quantise_sample(&quantise, going_in);

        TEST_ASSERT_TRUE((answer == REAL_C(0.0))
                         || (answer == REAL_C(1.0))
                         || (answer == -REAL_C(1.0)));
    }
}
