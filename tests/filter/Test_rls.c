#include "unity.h"
#include "real_assert.h"
#include "rls.h"
#include "ringbuf.h"
#include <math.h>

#define LENGTH      8u
#define RUN         400u

static uint32_t seed;

void setUp(void)
{
    seed = 20260825u;
}

void tearDown(void)
{

}

static real_t noise(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

// The response the filter has to learn.
static void truth_of(real_t* wanted)
{
    for(uint32_t index = 0; index < LENGTH; index++)
    {
        wanted[index] = REAL_COS(REAL_C(0.7) * (real_t)index)
                        * REAL_EXP(-REAL_C(0.15) * (real_t)index);
    }
}

// Run the filter against that response and give how far its coefficients still
// stand from it, as a part of the response itself.
static real_t learn(rls_t* rls, uint32_t samples)
{
    real_t wanted[LENGTH];
    real_t history[LENGTH];

    truth_of(wanted);

    for(uint32_t index = 0; index < LENGTH; index++)
    {
        history[index] = REAL_C(0.0);
    }

    for(uint32_t step = 0; step < samples; step++)
    {
        real_t sample = noise();

        for(uint32_t index = LENGTH - 1u; index > 0u; index--)
        {
            history[index] = history[index - 1u];
        }

        history[0] = sample;

        real_t should_be = REAL_C(0.0);

        for(uint32_t index = 0; index < LENGTH; index++)
        {
            should_be += wanted[index] * history[index];
        }

        rls_error(rls, sample, should_be);
    }

    real_t missed = REAL_C(0.0);
    real_t whole = REAL_C(0.0);

    for(uint32_t index = 0; index < LENGTH; index++)
    {
        real_t apart = rls_get_coefficient(rls, index) - wanted[index];

        missed += apart * apart;
        whole += wanted[index] * wanted[index];
    }

    return missed / whole;
}

void test_rls_is_valid_forgetting(void)
{
    TEST_ASSERT_EQUAL(true, rls_is_valid_forgetting(REAL_C(1.0)));
    TEST_ASSERT_EQUAL(true, rls_is_valid_forgetting(REAL_C(0.99)));
    TEST_ASSERT_EQUAL(true, rls_is_valid_forgetting(RLS_SMALLEST_FORGETTING));

    // Remembering more than everything is not a thing.
    TEST_ASSERT_EQUAL(false, rls_is_valid_forgetting(REAL_C(1.01)));

    // And forgetting so fast that the matrix is rebuilt from too few samples.
    TEST_ASSERT_EQUAL(false, rls_is_valid_forgetting(REAL_C(0.5)));
}

void test_the_filter_learns_the_response_it_is_shown(void)
{
    rls_t rls = rls_alloc(LENGTH);

    TEST_ASSERT_EQUAL(true, rls_design(&rls, REAL_C(1.0), RLS_DEFAULT_DOUBT));

    // A part in ten thousand is 40 dB towards the truth.
    TEST_ASSERT_TRUE(learn(&rls, RUN) < REAL_C(0.0001));
    TEST_ASSERT_EQUAL(true, rls_is_healthy(&rls));

    rls_free(&rls);
}

void test_it_learns_in_about_as_many_samples_as_it_has_coefficients(void)
{
    // THE REASON THE MODULE EXISTS. The adaptive module needs hundreds of
    // samples for this; this one needs a few dozen.
    rls_t rls = rls_alloc(LENGTH);

    rls_design(&rls, REAL_C(1.0), RLS_DEFAULT_DOUBT);

    // Three times the length, and already most of the way there.
    TEST_ASSERT_TRUE(learn(&rls, LENGTH * 3u) < REAL_C(0.001));

    rls_free(&rls);
}

void test_the_two_halves_of_the_matrix_stay_exactly_equal(void)
{
    // THE FAULT THIS MODULE IS WRITTEN TO AVOID.
    //
    // The matrix the filter carries should be symmetric, and nothing in the
    // arithmetic holds it to that. Written the usual way, each half is worked
    // out on its own, their roundings differ, and after a few thousand samples
    // one direction of the spread goes below nothing and the filter runs away.
    //
    // This module works out one half and writes it to both. That holds them
    // EXACTLY equal, not nearly, and this test says exactly.
    rls_t rls = rls_alloc(LENGTH);

    rls_design(&rls, REAL_C(0.99), RLS_DEFAULT_DOUBT);
    learn(&rls, RUN);

    for(uint32_t row = 0; row < LENGTH; row++)
    {
        for(uint32_t column = 0; column < LENGTH; column++)
        {
            TEST_ASSERT_EQUAL_REAL(rls.inverse[(row * LENGTH) + column],
                                   rls.inverse[(column * LENGTH) + row]);
        }
    }

    rls_free(&rls);
}

void test_the_filter_stays_healthy_while_the_past_fades(void)
{
    // A fading past is what makes the matrix lose its footing fastest, thus
    // every factor the module accepts is run.
    const real_t factors[4] = {REAL_C(1.0), REAL_C(0.999), REAL_C(0.99),
                               REAL_C(0.95)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        rls_t rls = rls_alloc(LENGTH);

        TEST_ASSERT_EQUAL(true, rls_design(&rls, factors[which],
                                           RLS_DEFAULT_DOUBT));

        learn(&rls, RUN * 4u);

        TEST_ASSERT_EQUAL(true, rls_is_healthy(&rls));

        rls_free(&rls);
    }
}

void test_a_fading_past_lets_the_filter_follow_something_that_moves(void)
{
    // What the forgetting factor is FOR. The response changes half way
    // through, and a filter that remembers everything cannot follow it.
    rls_t remembers = rls_alloc(LENGTH);
    rls_t forgets = rls_alloc(LENGTH);

    rls_design(&remembers, REAL_C(1.0), RLS_DEFAULT_DOUBT);
    rls_design(&forgets, REAL_C(0.97), RLS_DEFAULT_DOUBT);

    real_t history[LENGTH];

    for(uint32_t index = 0; index < LENGTH; index++)
    {
        history[index] = REAL_C(0.0);
    }

    // First a response of one shape, then quite another.
    for(uint32_t step = 0; step < 2000u; step++)
    {
        real_t sample = noise();

        for(uint32_t index = LENGTH - 1u; index > 0u; index--)
        {
            history[index] = history[index - 1u];
        }

        history[0] = sample;

        real_t should_be = (step < 1000u) ? history[1]
                                          : (REAL_C(2.0) * history[5]);

        rls_error(&remembers, sample, should_be);
        rls_error(&forgets, sample, should_be);
    }

    // The one that forgets has found the new response; the one that remembers
    // is still holding an average of both.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(2.0),
                            rls_get_coefficient(&forgets, 5));
    TEST_ASSERT_TRUE(REAL_ABS(rls_get_coefficient(&remembers, 5)
                              - REAL_C(2.0)) > REAL_C(0.1));

    rls_free(&remembers);
    rls_free(&forgets);
}

void test_what_is_left_over_is_the_answer(void)
{
    // As with the adaptive module: where the filter is learning noise so that
    // it can be taken away, the error is the signal.
    rls_t rls = rls_alloc(LENGTH);

    rls_design(&rls, REAL_C(1.0), RLS_DEFAULT_DOUBT);

    real_t history[LENGTH];

    for(uint32_t index = 0; index < LENGTH; index++)
    {
        history[index] = REAL_C(0.0);
    }

    real_t left_over = REAL_C(0.0);
    real_t interference_alone = REAL_C(0.0);
    uint32_t counted = 0u;

    for(uint32_t step = 0; step < 2000u; step++)
    {
        real_t interference = noise();

        for(uint32_t index = LENGTH - 1u; index > 0u; index--)
        {
            history[index] = history[index - 1u];
        }

        history[0] = interference;

        // The signal, which the reference knows nothing about.
        real_t signal = REAL_SIN(REAL_C(0.1) * (real_t)step);

        // What the sensor hears: the signal plus the interference, delayed and
        // made smaller on its way.
        real_t heard = signal + (REAL_C(0.6) * history[2]);

        real_t left = rls_error(&rls, interference, heard);

        if(step > 500u)
        {
            real_t apart = left - signal;
            real_t before = REAL_C(0.6) * history[2];

            left_over += apart * apart;
            interference_alone += before * before;
            counted++;
        }
    }

    // WHAT IS LEFT OVER IS NOT THE SIGNAL EXACTLY, and the header says why:
    // the filter is estimating a response from measurements that hold the
    // signal too, and the signal it cannot see acts as noise on that estimate.
    //
    // What can be asked is how much of the interference went, and 20 dB of it
    // is what this arrangement gives.
    real_t was = REAL_SQRT(interference_alone / (real_t)counted);
    real_t is_now = REAL_SQRT(left_over / (real_t)counted);

    TEST_ASSERT_TRUE(is_now < (was / REAL_C(10.0)));

    rls_free(&rls);
}

void test_rls_design_refuses_what_it_cannot_use(void)
{
    rls_t rls = rls_alloc(LENGTH);

    TEST_ASSERT_EQUAL(false, rls_design(&rls, REAL_C(1.5),
                                        RLS_DEFAULT_DOUBT));
    TEST_ASSERT_EQUAL(false, rls_design(&rls, REAL_C(0.1),
                                        RLS_DEFAULT_DOUBT));
    TEST_ASSERT_EQUAL(false, rls_design(&rls, REAL_C(1.0), REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, rls_design(&rls, REAL_C(1.0), -REAL_C(1.0)));

    rls_free(&rls);
}

void test_rls_reset_clears_what_was_learned(void)
{
    rls_t rls = rls_alloc(LENGTH);

    rls_design(&rls, REAL_C(1.0), RLS_DEFAULT_DOUBT);
    learn(&rls, RUN);

    TEST_ASSERT_TRUE(REAL_ABS(rls_get_coefficient(&rls, 0)) > REAL_C(0.1));

    rls_reset(&rls);

    for(uint32_t index = 0; index < LENGTH; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                                rls_get_coefficient(&rls, index));
    }

    TEST_ASSERT_EQUAL(true, rls_is_healthy(&rls));

    rls_free(&rls);
}

void test_rls_static_alloc(void)
{
    static real_t coefficient[LENGTH];
    static real_t inverse[RLS_MATRIX_SIZE(LENGTH)];
    static real_t gain[LENGTH];
    static real_t carried[LENGTH];
    static real_t history[LENGTH];

    rls_t rls = rls_static_alloc(LENGTH, coefficient, inverse, gain, carried,
                                 history);

    TEST_ASSERT_EQUAL(LENGTH, rls.length);
    TEST_ASSERT_EQUAL(true, rls_design(&rls, REAL_C(1.0), RLS_DEFAULT_DOUBT));

    TEST_ASSERT_TRUE(learn(&rls, RUN) < REAL_C(0.0001));

    rls_free(&rls);
}

void test_rls_matrix_size(void)
{
    // The whole reason this is a module of its own: the memory grows with the
    // square of the length.
    TEST_ASSERT_EQUAL(256, RLS_MATRIX_SIZE(16));
    TEST_ASSERT_EQUAL(4096, RLS_MATRIX_SIZE(64));
    TEST_ASSERT_EQUAL(65536, RLS_MATRIX_SIZE(256));
}
