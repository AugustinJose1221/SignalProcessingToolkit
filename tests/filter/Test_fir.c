#include "unity.h"
#include "real_assert.h"
#include "fir.h"
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
