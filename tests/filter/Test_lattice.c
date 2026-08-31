#include "unity.h"
#include "real_assert.h"
#include "lattice.h"
#include <math.h>

#define STAGES      6u

static uint32_t seed;

void setUp(void)
{
    seed = 20260826u;
}

void tearDown(void)
{

}

static real_t white(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

void test_lattice_is_valid_rate_and_forgetting(void)
{
    TEST_ASSERT_EQUAL(true, lattice_is_valid_rate(REAL_C(0.2)));
    TEST_ASSERT_EQUAL(true, lattice_is_valid_rate(LATTICE_LARGEST_RATE));
    TEST_ASSERT_EQUAL(false, lattice_is_valid_rate(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, lattice_is_valid_rate(-REAL_C(0.1)));
    TEST_ASSERT_EQUAL(false,
                      lattice_is_valid_rate(LATTICE_LARGEST_RATE
                                            + REAL_C(0.1)));

    TEST_ASSERT_EQUAL(true, lattice_is_valid_forgetting(REAL_C(0.99)));
    TEST_ASSERT_EQUAL(true, lattice_is_valid_forgetting(REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, lattice_is_valid_forgetting(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, lattice_is_valid_forgetting(REAL_C(1.1)));
}

void test_the_first_stage_finds_how_much_the_input_leans_on_itself(void)
{
    // THE LADDER ITSELF, APART FROM THE WEIGHTS.
    //
    // For an input where each sample is 0.9 of the one before plus a little
    // noise, the first stage must find 0.9 and every stage beyond it must find
    // nothing: there is nothing left for them to find.
    lattice_t lattice = lattice_alloc(4);

    lattice_design(&lattice, REAL_C(0.5), REAL_C(0.99));

    real_t last = REAL_C(0.0);

    for(uint32_t step = 0; step < 20000u; step++)
    {
        real_t sample = (REAL_C(0.9) * last) + (REAL_C(0.1) * white());

        last = sample;
        lattice_process_sample(&lattice, sample, REAL_C(0.0));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.08), REAL_C(0.9),
                            lattice_get_reflection(&lattice, 0));

    for(uint32_t stage = 1; stage < 4u; stage++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.15), REAL_C(0.0),
                                lattice_get_reflection(&lattice, stage));
    }

    lattice_free(&lattice);
}

void test_every_reflection_number_stays_between_minus_one_and_one(void)
{
    // WHY A LADDER CANNOT RUN AWAY. A number outside this describes a stage
    // giving out more than it was given, and the arithmetic holds it rather
    // than trusting it. An rls filter has a whole matrix that nothing holds.
    lattice_t lattice = lattice_alloc(STAGES);

    lattice_design(&lattice, LATTICE_LARGEST_RATE, REAL_C(0.99));

    real_t last = REAL_C(0.0);

    // A hard input: very heavily leaning, and large.
    for(uint32_t step = 0; step < 20000u; step++)
    {
        real_t sample = (REAL_C(0.99) * last)
                        + (REAL_C(50.0) * white());

        last = sample;
        lattice_process_sample(&lattice, sample, REAL_C(100.0) * white());

        for(uint32_t stage = 0; stage < STAGES; stage++)
        {
            real_t reflection = lattice_get_reflection(&lattice, stage);

            TEST_ASSERT_TRUE(reflection <= LATTICE_LARGEST_REFLECTION);
            TEST_ASSERT_TRUE(reflection >= -LATTICE_LARGEST_REFLECTION);
        }
    }

    lattice_free(&lattice);
}

void test_the_ladder_learns_the_response_it_is_shown(void)
{
    lattice_t lattice = lattice_alloc(STAGES);

    lattice_design(&lattice, REAL_C(0.2), REAL_C(0.99));

    real_t last = REAL_C(0.0);
    real_t history[STAGES + 1u];

    for(uint32_t index = 0; index <= STAGES; index++)
    {
        history[index] = REAL_C(0.0);
    }

    real_t error_power = REAL_C(0.0);
    real_t wanted_power = REAL_C(0.0);

    for(uint32_t step = 0; step < 40000u; step++)
    {
        real_t sample = (REAL_C(0.9) * last) + (REAL_C(0.1) * white());

        last = sample;

        for(uint32_t index = STAGES; index > 0u; index--)
        {
            history[index] = history[index - 1u];
        }

        history[0] = sample;

        real_t wanted = (REAL_C(0.8) * history[1])
                        - (REAL_C(0.5) * history[3]);

        real_t left = lattice_process_sample(&lattice, sample, wanted);

        if(step > 20000u)
        {
            error_power += left * left;
            wanted_power += wanted * wanted;
        }
    }

    // Twenty decibels down is a hundredth of the power.
    TEST_ASSERT_TRUE(error_power < (wanted_power / REAL_C(100.0)));

    lattice_free(&lattice);
}

void test_what_is_left_after_learning_is_never_the_larger(void)
{
    // THE TWO ERRORS, AND THE REASON BOTH ARE OFFERED.
    //
    // The error after a sample has been learned from has been told the answer
    // first, thus it is always the smaller. Reporting it as how well a filter
    // is doing is how an adaptive filter comes to look better than it is.
    lattice_t lattice = lattice_alloc(STAGES);

    lattice_design(&lattice, REAL_C(0.2), REAL_C(0.99));

    real_t last = REAL_C(0.0);
    uint32_t smaller = 0u;

    for(uint32_t step = 0; step < 2000u; step++)
    {
        real_t sample = (REAL_C(0.9) * last) + (REAL_C(0.1) * white());

        last = sample;

        lattice_process_sample(&lattice, sample, sample * REAL_C(0.5));

        real_t before = lattice_error_before(&lattice);
        real_t after = lattice_error_after(&lattice);

        TEST_ASSERT_TRUE(REAL_ABS(after) <= (REAL_ABS(before)
                                             + REAL_C(0.000001)));

        if(REAL_ABS(after) < REAL_ABS(before))
        {
            smaller++;
        }
    }

    // And it really is smaller, not merely never larger.
    TEST_ASSERT_TRUE(smaller > 1000u);

    lattice_free(&lattice);
}

void test_the_answer_that_comes_back_is_the_error_before_learning(void)
{
    lattice_t lattice = lattice_alloc(STAGES);

    lattice_design(&lattice, REAL_C(0.2), REAL_C(0.99));

    for(uint32_t step = 0; step < 100u; step++)
    {
        real_t sample = white();
        real_t given = lattice_process_sample(&lattice, sample,
                                              sample * REAL_C(0.5));

        TEST_ASSERT_EQUAL_REAL(lattice_error_before(&lattice), given);
    }

    lattice_free(&lattice);
}

void test_lattice_design_refuses_what_it_cannot_use(void)
{
    lattice_t lattice = lattice_alloc(STAGES);

    TEST_ASSERT_EQUAL(false, lattice_design(&lattice, REAL_C(0.0),
                                            REAL_C(0.99)));
    TEST_ASSERT_EQUAL(false, lattice_design(&lattice, REAL_C(2.0),
                                            REAL_C(0.99)));
    TEST_ASSERT_EQUAL(false, lattice_design(&lattice, REAL_C(0.2),
                                            REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, lattice_design(&lattice, REAL_C(0.2),
                                            REAL_C(1.5)));

    TEST_ASSERT_EQUAL(true, lattice_design(&lattice, REAL_C(0.2),
                                           REAL_C(0.99)));

    lattice_free(&lattice);
}

void test_lattice_reset_clears_what_was_learned(void)
{
    lattice_t lattice = lattice_alloc(STAGES);

    lattice_design(&lattice, REAL_C(0.5), REAL_C(0.99));

    real_t last = REAL_C(0.0);

    for(uint32_t step = 0; step < 2000u; step++)
    {
        real_t sample = (REAL_C(0.9) * last) + (REAL_C(0.1) * white());

        last = sample;
        lattice_process_sample(&lattice, sample, sample);
    }

    TEST_ASSERT_TRUE(REAL_ABS(lattice_get_reflection(&lattice, 0))
                     > REAL_C(0.1));

    lattice_reset(&lattice);

    for(uint32_t stage = 0; stage < STAGES; stage++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                                lattice_get_reflection(&lattice, stage));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            lattice_error_before(&lattice));

    lattice_free(&lattice);
}

void test_lattice_static_alloc(void)
{
    static real_t reflection[STAGES + 1u];
    static real_t forward[STAGES + 1u];
    static real_t backward[STAGES + 1u];
    static real_t held[STAGES + 1u];
    static real_t energy[STAGES + 1u];
    static real_t weight[STAGES + 1u];

    lattice_t lattice = lattice_static_alloc(STAGES, reflection, forward,
                                             backward, held, energy, weight);

    TEST_ASSERT_EQUAL(STAGES, lattice.stages);
    TEST_ASSERT_EQUAL(true, lattice_design(&lattice, REAL_C(0.5),
                                           REAL_C(0.99)));

    real_t last = REAL_C(0.0);

    for(uint32_t step = 0; step < 20000u; step++)
    {
        real_t sample = (REAL_C(0.9) * last) + (REAL_C(0.1) * white());

        last = sample;
        lattice_process_sample(&lattice, sample, REAL_C(0.0));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.08), REAL_C(0.9),
                            lattice_get_reflection(&lattice, 0));

    lattice_free(&lattice);
}

// A filter told to forget nothing must still answer with numbers.
//
// The weights divide by the mean loudness of each stage, and that mean used to
// be worked out by multiplying the running sum by one less the forgetting
// factor. At a factor of exactly one, which lattice_is_valid_forgetting takes,
// that multiplier is NOTHING. The whole normalisation then disappeared and
// each weight was left dividing by its own sample alone, thus one quiet sample
// moved a weight by thousands and the next loud one carried the answer away.
// Measured, the answer reached infinity by sample 244 at a rate of 1.0.
void test_lattice_forgetting_nothing_still_answers(void)
{
    real_t rates[2] = {REAL_C(0.5), REAL_C(1.0)};

    for(uint32_t which = 0; which < 2u; which++)
    {
        lattice_t lattice = lattice_alloc(2u);

        TEST_ASSERT_EQUAL(true, lattice_design(&lattice, rates[which],
                                               REAL_C(1.0)));

        seed = 1u;

        real_t last = REAL_C(0.0);

        for(uint32_t step = 0; step < 2000u; step++)
        {
            real_t sample = white();
            real_t wanted = (REAL_C(0.6) * sample) - (REAL_C(0.3) * last);

            last = sample;

            real_t left = lattice_process_sample(&lattice, sample, wanted);

            TEST_ASSERT_TRUE(REAL_ABS(left) < REAL_C(1000.0));
        }

        lattice_free(&lattice);
    }
}

// A BLOCK MUST BE THE SAMPLES ONE AT A TIME. Every stage learns from every
// sample, thus a block form that differed would leave a different ladder.
void test_lattice_a_block_is_the_samples_one_at_a_time(void)
{
    const uint32_t count = 300u;
    static real_t reference[300];
    static real_t wanted[300];
    static real_t error[300];

    seed = 13u;

    for(uint32_t index = 0; index < count; index++)
    {
        reference[index] = white();
        wanted[index] = (REAL_C(0.6) * reference[index])
                        - ((index > 0u) ? (REAL_C(0.3) * reference[index - 1u])
                                        : REAL_C(0.0));
    }

    lattice_t together = lattice_alloc(4u);
    lattice_t apart = lattice_alloc(4u);

    lattice_design(&together, REAL_C(0.5), REAL_C(0.99));
    lattice_design(&apart, REAL_C(0.5), REAL_C(0.99));

    TEST_ASSERT_EQUAL(true, lattice_process_block(&together, reference,
                                                  wanted, error, count));

    for(uint32_t index = 0; index < count; index++)
    {
        real_t left = lattice_process_sample(&apart, reference[index],
                                             wanted[index]);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), left, error[index]);
    }

    for(uint32_t stage = 0; stage < 4u; stage++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                                lattice_get_reflection(&apart, stage),
                                lattice_get_reflection(&together, stage));
    }

    lattice_free(&together);
    lattice_free(&apart);
}

// A ladder that was never designed has no stages to run a block through.
void test_lattice_a_block_of_an_undesigned_ladder_is_refused(void)
{
    real_t reference[4] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    real_t wanted[4] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    real_t error[4];

    lattice_t lattice = lattice_alloc(2u);

    lattice.designed = false;

    TEST_ASSERT_EQUAL(false, lattice_process_block(&lattice, reference,
                                                   wanted, error, 4u));

    lattice_free(&lattice);
}

void test_silence_leaves_the_reflections_where_they_were(void)
{
    // Each stage divides by how loud it is, and in silence that is nothing.
    // The floor keeps the division honest. Without it the reflections would be
    // set from a division by nothing and the filter would never recover.
    lattice_t lattice = lattice_alloc(4u);
    TEST_ASSERT_TRUE(lattice_design(&lattice, REAL_C(0.01), REAL_C(0.99)));

    for(uint32_t index = 0; index < 500u; index++)
    {
        lattice_process_sample(&lattice, REAL_C(0.0), REAL_C(0.0));
    }

    for(uint32_t stage = 0; stage < 4u; stage++)
    {
        real_t reflection = lattice_get_reflection(&lattice, stage);
        TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), reflection);
    }

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), lattice_error_after(&lattice));

    lattice_free(&lattice);
}

void test_a_reflection_is_held_inside_the_range_that_stays_steady(void)
{
    // A reflection of 1 or more is a stage that grows without bound. The
    // module holds every one of them inside the range, at both ends, whatever
    // the signal does.
    lattice_t lattice = lattice_alloc(4u);
    TEST_ASSERT_TRUE(lattice_design(&lattice, REAL_C(0.5), REAL_C(0.9)));

    // A signal that changes sign at every sample, against one that does not,
    // drives the reflections hard in both directions.
    for(uint32_t index = 0; index < 2000u; index++)
    {
        real_t sign = ((index % 2u) == 0u) ? REAL_C(1.0) : REAL_C(-1.0);
        lattice_process_sample(&lattice, sign * REAL_C(50.0),
                               REAL_C(-50.0) * sign);
    }

    for(uint32_t stage = 0; stage < 4u; stage++)
    {
        real_t reflection = lattice_get_reflection(&lattice, stage);
        TEST_ASSERT_TRUE(reflection <= LATTICE_LARGEST_REFLECTION);
        TEST_ASSERT_TRUE(reflection >= -LATTICE_LARGEST_REFLECTION);
    }

    lattice_free(&lattice);
}
