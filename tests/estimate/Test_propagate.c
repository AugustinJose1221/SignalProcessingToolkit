#include "unity.h"
#include "real_assert.h"
#include "propagate.h"
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

// A turning: the first state changes at the rate of the second, and the second
// at minus the rate of the first. Started at 1 and 0, the answer is a cosine
// and a minus sine, thus it is known exactly at every moment.
static void turning(real_t time, const real_t* state, const real_t* input,
                    real_t* rate, uint32_t count)
{
    (void)time;
    (void)input;
    (void)count;

    rate[0] = state[1];
    rate[1] = -state[0];
}

// Something that falls away at a rate following how much is left, which is the
// commonest model there is.
static void falling(real_t time, const real_t* state, const real_t* input,
                    real_t* rate, uint32_t count)
{
    (void)time;
    (void)count;

    real_t pushed = (input != NULL) ? input[0] : REAL_C(0.0);

    rate[0] = (-REAL_C(2.0) * state[0]) + pushed;
}

// Carry the turning across one second in steps of the given size and give the
// worst the state was ever out by.
static real_t worst_of(propagate_method_t method, real_t step)
{
    real_t state[2] = {REAL_C(1.0), REAL_C(0.0)};
    uint32_t steps = (uint32_t)(REAL_C(1.0) / step);
    real_t worst = REAL_C(0.0);

    for(uint32_t taken = 0; taken < steps; taken++)
    {
        propagate_state(method, turning, step * (real_t)taken, step, state,
                        NULL, 2u);

        real_t at = step * (real_t)(taken + 1u);
        real_t apart_x = state[0] - REAL_COS(at);
        real_t apart_y = state[1] + REAL_SIN(at);
        real_t apart = REAL_SQRT((apart_x * apart_x) + (apart_y * apart_y));

        if(apart > worst) { worst = apart; }
    }

    return worst;
}

void test_propagate_is_valid_method_and_count(void)
{
    TEST_ASSERT_EQUAL(true, propagate_is_valid_method(PROPAGATE_EULER));
    TEST_ASSERT_EQUAL(true, propagate_is_valid_method(PROPAGATE_MIDPOINT));
    TEST_ASSERT_EQUAL(true, propagate_is_valid_method(PROPAGATE_RUNGE));
    TEST_ASSERT_EQUAL(false, propagate_is_valid_method((propagate_method_t)7));

    TEST_ASSERT_EQUAL(true, propagate_is_valid_count(1));
    TEST_ASSERT_EQUAL(true, propagate_is_valid_count(PROPAGATE_LARGEST_STATE));
    TEST_ASSERT_EQUAL(false, propagate_is_valid_count(0));
    TEST_ASSERT_EQUAL(false,
                      propagate_is_valid_count(PROPAGATE_LARGEST_STATE + 1u));
}

void test_propagate_asks_for_each_step(void)
{
    TEST_ASSERT_EQUAL(1, propagate_asks_for_each_step(PROPAGATE_EULER));
    TEST_ASSERT_EQUAL(2, propagate_asks_for_each_step(PROPAGATE_MIDPOINT));
    TEST_ASSERT_EQUAL(4, propagate_asks_for_each_step(PROPAGATE_RUNGE));
    TEST_ASSERT_EQUAL(0,
                      propagate_asks_for_each_step((propagate_method_t)7));
}

void test_the_error_of_each_method_falls_as_its_order_says(void)
{
    // THE TEST THAT SAYS EACH METHOD IS THE METHOD IT CLAIMS TO BE.
    //
    // Halving the step must halve the error of Euler, quarter that of the
    // midpoint and cut Runge to a sixteenth. Nothing else about a method
    // matters as much as this, and it either holds or the method is not what
    // it says.
    real_t coarse = worst_of(PROPAGATE_EULER, REAL_C(0.05));
    real_t fine = worst_of(PROPAGATE_EULER, REAL_C(0.025));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(2.0), coarse / fine);

    coarse = worst_of(PROPAGATE_MIDPOINT, REAL_C(0.05));
    fine = worst_of(PROPAGATE_MIDPOINT, REAL_C(0.025));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.4), REAL_C(4.0), coarse / fine);

#if defined(SPTK_REAL_64)
    // AT 32 BITS THIS CANNOT BE ASKED. The method is so accurate that by a
    // step of 0.05 its error is already below the rounding of the state
    // itself, and halving the step only adds more roundings. The header
    // records that; here it means the sixteenth can only be seen at 64 bits.
    coarse = worst_of(PROPAGATE_RUNGE, REAL_C(0.05));
    fine = worst_of(PROPAGATE_RUNGE, REAL_C(0.025));

    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), REAL_C(16.0), coarse / fine);
#endif
}

void test_the_better_method_really_is_the_better_one(void)
{
    // At the same step, each method must be well ahead of the one before it.
    real_t step = REAL_C(0.05);

    real_t euler = worst_of(PROPAGATE_EULER, step);
    real_t midpoint = worst_of(PROPAGATE_MIDPOINT, step);
    real_t runge = worst_of(PROPAGATE_RUNGE, step);

    TEST_ASSERT_TRUE(midpoint < (euler / REAL_C(10.0)));
    TEST_ASSERT_TRUE(runge < (midpoint / REAL_C(10.0)));
}

void test_something_that_falls_away_reaches_where_it_should(void)
{
    // A state that falls at twice how much is left, started at 1, stands at
    // exp(-2t) after t. This is the commonest model there is.
    real_t state[1] = {REAL_C(1.0)};

    TEST_ASSERT_EQUAL(true,
                      propagate_state_over(PROPAGATE_RUNGE, falling,
                                           REAL_C(0.0), REAL_C(1.0), 100u,
                                           state, NULL, 1u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_EXP(-REAL_C(2.0)), state[0]);
}

void test_what_is_put_in_reaches_the_model(void)
{
    // The same, with something pushing it. Held at a push of 2, it settles
    // where the falling and the pushing balance, which is at 1.
    real_t state[1] = {REAL_C(0.0)};
    real_t input[1] = {REAL_C(2.0)};

    propagate_state_over(PROPAGATE_RUNGE, falling, REAL_C(0.0), REAL_C(10.0),
                         1000u, state, input, 1u);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), state[0]);
}

void test_splitting_a_stretch_beats_taking_it_in_one_step(void)
{
    // WHY propagate_state_over EXISTS. The sample rate of a filter fixes how
    // far apart the measurements are, and that distance is often far too large
    // for one step.
    real_t at_once[2] = {REAL_C(1.0), REAL_C(0.0)};
    real_t split[2] = {REAL_C(1.0), REAL_C(0.0)};

    propagate_state(PROPAGATE_RUNGE, turning, REAL_C(0.0), REAL_C(1.0),
                    at_once, NULL, 2u);
    propagate_state_over(PROPAGATE_RUNGE, turning, REAL_C(0.0), REAL_C(1.0),
                         20u, split, NULL, 2u);

    real_t truly[2] = {REAL_COS(REAL_C(1.0)), -REAL_SIN(REAL_C(1.0))};

    real_t one_out = REAL_ABS(at_once[0] - truly[0]);
    real_t many_out = REAL_ABS(split[0] - truly[0]);

    TEST_ASSERT_TRUE(many_out < (one_out / REAL_C(1000.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), truly[0], split[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), truly[1], split[1]);
}

void test_a_state_that_does_not_change_does_not_move(void)
{
    // A rate of nothing must leave the state exactly where it was, by every
    // method, whatever the step.
    real_t state[2] = {REAL_C(3.0), REAL_C(0.0)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        // The turning of a state that is already at rest in both is nothing.
        real_t rest[2] = {REAL_C(0.0), REAL_C(0.0)};

        propagate_state((propagate_method_t)which, turning, REAL_C(0.0),
                        REAL_C(1.0), rest, NULL, 2u);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), rest[0]);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), rest[1]);
    }

    (void)state;
}

void test_propagate_refuses_what_it_cannot_carry(void)
{
    real_t state[2] = {REAL_C(1.0), REAL_C(0.0)};

    TEST_ASSERT_EQUAL(false, propagate_state((propagate_method_t)9, turning,
                                             REAL_C(0.0), REAL_C(0.1), state,
                                             NULL, 2u));
    TEST_ASSERT_EQUAL(false, propagate_state(PROPAGATE_RUNGE, turning,
                                             REAL_C(0.0), REAL_C(0.0), state,
                                             NULL, 2u));
    TEST_ASSERT_EQUAL(false, propagate_state(PROPAGATE_RUNGE, turning,
                                             REAL_C(0.0), -REAL_C(0.1), state,
                                             NULL, 2u));
    TEST_ASSERT_EQUAL(false, propagate_state(PROPAGATE_RUNGE, turning,
                                             REAL_C(0.0), REAL_C(0.1), state,
                                             NULL, 0u));
    TEST_ASSERT_EQUAL(false,
                      propagate_state(PROPAGATE_RUNGE, turning, REAL_C(0.0),
                                      REAL_C(0.1), state, NULL,
                                      PROPAGATE_LARGEST_STATE + 1u));

    TEST_ASSERT_EQUAL(false, propagate_state_over(PROPAGATE_RUNGE, turning,
                                                  REAL_C(0.0), REAL_C(1.0),
                                                  0u, state, NULL, 2u));
    TEST_ASSERT_EQUAL(false, propagate_state_over(PROPAGATE_RUNGE, turning,
                                                  REAL_C(0.0), REAL_C(0.0),
                                                  10u, state, NULL, 2u));
}
