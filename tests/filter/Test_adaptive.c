#include "unity.h"
#include "real_assert.h"
#include "adaptive.h"
#include "ringbuf.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265358979323846)

static uint32_t seed = 1u;

static real_t noise(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

void setUp(void)
{
    seed = 1u;
}

void tearDown(void)
{

}

void test_adaptive_is_valid_rule(void)
{
    TEST_ASSERT_EQUAL(true, adaptive_is_valid_rule(ADAPTIVE_PLAIN));
    TEST_ASSERT_EQUAL(true, adaptive_is_valid_rule(ADAPTIVE_SIGN));
    TEST_ASSERT_EQUAL(false, adaptive_is_valid_rule(
                          (adaptive_rule_t)(ADAPTIVE_SIGN + 1)));
}

void test_adaptive_alloc_starts_with_nothing_learned(void)
{
    adaptive_t adaptive = adaptive_alloc(8);

    for(uint32_t index = 0; index < 8u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                                adaptive_get_coefficient(&adaptive, index));
    }
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                            adaptive_get_energy(&adaptive));

    adaptive_free(&adaptive);
}

void test_adaptive_static_alloc(void)
{
    real_t coefficient[4];
    real_t history[4];

    adaptive_t adaptive = adaptive_static_alloc(4, coefficient, history);

    TEST_ASSERT_EQUAL(false, adaptive.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(coefficient, adaptive.coefficient);

    adaptive_free(&adaptive);
    TEST_ASSERT_EQUAL_PTR(coefficient, adaptive.coefficient);
}

void test_adaptive_design_refuses_what_cannot_settle(void)
{
    adaptive_t adaptive = adaptive_alloc(8);

    TEST_ASSERT_EQUAL(true, adaptive_design(&adaptive, ADAPTIVE_NORMALISED,
                                            REAL_C(0.5)));
    // A rate of 2 or more makes the normalised rule run away for any signal.
    TEST_ASSERT_EQUAL(false, adaptive_design(&adaptive, ADAPTIVE_NORMALISED,
                                             REAL_C(2.0)));
    TEST_ASSERT_EQUAL(false, adaptive_design(&adaptive, ADAPTIVE_PLAIN,
                                             REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, adaptive_design(&adaptive,
                          (adaptive_rule_t)(ADAPTIVE_SIGN + 1), REAL_C(0.1)));

    adaptive_free(&adaptive);
}

void test_adaptive_learns_a_gain(void)
{
    // The simplest thing there is to learn: the reference multiplied by 3.
    adaptive_t adaptive = adaptive_alloc(1);
    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.5));

    for(uint32_t index = 0; index < 2000u; index++)
    {
        real_t reference = noise();
        adaptive_error(&adaptive, reference, REAL_C(3.0) * reference);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(3.0),
                            adaptive_get_coefficient(&adaptive, 0));

    adaptive_free(&adaptive);
}

void test_adaptive_learns_a_delay(void)
{
    // The coefficients are the answer to what the path does, and where the
    // largest one stands is the delay in samples. The header says so.
    const uint32_t delay = 5u;
    adaptive_t adaptive = adaptive_alloc(16);
    real_t past[16];

    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.5));
    for(uint32_t index = 0; index < 16u; index++) { past[index] = REAL_C(0.0); }

    for(uint32_t index = 0; index < 8000u; index++)
    {
        real_t reference = noise();

        for(uint32_t k = 15u; k > 0u; k--) { past[k] = past[k - 1u]; }
        past[0] = reference;

        adaptive_error(&adaptive, reference, REAL_C(2.0) * past[delay]);
    }

    uint32_t largest = 0;
    for(uint32_t index = 1; index < 16u; index++)
    {
        if(REAL_ABS(adaptive_get_coefficient(&adaptive, index))
           > REAL_ABS(adaptive_get_coefficient(&adaptive, largest)))
        {
            largest = index;
        }
    }

    TEST_ASSERT_EQUAL(delay, largest);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.15), REAL_C(2.0),
                            adaptive_get_coefficient(&adaptive, delay));

    adaptive_free(&adaptive);
}

// Run the filter against a signal buried in noise, and give back how much of
// the noise is left and how much of the signal survived.
//
// The reference either sees the noise alone, which is right, or the noise with
// the signal in it, which is the way of using this filter that fails quietly.
static void cancel_noise(bool reference_holds_signal, real_t rate,
                         uint32_t steps, real_t* noise_left,
                         real_t* noise_before, real_t* signal_surviving,
                         real_t* learned)
{
    adaptive_t adaptive = adaptive_alloc(8);
    real_t past[8];

    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, rate);
    for(uint32_t index = 0; index < 8u; index++) { past[index] = REAL_C(0.0); }

    real_t before = REAL_C(0.0);
    real_t after = REAL_C(0.0);
    real_t together = REAL_C(0.0);
    real_t energy = REAL_C(0.0);
    uint32_t settled = (steps * 3u) / 4u;

    for(uint32_t index = 0; index < steps; index++)
    {
        real_t signal = REAL_SIN(REAL_C(0.11) * (real_t)index);

        // What the second sensor sees.
        real_t source = REAL_C(3.0) * noise();
        for(uint32_t k = 7u; k > 0u; k--) { past[k] = past[k - 1u]; }
        past[0] = source;

        real_t reference = reference_holds_signal ? (source + signal) : source;

        // What the first sensor sees: the signal, with the same noise arriving
        // by a path that delays it by 3 and halves it.
        real_t arriving = REAL_C(0.5) * past[3];
        real_t left = adaptive_error(&adaptive, reference, signal + arriving);

        if(index > settled)
        {
            before += arriving * arriving;
            after += (left - signal) * (left - signal);
            together += left * signal;
            energy += signal * signal;
        }
    }

    *noise_before = before;
    *noise_left = after;
    *signal_surviving = together / energy;
    *learned = adaptive_get_coefficient(&adaptive, 3);

    adaptive_free(&adaptive);
}

void test_adaptive_takes_away_noise_that_a_filter_of_frequency_cannot(void)
{
    // The use that matters. The noise here is random, thus it holds every
    // frequency and covers the signal completely: no filter of frequency could
    // part the two. What parts them is that the reference holds one and not
    // the other.
    real_t left;
    real_t before;
    real_t surviving;
    real_t learned;

    cancel_noise(false, REAL_C(0.02), 80000u, &left, &before, &surviving,
                 &learned);

    // Under a fiftieth of the noise is left.
    TEST_ASSERT_TRUE(left < (REAL_C(0.02) * before));

    // The signal came through whole.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), surviving);

    // What the filter learned IS the path: a delay of 3 and a half.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(0.5), learned);
}

void test_a_higher_rate_settles_sooner_and_leaves_more_behind(void)
{
    // The trade that the rate decides. A high rate follows a change quickly
    // and then rattles about the answer; a low rate settles closer and takes
    // longer to get there. Measured over the same number of steps:
    real_t fast_left;
    real_t slow_left;
    real_t before;
    real_t surviving;
    real_t learned;

    cancel_noise(false, REAL_C(0.5), 80000u, &fast_left, &before, &surviving,
                 &learned);
    cancel_noise(false, REAL_C(0.02), 80000u, &slow_left, &before, &surviving,
                 &learned);

    // The high rate leaves about a fifth of the noise; the low rate about a
    // hundredth. Neither is wrong; they answer different questions.
    TEST_ASSERT_TRUE(slow_left < fast_left);
}

void test_a_reference_that_holds_the_signal_takes_the_signal_away(void)
{
    // The one way to use this filter that fails quietly, which the header
    // warns about. The filter learns to take the signal away as well, because
    // that also makes the error smaller. The error falls, everything looks
    // well, and the answer has had the signal removed from it.
    real_t left;
    real_t before;
    real_t good_reference;
    real_t bad_reference;
    real_t learned;

    cancel_noise(false, REAL_C(0.05), 60000u, &left, &before, &good_reference,
                 &learned);
    cancel_noise(true, REAL_C(0.05), 60000u, &left, &before, &bad_reference,
                 &learned);

    // With a reference that sees the noise alone, nearly all of the signal
    // survives. With one that holds the signal, most of it is gone.
    TEST_ASSERT_TRUE(good_reference > REAL_C(0.9));
    TEST_ASSERT_TRUE(bad_reference < REAL_C(0.5));
}

void test_the_normalised_rule_settles_for_a_loud_reference_where_the_plain_one_runs_away(void)
{
    // The reason the normalised rule is the one to reach for. The same rate is
    // given to both, and the reference is loud. The rate that would be safe
    // for the plain rule depends on a signal the designer has not heard yet.
    adaptive_t plain = adaptive_alloc(4);
    adaptive_t normalised = adaptive_alloc(4);

    adaptive_design(&plain, ADAPTIVE_PLAIN, REAL_C(0.3));
    adaptive_design(&normalised, ADAPTIVE_NORMALISED, REAL_C(0.3));

    for(uint32_t index = 0; index < 3000u; index++)
    {
        real_t reference = REAL_C(50.0) * noise();
        real_t wanted = REAL_C(2.0) * reference;

        adaptive_error(&plain, reference, wanted);
        adaptive_error(&normalised, reference, wanted);
    }

    // The normalised one learned the gain of 2.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0),
                            adaptive_get_coefficient(&normalised, 0));

    // The plain one, at the same rate, did not. It may have run away to a
    // value that is not a number at all, thus the test asks that it is NOT
    // near the answer rather than that it is near anything in particular.
    real_t ran_away = adaptive_get_coefficient(&plain, 0);
    TEST_ASSERT_FALSE(REAL_ABS(ran_away - REAL_C(2.0)) < REAL_C(0.5));

    adaptive_free(&plain);
    adaptive_free(&normalised);
}

void test_the_sign_rule_learns_without_multiplying_by_the_error(void)
{
    adaptive_t adaptive = adaptive_alloc(1);
    adaptive_design(&adaptive, ADAPTIVE_SIGN, REAL_C(0.002));

    for(uint32_t index = 0; index < 20000u; index++)
    {
        real_t reference = noise();
        adaptive_error(&adaptive, reference, REAL_C(1.5) * reference);
    }

    // It settles more slowly and never quite as close, thus the tolerance is
    // wider than for the other two.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(1.5),
                            adaptive_get_coefficient(&adaptive, 0));

    adaptive_free(&adaptive);
}

void test_adaptive_process_sample_gives_the_noise_and_the_error_gives_the_answer(void)
{
    // The two are the two halves of the same step, and the header warns which
    // one a caller wants.
    adaptive_t adaptive = adaptive_alloc(4);
    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.4));

    for(uint32_t index = 0; index < 500u; index++)
    {
        real_t reference = noise();
        adaptive_error(&adaptive, reference, REAL_C(2.0) * reference);
    }

    real_t reference = noise();
    real_t wanted = REAL_C(2.0) * reference;

    adaptive_t copy = adaptive_alloc(4);
    for(uint32_t index = 0; index < 4u; index++)
    {
        copy.coefficient[index] = adaptive.coefficient[index];
    }
    copy.rule = adaptive.rule;
    copy.rate = adaptive.rate;
    for(uint32_t index = 0; index < 4u; index++)
    {
        ringbuf_put(&copy.history, ringbuf_get(&adaptive.history, 3u - index));
    }
    copy.energy = adaptive.energy;

    real_t guess = adaptive_process_sample(&adaptive, reference, wanted);
    real_t left = adaptive_error(&copy, reference, wanted);

    // What it makes of the reference plus what is left over is what it aimed
    // at, always.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), wanted, guess + left);

    adaptive_free(&adaptive);
    adaptive_free(&copy);
}

void test_adaptive_leak_pulls_a_coefficient_back_when_the_reference_goes_quiet(void)
{
    adaptive_t adaptive = adaptive_alloc(2);
    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.5));

    for(uint32_t index = 0; index < 2000u; index++)
    {
        real_t reference = noise();
        adaptive_error(&adaptive, reference, REAL_C(3.0) * reference);
    }
    real_t learned = adaptive_get_coefficient(&adaptive, 0);
    TEST_ASSERT_TRUE(REAL_ABS(learned) > REAL_C(2.0));

    // Now the reference goes silent and a leak is set. What was learned must
    // fall away rather than stand for ever.
    TEST_ASSERT_EQUAL(true, adaptive_set_leak(&adaptive, REAL_C(0.01)));
    for(uint32_t index = 0; index < 2000u; index++)
    {
        adaptive_error(&adaptive, REAL_C(0.0), REAL_C(0.0));
    }

    TEST_ASSERT_TRUE(REAL_ABS(adaptive_get_coefficient(&adaptive, 0))
                     < REAL_C(0.1));

    adaptive_free(&adaptive);
}

void test_adaptive_set_leak_refuses_a_leak_outside_its_range(void)
{
    adaptive_t adaptive = adaptive_alloc(2);

    TEST_ASSERT_EQUAL(true, adaptive_set_leak(&adaptive, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(true, adaptive_set_leak(&adaptive, REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, adaptive_set_leak(&adaptive, REAL_C(-0.1)));
    TEST_ASSERT_EQUAL(false, adaptive_set_leak(&adaptive, REAL_C(1.1)));

    adaptive_free(&adaptive);
}

void test_adaptive_reset_forgets_everything(void)
{
    adaptive_t adaptive = adaptive_alloc(4);
    adaptive_design(&adaptive, ADAPTIVE_NORMALISED, REAL_C(0.5));

    for(uint32_t index = 0; index < 1000u; index++)
    {
        real_t reference = noise();
        adaptive_error(&adaptive, reference, REAL_C(3.0) * reference);
    }
    TEST_ASSERT_TRUE(REAL_ABS(adaptive_get_coefficient(&adaptive, 0))
                     > REAL_C(1.0));

    adaptive_reset(&adaptive);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                            adaptive_get_coefficient(&adaptive, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                            adaptive_get_energy(&adaptive));

    adaptive_free(&adaptive);
}

void test_adaptive_get_coefficient_outside_the_filter_gives_nothing(void)
{
    adaptive_t adaptive = adaptive_alloc(4);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                            adaptive_get_coefficient(&adaptive, 4));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                            adaptive_get_coefficient(&adaptive, 1000));

    adaptive_free(&adaptive);
}

// A BLOCK MUST BE THE SAMPLES ONE AT A TIME. These filters learn from every
// sample, thus a block form that ran the pass twice, or learned in a different
// order, would give a different filter at the end and a caller could not mix
// the two forms.
void test_adaptive_a_block_is_the_samples_one_at_a_time(void)
{
    const uint32_t count = 200u;
    real_t reference[200];
    real_t wanted[200];
    real_t output[200];
    real_t error[200];

    uint32_t seed = 5u;

    for(uint32_t index = 0; index < count; index++)
    {
        seed = (seed * 1103515245u) + 12345u;
        reference[index] = ((real_t)((seed >> 16u) % 2000u)
                            / REAL_C(1000.0)) - REAL_C(1.0);
        wanted[index] = (REAL_C(0.6) * reference[index])
                        - ((index > 0u) ? (REAL_C(0.3) * reference[index - 1u])
                                        : REAL_C(0.0));
    }

    adaptive_t together = adaptive_alloc(4u);
    adaptive_t apart = adaptive_alloc(4u);

    adaptive_design(&together, ADAPTIVE_NORMALISED, REAL_C(0.1));
    adaptive_design(&apart, ADAPTIVE_NORMALISED, REAL_C(0.1));

    TEST_ASSERT_EQUAL(true, adaptive_process_block(&together, reference,
                                                   wanted, output, error,
                                                   count));

    for(uint32_t index = 0; index < count; index++)
    {
        real_t made = adaptive_process_sample(&apart, reference[index],
                                              wanted[index]);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), made, output[index]);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), wanted[index] - made,
                                error[index]);
    }

    // And the two filters ended in the same place.
    for(uint32_t index = 0; index < 4u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                                adaptive_get_coefficient(&apart, index),
                                adaptive_get_coefficient(&together, index));
    }

    adaptive_free(&together);
    adaptive_free(&apart);
}

// Either answer may be left out, and leaving one out must not change the other.
void test_adaptive_a_block_takes_null_for_either_answer(void)
{
    const uint32_t count = 60u;
    real_t reference[60];
    real_t wanted[60];
    real_t error[60];
    real_t both_error[60];
    real_t output[60];

    for(uint32_t index = 0; index < count; index++)
    {
        reference[index] = REAL_SIN((real_t)index * REAL_C(0.4));
        wanted[index] = REAL_C(0.5) * reference[index];
    }

    adaptive_t one = adaptive_alloc(3u);
    adaptive_t other = adaptive_alloc(3u);

    adaptive_design(&one, ADAPTIVE_NORMALISED, REAL_C(0.1));
    adaptive_design(&other, ADAPTIVE_NORMALISED, REAL_C(0.1));

    TEST_ASSERT_EQUAL(true, adaptive_process_block(&one, reference, wanted,
                                                   NULL, error, count));
    TEST_ASSERT_EQUAL(true, adaptive_process_block(&other, reference, wanted,
                                                   output, both_error,
                                                   count));

    for(uint32_t index = 0; index < count; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), both_error[index],
                                error[index]);
    }

    adaptive_free(&one);
    adaptive_free(&other);
}
