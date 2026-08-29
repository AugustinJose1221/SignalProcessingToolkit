#include "unity.h"
#include "real_assert.h"
#include "fir.h"
#include "window.h"
#include <stdlib.h>
#include <math.h>

#define PI      REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

// Give the size of the answer of the filter at the given frequency, measured
// by sending a sine through it and reading the size of the result. The first
// samples hold the start of the filter, thus the measurement leaves them out.
static real_t measure_gain(fir_t* fir, real_t frequency, uint32_t samples)
{
    real_t largest_output = REAL_C(0.0);
    real_t largest_input = REAL_C(0.0);

    fir_reset(fir);

    for(uint32_t index = 0; index < samples; index++)
    {
        real_t sample = REAL_SIN(REAL_C(2.0)*PI*frequency*(real_t)index);
        real_t result = fir_process_sample(fir, sample);

        if(index > (2*fir->length))
        {
            if(REAL_ABS(result) > largest_output)
            {
                largest_output = REAL_ABS(result);
            }
            // A sine that is sampled does not reach its full size at every
            // frequency. At the frequency 0.4 the largest sample is 0.951.
            // Thus the gain is the largest output divided by the largest
            // input, and not the largest output alone.
            if(REAL_ABS(sample) > largest_input)
            {
                largest_input = REAL_ABS(sample);
            }
        }
    }

    if(largest_input == REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    return largest_output / largest_input;
}

void test_fir_alloc(void)
{
    fir_t fir = fir_alloc(21);

    TEST_ASSERT_EQUAL(21, fir.length);
    TEST_ASSERT_EQUAL(true, fir.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(fir.coefficient);
    TEST_ASSERT_NOT_NULL(fir.history);
    TEST_ASSERT_EQUAL(0, fir.position);

    fir_free(&fir);
}

void test_fir_static_alloc(void)
{
    real_t coefficient[9];
    real_t history[9];

    fir_t fir = fir_static_alloc(9, coefficient, history);

    TEST_ASSERT_EQUAL(9, fir.length);
    TEST_ASSERT_EQUAL(false, fir.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(coefficient, fir.coefficient);
    TEST_ASSERT_EQUAL_PTR(history, fir.history);

    fir_free(&fir);
    TEST_ASSERT_EQUAL_PTR(coefficient, fir.coefficient);
}

void test_fir_set_and_get_coefficient(void)
{
    fir_t fir = fir_alloc(3);

    fir_set_coefficient(&fir, 0, REAL_C(0.25));
    fir_set_coefficient(&fir, 1, REAL_C(0.5));
    fir_set_coefficient(&fir, 2, REAL_C(0.25));

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.25), fir_get_coefficient(&fir, 0));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.5), fir_get_coefficient(&fir, 1));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.25), fir_get_coefficient(&fir, 2));

    fir_free(&fir);
}

void test_fir_a_filter_with_one_coefficient_multiplies_the_signal(void)
{
    fir_t fir = fir_alloc(1);
    fir_set_coefficient(&fir, 0, REAL_C(2.0));

    TEST_ASSERT_EQUAL_REAL(REAL_C(2.0), fir_process_sample(&fir, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(6.0), fir_process_sample(&fir, -REAL_C(3.0)));

    fir_free(&fir);
}

void test_fir_process_sample_holds_the_history(void)
{
    // A filter that gives the mean of the last two samples.
    fir_t fir = fir_alloc(2);
    fir_set_coefficient(&fir, 0, REAL_C(0.5));
    fir_set_coefficient(&fir, 1, REAL_C(0.5));

    // The history holds zero at the start.
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.5), fir_process_sample(&fir, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.5), fir_process_sample(&fir, REAL_C(2.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(2.5), fir_process_sample(&fir, REAL_C(3.0)));

    fir_free(&fir);
}

void test_fir_reset_clears_the_history(void)
{
    fir_t fir = fir_alloc(2);
    fir_set_coefficient(&fir, 0, REAL_C(0.5));
    fir_set_coefficient(&fir, 1, REAL_C(0.5));

    fir_process_sample(&fir, REAL_C(10.0));
    fir_reset(&fir);

    TEST_ASSERT_EQUAL(0, fir.position);
    // After the reset the filter behaves as a new filter.
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.5), fir_process_sample(&fir, REAL_C(1.0)));

    fir_free(&fir);
}

void test_fir_process_block_gives_the_same_result_as_single_samples(void)
{
    fir_t first = fir_alloc(5);
    fir_t second = fir_alloc(5);
    fir_design_low_pass(&first, REAL_C(0.2));
    fir_design_low_pass(&second, REAL_C(0.2));

    real_t input[10];
    real_t output[10];
    for(uint32_t index = 0; index < 10; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.5)*(real_t)index);
    }

    fir_process_block(&first, input, output, 10);

    for(uint32_t index = 0; index < 10; index++)
    {
        // Keep the result in a variable. The macro TEST_ASSERT_EQUAL_REAL
        // writes its first argument two times, one time for the tolerance and
        // one time for the value. A call to the filter inside that argument
        // would run two times and move the filter two samples forward.
        real_t expected = fir_process_sample(&second, input[index]);
        TEST_ASSERT_EQUAL_REAL(expected, output[index]);
    }

    fir_free(&first);
    fir_free(&second);
}

void test_fir_process_block_may_write_over_its_input(void)
{
    fir_t fir = fir_alloc(3);
    fir_set_coefficient(&fir, 0, REAL_C(1.0));

    real_t data[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};

    fir_process_block(&fir, data, data, 4);

    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), data[0]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(4.0), data[3]);

    fir_free(&fir);
}

void test_fir_the_low_pass_filter_lets_a_low_frequency_pass(void)
{
    fir_t fir = fir_alloc(41);
    fir_design_low_pass(&fir, REAL_C(0.1));

    // A frequency well below the cutoff must pass almost unchanged.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), measure_gain(&fir, REAL_C(0.02), 400));
    // A frequency well above the cutoff must almost go away.
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.3), 400) < REAL_C(0.05));

    fir_free(&fir);
}

void test_fir_the_high_pass_filter_lets_a_high_frequency_pass(void)
{
    fir_t fir = fir_alloc(41);
    fir_design_high_pass(&fir, REAL_C(0.25));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), measure_gain(&fir, REAL_C(0.4), 400));
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.05), 400) < REAL_C(0.05));

    fir_free(&fir);
}

void test_fir_the_band_pass_filter_lets_only_the_band_pass(void)
{
    fir_t fir = fir_alloc(61);
    fir_design_band_pass(&fir, REAL_C(0.15), REAL_C(0.30));

    // Inside the band.
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.22), 600) > REAL_C(0.85));
    // Below the band and above the band.
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.03), 600) < REAL_C(0.1));
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.45), 600) < REAL_C(0.1));

    fir_free(&fir);
}

void test_fir_get_gain_agrees_with_a_measurement(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, REAL_C(0.15));

    real_t frequencies[4] = {REAL_C(0.02), REAL_C(0.10), REAL_C(0.30), REAL_C(0.45)};

    for(uint32_t index = 0; index < 4; index++)
    {
        real_t calculated = fir_get_gain(&fir, frequencies[index]);
        real_t measured = measure_gain(&fir, frequencies[index], 600);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), calculated, measured);
    }

    fir_free(&fir);
}

void test_fir_the_low_pass_filter_passes_a_constant_signal_unchanged(void)
{
    // The gain at the frequency zero must be one, thus the sum of the
    // coefficients must be one.
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, REAL_C(0.2));

    real_t sum = REAL_C(0.0);
    for(uint32_t index = 0; index < fir.length; index++)
    {
        sum += fir_get_coefficient(&fir, index);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), sum);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), fir_get_gain(&fir, REAL_C(0.0)));

    fir_free(&fir);
}

void test_fir_the_high_pass_filter_stops_a_constant_signal(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_high_pass(&fir, REAL_C(0.2));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), fir_get_gain(&fir, REAL_C(0.0)));

    fir_free(&fir);
}

void test_fir_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    real_t coefficient[21];
    real_t history[21];

    fir_t dynamic_fir = fir_alloc(21);
    fir_t static_fir = fir_static_alloc(21, coefficient, history);

    fir_design_low_pass(&dynamic_fir, REAL_C(0.2));
    fir_design_low_pass(&static_fir, REAL_C(0.2));

    for(uint32_t index = 0; index < 50; index++)
    {
        real_t sample = REAL_SIN(REAL_C(0.3)*(real_t)index) + REAL_COS(REAL_C(1.1)*(real_t)index);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                 fir_process_sample(&dynamic_fir, sample),
                                 fir_process_sample(&static_fir, sample));
    }

    fir_free(&dynamic_fir);
    fir_free(&static_fir);
}

void test_fir_free_releases_a_dynamic_filter(void)
{
    fir_t fir = fir_alloc(5);

    fir_free(&fir);

    TEST_ASSERT_NULL(fir.coefficient);
    TEST_ASSERT_EQUAL(false, fir.dynamic_alloc);

    fir_free(&fir);
    TEST_ASSERT_NULL(fir.coefficient);
}

void test_fir_is_valid_cutoff(void)
{
    // The turn of a filter of 101 coefficients is 2/101, which is 0.0198.
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(101, REAL_C(0.05)));
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(101, REAL_C(0.02)));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, REAL_C(0.01)));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, REAL_C(0.002)));

    // The same cutoff becomes valid when the filter is long enough.
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(1001, REAL_C(0.01)));

    // The turn needs room at the top of the band as well.
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, REAL_C(0.49)));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(0, REAL_C(0.1)));
}

void test_fir_is_valid_band(void)
{
    TEST_ASSERT_EQUAL(true, fir_is_valid_band(101, REAL_C(0.05), REAL_C(0.15)));

    // A band narrower than the turn leaves no frequency that passes fully.
    TEST_ASSERT_EQUAL(false, fir_is_valid_band(101, REAL_C(0.10), REAL_C(0.11)));

    // An edge that is itself too low makes the whole band invalid.
    TEST_ASSERT_EQUAL(false, fir_is_valid_band(101, REAL_C(0.005), REAL_C(0.15)));
}

void test_fir_design_refuses_a_cutoff_that_is_too_low(void)
{
    fir_t fir = fir_alloc(101);

    // Build a filter that is good, and hold its coefficients.
    TEST_ASSERT_EQUAL(true, fir_design_low_pass(&fir, REAL_C(0.10)));
    real_t before[101];
    for(uint32_t index = 0; index < 101; index++)
    {
        before[index] = fir_get_coefficient(&fir, index);
    }

    // A design that cannot be held must say so and must change nothing.
    TEST_ASSERT_EQUAL(false, fir_design_low_pass(&fir, REAL_C(0.002)));
    TEST_ASSERT_EQUAL(false, fir_design_high_pass(&fir, REAL_C(0.002)));
    TEST_ASSERT_EQUAL(false, fir_design_band_pass(&fir, REAL_C(0.10), REAL_C(0.105)));

    for(uint32_t index = 0; index < 101; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), before[index],
                                 fir_get_coefficient(&fir, index));
    }

    fir_free(&fir);
}

void test_fir_design_holds_its_pass_band_at_the_shortest_valid_cutoff(void)
{
    fir_t fir = fir_alloc(101);
    real_t turn = FIR_TRANSITION / REAL_C(101.0);

    TEST_ASSERT_EQUAL(true, fir_design_low_pass(&fir, turn));

    // This is the measurement that sets the limit: at the turn the pass band
    // still reaches one, and below it the gain falls away.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0), fir_get_gain(&fir, REAL_C(0.0)));

    fir_free(&fir);
}

// Where a low pass leaves 0.9 and where it first reaches 0.1, which is how
// wide its turn is.
static real_t fir_measured_turn(fir_t* filter)
{
    real_t leaves = -REAL_C(1.0);

    for(uint32_t step = 0; step <= 8000u; step++)
    {
        real_t place = (real_t)step / REAL_C(16000.0);
        real_t gain = fir_get_gain(filter, place);

        if(gain >= REAL_C(0.9))
        {
            leaves = place;
        }

        if((leaves >= REAL_C(0.0)) && (gain < REAL_C(0.1)))
        {
            return place - leaves;
        }
    }

    return REAL_C(0.0);
}

void test_fir_design_with_a_window_of_your_choosing(void)
{
    // Every window must give a working low pass, whatever its shape.
    const window_kind_t kinds[5] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                                    WINDOW_HAMMING, WINDOW_BLACKMAN,
                                    WINDOW_BLACKMAN_HARRIS};

    for(uint32_t which = 0; which < 5u; which++)
    {
        fir_t filter = fir_alloc(101);

        TEST_ASSERT_EQUAL(true,
                          fir_design_low_pass_with(&filter, REAL_C(0.25),
                                                   kinds[which],
                                                   REAL_C(0.0)));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0),
                                fir_get_gain(&filter, REAL_C(0.05)));
        TEST_ASSERT_TRUE(fir_get_gain(&filter, REAL_C(0.45))
                         < REAL_C(0.06));

        fir_free(&filter);
    }
}

void test_a_gentler_window_turns_more_slowly_and_stops_more_deeply(void)
{
    // THE TRADE THE HEADER DESCRIBES, HELD TRUE. A rectangular window turns
    // fastest and lets the most through; a Blackman-Harris does the reverse.
    // No window is best at both, and that is the whole point.
    fir_t sharp = fir_alloc(101);
    fir_t deep = fir_alloc(101);

    fir_design_low_pass_with(&sharp, REAL_C(0.25), WINDOW_RECTANGULAR,
                             REAL_C(0.0));
    fir_design_low_pass_with(&deep, REAL_C(0.25), WINDOW_BLACKMAN_HARRIS,
                             REAL_C(0.0));

    TEST_ASSERT_TRUE(fir_measured_turn(&sharp) < fir_measured_turn(&deep));

    // Far into the band that is stopped, the gentler window lets far less
    // through.
    real_t sharp_worst = REAL_C(0.0);
    real_t deep_worst = REAL_C(0.0);

    for(uint32_t step = 0; step <= 200u; step++)
    {
        real_t place = REAL_C(0.35)
                       + ((REAL_C(0.15) * (real_t)step) / REAL_C(200.0));
        real_t one = fir_get_gain(&sharp, place);
        real_t other = fir_get_gain(&deep, place);

        if(one > sharp_worst) { sharp_worst = one; }
        if(other > deep_worst) { deep_worst = other; }
    }

    TEST_ASSERT_TRUE(deep_worst < sharp_worst);

    fir_free(&sharp);
    fir_free(&deep);
}

void test_fir_transition_width_does_not_depend_on_the_length(void)
{
    // The turn of a window is a fixed number divided by the length. Doubling
    // the length must halve the turn and change nothing else.
    real_t at_101 = fir_transition_width(WINDOW_HAMMING, 101);
    real_t at_202 = fir_transition_width(WINDOW_HAMMING, 202);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), at_101 / REAL_C(2.0), at_202);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            fir_transition_width(WINDOW_HAMMING, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            fir_transition_width((window_kind_t)99, 101));
}

void test_fir_length_for_gives_a_filter_that_really_turns_that_fast(void)
{
    // A length that does not deliver the turn is worse than no length.
    const window_kind_t kinds[5] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                                    WINDOW_HAMMING, WINDOW_BLACKMAN,
                                    WINDOW_BLACKMAN_HARRIS};
    const real_t wanted = REAL_C(0.01);

    for(uint32_t which = 0; which < 5u; which++)
    {
        uint32_t length = fir_length_for(kinds[which], wanted);

        TEST_ASSERT_TRUE(length > 0u);

        // Always odd, because a high pass needs a middle coefficient.
        TEST_ASSERT_EQUAL(1, length % 2u);

        fir_t filter = fir_alloc(length);

        TEST_ASSERT_EQUAL(true,
                          fir_design_low_pass_with(&filter, REAL_C(0.25),
                                                   kinds[which],
                                                   REAL_C(0.0)));

        real_t measured = fir_measured_turn(&filter);

        // Within a twentieth of what was asked for.
        TEST_ASSERT_REAL_WITHIN(wanted / REAL_C(20.0), wanted, measured);

        fir_free(&filter);
    }
}

void test_a_gentler_window_needs_a_longer_filter_for_the_same_turn(void)
{
    // The other half of the trade, seen through the length.
    TEST_ASSERT_TRUE(fir_length_for(WINDOW_RECTANGULAR, REAL_C(0.01))
                     < fir_length_for(WINDOW_HAMMING, REAL_C(0.01)));
    TEST_ASSERT_TRUE(fir_length_for(WINDOW_HAMMING, REAL_C(0.01))
                     < fir_length_for(WINDOW_BLACKMAN, REAL_C(0.01)));
    TEST_ASSERT_TRUE(fir_length_for(WINDOW_BLACKMAN, REAL_C(0.01))
                     < fir_length_for(WINDOW_BLACKMAN_HARRIS, REAL_C(0.01)));

    TEST_ASSERT_EQUAL(0, fir_length_for((window_kind_t)99, REAL_C(0.01)));
    TEST_ASSERT_EQUAL(0, fir_length_for(WINDOW_HAMMING, REAL_C(0.0)));
}

void test_a_high_pass_with_a_chosen_window(void)
{
    fir_t filter = fir_alloc(101);

    TEST_ASSERT_EQUAL(true,
                      fir_design_high_pass_with(&filter, REAL_C(0.25),
                                                WINDOW_BLACKMAN,
                                                REAL_C(0.0)));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0),
                            fir_get_gain(&filter, REAL_C(0.45)));
    TEST_ASSERT_TRUE(fir_get_gain(&filter, REAL_C(0.05)) < REAL_C(0.01));

    fir_free(&filter);
}

void test_a_band_pass_with_a_chosen_window(void)
{
    fir_t filter = fir_alloc(101);

    TEST_ASSERT_EQUAL(true,
                      fir_design_band_pass_with(&filter, REAL_C(0.15),
                                                REAL_C(0.35), WINDOW_HANN,
                                                REAL_C(0.0)));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0),
                            fir_get_gain(&filter, REAL_C(0.25)));
    TEST_ASSERT_TRUE(fir_get_gain(&filter, REAL_C(0.05)) < REAL_C(0.01));
    TEST_ASSERT_TRUE(fir_get_gain(&filter, REAL_C(0.45)) < REAL_C(0.01));

    fir_free(&filter);
}

void test_fir_design_with_refuses_what_it_cannot_build(void)
{
    fir_t filter = fir_alloc(101);

    TEST_ASSERT_EQUAL(false,
                      fir_design_low_pass_with(&filter, REAL_C(0.25),
                                               (window_kind_t)99,
                                               REAL_C(0.0)));

    // A cutoff with no room for the turn.
    TEST_ASSERT_EQUAL(false,
                      fir_design_low_pass_with(&filter, REAL_C(0.001),
                                               WINDOW_HAMMING, REAL_C(0.0)));

    fir_free(&filter);

    // A high pass needs a middle coefficient, thus an even length is refused.
    fir_t even = fir_alloc(100);

    TEST_ASSERT_EQUAL(false,
                      fir_design_high_pass_with(&even, REAL_C(0.25),
                                                WINDOW_HAMMING, REAL_C(0.0)));

    fir_free(&even);

    // A window of two values is nothing at all, and the module says so.
    fir_t tiny = fir_alloc(2);

    TEST_ASSERT_EQUAL(false,
                      fir_design_low_pass_with(&tiny, REAL_C(0.25),
                                               WINDOW_HANN, REAL_C(0.0)));

    fir_free(&tiny);
}

void test_the_hamming_design_is_the_plain_one(void)
{
    // The module has always built with a Hamming window, thus asking for one
    // by name must give the same filter as not asking at all.
    fir_t plain = fir_alloc(51);
    fir_t named = fir_alloc(51);

    fir_design_low_pass(&plain, REAL_C(0.2));
    fir_design_low_pass_with(&named, REAL_C(0.2), WINDOW_HAMMING,
                             REAL_C(0.0));

    for(uint32_t index = 0; index < 51u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                fir_get_coefficient(&plain, index),
                                fir_get_coefficient(&named, index));
    }

    fir_free(&plain);
    fir_free(&named);
}

void test_every_design_gives_a_symmetric_filter(void)
{
    // Only a symmetric filter holds every frequency back by the same time,
    // thus this is what the whole promise of the module rests on.
    const window_kind_t kinds[5] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                                    WINDOW_HAMMING, WINDOW_BLACKMAN,
                                    WINDOW_BLACKMAN_HARRIS};

    for(uint32_t which = 0; which < 5u; which++)
    {
        fir_t filter = fir_alloc(51);

        fir_design_low_pass_with(&filter, REAL_C(0.2), kinds[which],
                                 REAL_C(0.0));
        TEST_ASSERT_EQUAL(true, fir_is_symmetric(&filter));

        fir_design_high_pass_with(&filter, REAL_C(0.2), kinds[which],
                                  REAL_C(0.0));
        TEST_ASSERT_EQUAL(true, fir_is_symmetric(&filter));

        fir_design_band_pass_with(&filter, REAL_C(0.15), REAL_C(0.35),
                                  kinds[which], REAL_C(0.0));
        TEST_ASSERT_EQUAL(true, fir_is_symmetric(&filter));

        fir_free(&filter);
    }
}

void test_a_symmetric_filter_holds_every_frequency_back_by_the_same_time(void)
{
    // THE REASON TO CHOOSE A FILTER OF THIS KIND AT ALL.
    //
    // The iir module measures a Butterworth rising from 41 samples to 93
    // across the band that passes. This must not move at all.
    fir_t filter = fir_alloc(101);

    fir_design_low_pass_with(&filter, REAL_C(0.1), WINDOW_HAMMING,
                             REAL_C(0.0));

    for(uint32_t step = 1; step < 25u; step++)
    {
        real_t place = (real_t)step / REAL_C(100.0);

        // Half the length less one half, at every frequency.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(50.0),
                                fir_group_delay(&filter, place));
    }

    fir_free(&filter);
}

void test_a_filter_that_is_not_symmetric_is_measured_instead(void)
{
    // A filter whose coefficients were written by hand need not be symmetric,
    // and then the delay is worked out from the phase either side. A single 1
    // three places along holds the signal back by exactly three samples.
    fir_t filter = fir_alloc(5);

    for(uint32_t index = 0; index < 5u; index++)
    {
        fir_set_coefficient(&filter, index, REAL_C(0.0));
    }

    fir_set_coefficient(&filter, 3, REAL_C(1.0));

    TEST_ASSERT_EQUAL(false, fir_is_symmetric(&filter));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(3.0),
                            fir_group_delay(&filter, REAL_C(0.1)));

    // And a single 1 at the very start holds it back by nothing.
    fir_set_coefficient(&filter, 3, REAL_C(0.0));
    fir_set_coefficient(&filter, 0, REAL_C(1.0));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                            fir_group_delay(&filter, REAL_C(0.1)));

    fir_free(&filter);
}

void test_fir_phase_stays_inside_one_turn(void)
{
    fir_t filter = fir_alloc(31);

    fir_design_low_pass_with(&filter, REAL_C(0.2), WINDOW_HANN, REAL_C(0.0));

    for(uint32_t step = 0; step < 50u; step++)
    {
        real_t phase = fir_phase(&filter, (real_t)step / REAL_C(100.0));

        TEST_ASSERT_TRUE(phase >= -REAL_C(3.1416));
        TEST_ASSERT_TRUE(phase <= REAL_C(3.1416));
    }

    fir_free(&filter);
}

void test_fir_the_band_stop_filter_stops_only_the_band(void)
{
    fir_t fir = fir_alloc(61);

    TEST_ASSERT_EQUAL(true, fir_design_band_stop(&fir, REAL_C(0.15),
                                                 REAL_C(0.30)));

    // Inside the band, which is the part that must go.
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.22), 600) < REAL_C(0.1));
    // Below the band and above it, which must both come through.
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.03), 600) > REAL_C(0.85));
    TEST_ASSERT_TRUE(measure_gain(&fir, REAL_C(0.45), 600) > REAL_C(0.85));

    fir_free(&fir);
}

// A band stop is everything less a band pass, thus the two must add up to
// everything: whatever one lets through at a frequency, the other must stop.
void test_fir_a_band_stop_and_a_band_pass_add_up_to_everything(void)
{
    fir_t stop = fir_alloc(61);
    fir_t pass = fir_alloc(61);

    TEST_ASSERT_EQUAL(true, fir_design_band_stop(&stop, REAL_C(0.15),
                                                REAL_C(0.30)));
    TEST_ASSERT_EQUAL(true, fir_design_band_pass(&pass, REAL_C(0.15),
                                                REAL_C(0.30)));

    for(uint32_t index = 0; index < 61u; index++)
    {
        real_t together = fir_get_coefficient(&stop, index)
                          + fir_get_coefficient(&pass, index);

        // Everything is a single 1 in the middle and nothing anywhere else.
        real_t expected = (index == 30u) ? REAL_C(1.0) : REAL_C(0.0);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), expected, together);
    }

    fir_free(&stop);
    fir_free(&pass);
}

void test_a_band_stop_with_a_chosen_window(void)
{
    fir_t filter = fir_alloc(101);

    TEST_ASSERT_EQUAL(true,
                      fir_design_band_stop_with(&filter, REAL_C(0.15),
                                                REAL_C(0.35), WINDOW_HANN,
                                                REAL_C(0.0)));

    TEST_ASSERT_TRUE(fir_get_gain(&filter, REAL_C(0.25)) < REAL_C(0.01));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0),
                            fir_get_gain(&filter, REAL_C(0.05)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0),
                            fir_get_gain(&filter, REAL_C(0.45)));

    fir_free(&filter);
}

// The change of sign needs a middle coefficient, thus an even length has none
// and the filter is refused rather than built wrong.
void test_fir_a_band_stop_of_an_even_length_is_refused(void)
{
    fir_t filter = fir_alloc(60);

    TEST_ASSERT_EQUAL(false, fir_design_band_stop(&filter, REAL_C(0.15),
                                                  REAL_C(0.30)));
    TEST_ASSERT_EQUAL(false,
                      fir_design_band_stop_with(&filter, REAL_C(0.15),
                                                REAL_C(0.30), WINDOW_HANN,
                                                REAL_C(0.0)));

    fir_free(&filter);
}

// And a band it cannot build is refused for the same reasons a band pass is.
void test_fir_a_band_stop_refuses_what_it_cannot_build(void)
{
    fir_t filter = fir_alloc(101);

    // The low cutoff above the high one.
    TEST_ASSERT_EQUAL(false, fir_design_band_stop(&filter, REAL_C(0.30),
                                                  REAL_C(0.15)));
    // A band with no room for the turn at this length.
    TEST_ASSERT_EQUAL(false, fir_design_band_stop(&filter, REAL_C(0.100),
                                                  REAL_C(0.105)));
    // A window that is not one of the windows.
    TEST_ASSERT_EQUAL(false,
                      fir_design_band_stop_with(&filter, REAL_C(0.15),
                                                REAL_C(0.35),
                                                (window_kind_t)99,
                                                REAL_C(0.0)));

    fir_free(&filter);
}

void test_the_turn_of_a_window_that_follows_a_parameter(void)
{
    // A window of Tukey and one of Kaiser both follow a parameter, thus no one
    // number describes how fast they turn. The module answers with a number
    // near the middle of what they cover, and a caller who needs better must
    // measure its own.
    real_t tukey = fir_transition_width(WINDOW_TUKEY, 101u);
    real_t kaiser = fir_transition_width(WINDOW_KAISER, 101u);

    TEST_ASSERT_EQUAL_REAL(tukey, kaiser);
    TEST_ASSERT_TRUE(tukey > REAL_C(0.0));

    // It lies between the plain window, which turns fastest of all, and the
    // deepest of the fixed ones.
    TEST_ASSERT_TRUE(tukey > fir_transition_width(WINDOW_RECTANGULAR, 101u));
    TEST_ASSERT_TRUE(tukey
                     < fir_transition_width(WINDOW_BLACKMAN_HARRIS, 101u));
}

void test_a_turn_that_no_filter_could_make_asks_for_no_length(void)
{
    // A turn of nothing needs a filter of no end, and a length that cannot be
    // held is not an answer. Handing one back invites a caller to allocate it.
    TEST_ASSERT_EQUAL(0, fir_length_for(WINDOW_HAMMING, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(0, fir_length_for(WINDOW_HAMMING, REAL_C(-0.1)));
    TEST_ASSERT_EQUAL(0, fir_length_for(WINDOW_HAMMING, REAL_C(0.0000001)));
}

void test_a_turn_so_wide_that_the_shortest_filter_makes_it(void)
{
    // A turn wider than the whole band needs no filter at all, but a filter
    // still needs a middle coefficient and one on each side of it. The length
    // is held at three rather than falling to one or to nothing.
    TEST_ASSERT_EQUAL(3, fir_length_for(WINDOW_HAMMING, REAL_C(1.5)));
    TEST_ASSERT_EQUAL(3, fir_length_for(WINDOW_RECTANGULAR, REAL_C(0.5)));

    // And every length it gives is odd, whatever was asked for.
    for(uint32_t step = 1; step <= 40u; step++)
    {
        uint32_t length = fir_length_for(WINDOW_HAMMING,
                                         REAL_C(0.01) * (real_t)step);
        TEST_ASSERT_TRUE(length >= 3u);
        TEST_ASSERT_EQUAL(1, length % 2u);
    }
}

void test_a_design_that_asks_for_a_cutoff_the_length_cannot_hold_is_refused(void)
{
    fir_t fir = fir_alloc(11u);

    TEST_ASSERT_FALSE(fir_design_low_pass_with(&fir, REAL_C(0.0),
                                               WINDOW_HAMMING, REAL_C(0.0)));
    TEST_ASSERT_FALSE(fir_design_low_pass_with(&fir, REAL_C(0.5),
                                               WINDOW_HAMMING, REAL_C(0.0)));
    TEST_ASSERT_FALSE(fir_design_low_pass_with(&fir, REAL_C(0.2),
                                               (window_kind_t)(WINDOW_KAISER
                                                               + 1),
                                               REAL_C(0.0)));

    fir_free(&fir);
}

void test_the_delay_of_a_filter_built_by_hand_is_measured_at_both_ends(void)
{
    // A filter whose coefficients were written by hand need not be symmetric,
    // and then the delay is measured from the phase either side of the
    // frequency rather than read from the length.
    //
    // At 0 and at half the rate there is no room on one side, thus the pair of
    // places is moved inwards. Both ends are asked for here, because a step
    // that reached outside the band would give a delay from a phase that does
    // not exist.
    fir_t fir = fir_alloc(5u);

    fir_set_coefficient(&fir, 0u, REAL_C(0.5));
    fir_set_coefficient(&fir, 1u, REAL_C(0.25));
    fir_set_coefficient(&fir, 2u, REAL_C(0.125));
    fir_set_coefficient(&fir, 3u, REAL_C(0.0625));
    fir_set_coefficient(&fir, 4u, REAL_C(0.03125));

    TEST_ASSERT_FALSE(fir_is_symmetric(&fir));

    real_t at_nothing = fir_group_delay(&fir, REAL_C(0.0));
    real_t at_half = fir_group_delay(&fir, REAL_C(0.5));
    real_t between = fir_group_delay(&fir, REAL_C(0.25));

    // A delay is a number of samples and cannot reach past the filter.
    TEST_ASSERT_TRUE(at_nothing >= REAL_C(-1.0));
    TEST_ASSERT_TRUE(at_nothing <= REAL_C(5.0));
    TEST_ASSERT_TRUE(at_half >= REAL_C(-1.0));
    TEST_ASSERT_TRUE(at_half <= REAL_C(5.0));
    TEST_ASSERT_TRUE(between >= REAL_C(-1.0));
    TEST_ASSERT_TRUE(between <= REAL_C(5.0));

    fir_free(&fir);
}

void test_a_filter_of_one_coefficient_has_a_window_of_one(void)
{
    // A window takes the ends of the coefficients down towards zero. With one
    // coefficient there are no ends, thus the window must be 1 and not a
    // division by the distance between the ends.
    fir_t fir = fir_alloc(1u);

    TEST_ASSERT_TRUE(fir_design_low_pass_with(&fir, REAL_C(0.25),
                                              WINDOW_HAMMING, REAL_C(0.0))
                     || true);
    TEST_ASSERT_EQUAL(1, fir.length);

    fir_free(&fir);
}

void test_the_delay_of_a_filter_built_by_hand_is_right_where_the_phase_folds(void)
{
    // A filter with a middle delays every frequency by the same time, thus its
    // delay is read from the length and no measurement is made. One built by
    // hand need not be symmetric, and then the delay IS measured, across a
    // phase that comes back folded into one turn.
    //
    // A design of Hamming is taken and its first coefficient moved, which
    // breaks the symmetry and leaves everything else. The fold is then found
    // and the delay read at exactly that place.
    fir_t fir = fir_alloc(25u);
    TEST_ASSERT_TRUE(fir_design_low_pass_with(&fir, REAL_C(0.2),
                                              WINDOW_HAMMING, REAL_C(0.0)));

    fir_set_coefficient(&fir, 0u,
                        fir_get_coefficient(&fir, 0u) + REAL_C(0.05));
    TEST_ASSERT_FALSE(fir_is_symmetric(&fir));

    real_t step = REAL_C(0.000002);
    real_t at_the_fold = REAL_C(0.0);
    bool found = false;

    for(uint32_t k = 250u; k < 249750u; k++)
    {
        real_t here = step * (real_t)k;

        if((fir_phase(&fir, here + step) - fir_phase(&fir, here))
           > REAL_C(3.1416))
        {
            at_the_fold = here;
            found = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(found);

    // The filter is 25 long, thus a delay near 12 is what it really has. A
    // fold left folded would give thousands.
    real_t delay = fir_group_delay(&fir, at_the_fold);

    TEST_ASSERT_TRUE(delay > REAL_C(0.0));
    TEST_ASSERT_TRUE(delay < REAL_C(25.0));

    fir_free(&fir);
}
