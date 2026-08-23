#include "unity.h"
#include "real_assert.h"
#include "iir.h"
#include <stdlib.h>
#include <math.h>

#define PI      REAL_C(3.14159265358979323846)

// A cutoff that no build can hold, whatever its width. Taking it from the
// limit itself keeps these tests true at both widths: a number written here by
// hand would be too low at 32 bits and perfectly good at 64.
#define TOO_LOW     (IIR_MIN_CUTOFF / REAL_C(10.0))

void setUp(void)
{

}

void tearDown(void)
{

}

// Measure the size of the answer of the filter at the given frequency. The
// first samples hold the start of the filter, thus the measurement leaves
// them out.
static real_t measure_gain(iir_t* iir, real_t frequency)
{
    const uint32_t samples = 2000;
    real_t largest_output = REAL_C(0.0);
    real_t largest_input = REAL_C(0.0);

    iir_reset(iir);

    for(uint32_t index = 0; index < samples; index++)
    {
        real_t sample = REAL_SIN(REAL_C(2.0)*PI*frequency*(real_t)index);
        real_t result = iir_process_sample(iir, sample);

        if(index > (samples/2))
        {
            if(REAL_ABS(result) > largest_output)
            {
                largest_output = REAL_ABS(result);
            }
            // A sine that is sampled does not reach its full size at every
            // frequency, thus the gain is the largest output divided by the
            // largest input.
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

void test_iir_alloc(void)
{
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL(2, iir.sections);
    TEST_ASSERT_EQUAL(true, iir.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(iir.coefficient);
    TEST_ASSERT_NOT_NULL(iir.state);

    iir_free(&iir);
}

void test_iir_static_alloc(void)
{
    real_t coefficient[IIR_COEFFICIENT_SIZE(2)];
    real_t state[IIR_STATE_SIZE(2)];

    iir_t iir = iir_static_alloc(2, coefficient, state);

    TEST_ASSERT_EQUAL(2, iir.sections);
    TEST_ASSERT_EQUAL(false, iir.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(coefficient, iir.coefficient);
    TEST_ASSERT_EQUAL_PTR(state, iir.state);

    iir_free(&iir);
    TEST_ASSERT_EQUAL_PTR(coefficient, iir.coefficient);
}

void test_iir_a_new_filter_lets_everything_pass(void)
{
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), iir_process_sample(&iir, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(4.5), iir_process_sample(&iir, -REAL_C(4.5)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.25)));

    iir_free(&iir);
}

void test_iir_set_section_divides_by_the_first_coefficient(void)
{
    iir_t iir = iir_alloc(1);

    // Give the coefficients with a0 of 2. The filter must divide every
    // coefficient by that value.
    iir_set_section(&iir, 0, REAL_C(2.0), REAL_C(0.0), REAL_C(0.0), REAL_C(2.0), REAL_C(0.0), REAL_C(0.0));

    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), iir.coefficient[0]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(5.0), iir_process_sample(&iir, REAL_C(5.0)));

    iir_free(&iir);
}

void test_iir_the_low_pass_filter_lets_a_low_frequency_pass(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.1));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), measure_gain(&iir, REAL_C(0.01)));
    TEST_ASSERT_TRUE(measure_gain(&iir, REAL_C(0.35)) < REAL_C(0.05));

    iir_free(&iir);
}

void test_iir_the_high_pass_filter_lets_a_high_frequency_pass(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_high_pass(&iir, REAL_C(0.25));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), measure_gain(&iir, REAL_C(0.45)));
    TEST_ASSERT_TRUE(measure_gain(&iir, REAL_C(0.05)) < REAL_C(0.05));

    iir_free(&iir);
}

void test_iir_the_gain_at_the_cutoff_is_the_half_power_point(void)
{
    // A filter of Butterworth gives the gain 1/sqrt(2) at its cutoff, which is
    // about 0.7071, for every order.
    real_t expected = REAL_C(1.0) / REAL_SQRT(REAL_C(2.0));

    for(uint32_t sections = 1; sections <= 3; sections++)
    {
        iir_t low = iir_alloc(sections);
        iir_design_low_pass(&low, REAL_C(0.2));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), expected, iir_get_gain(&low, REAL_C(0.2)));
        iir_free(&low);

        iir_t high = iir_alloc(sections);
        iir_design_high_pass(&high, REAL_C(0.2));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), expected, iir_get_gain(&high, REAL_C(0.2)));
        iir_free(&high);
    }
}

void test_iir_the_low_pass_filter_passes_a_constant_signal_unchanged(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.15));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.0)));

    // Send a signal that does not change through the filter. After the start
    // the output must be the same as the input.
    for(uint32_t index = 0; index < 500; index++)
    {
        iir_process_sample(&iir, REAL_C(3.0));
    }
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(3.0), iir_process_sample(&iir, REAL_C(3.0)));

    iir_free(&iir);
}

void test_iir_the_high_pass_filter_stops_a_constant_signal(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_high_pass(&iir, REAL_C(0.15));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), iir_get_gain(&iir, REAL_C(0.0)));

    for(uint32_t index = 0; index < 500; index++)
    {
        iir_process_sample(&iir, REAL_C(3.0));
    }
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), iir_process_sample(&iir, REAL_C(3.0)));

    iir_free(&iir);
}

void test_iir_a_filter_of_a_higher_order_gives_a_sharper_edge(void)
{
    // Well above the cutoff, a filter with more sections must let less
    // through.
    iir_t small = iir_alloc(1);
    iir_t large = iir_alloc(3);
    iir_design_low_pass(&small, REAL_C(0.1));
    iir_design_low_pass(&large, REAL_C(0.1));

    real_t small_gain = iir_get_gain(&small, REAL_C(0.25));
    real_t large_gain = iir_get_gain(&large, REAL_C(0.25));

    TEST_ASSERT_TRUE(large_gain < small_gain);

    iir_free(&small);
    iir_free(&large);
}

void test_iir_get_gain_agrees_with_a_measurement(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.12));

    real_t frequencies[4] = {REAL_C(0.02), REAL_C(0.08), REAL_C(0.20), REAL_C(0.40)};

    for(uint32_t index = 0; index < 4; index++)
    {
        real_t calculated = iir_get_gain(&iir, frequencies[index]);
        real_t measured = measure_gain(&iir, frequencies[index]);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), calculated, measured);
    }

    iir_free(&iir);
}

void test_iir_the_filter_stays_stable(void)
{
    // A filter with feedback can run away if its coefficients are bad. Send a
    // long signal through it and examine that the output stays small.
    iir_t iir = iir_alloc(3);
    iir_design_low_pass(&iir, REAL_C(0.05));

    for(uint32_t index = 0; index < 5000; index++)
    {
        real_t sample = REAL_SIN(REAL_C(0.01)*(real_t)index) + REAL_SIN(REAL_C(2.0)*(real_t)index);
        real_t result = iir_process_sample(&iir, sample);

        TEST_ASSERT_TRUE(REAL_ABS(result) < REAL_C(10.0));
        TEST_ASSERT_FALSE(isnan(result));
    }

    iir_free(&iir);
}

void test_iir_reset_clears_the_state(void)
{
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.2));

    for(uint32_t index = 0; index < 100; index++)
    {
        iir_process_sample(&iir, REAL_C(5.0));
    }

    iir_reset(&iir);

    for(uint32_t index = 0; index < IIR_STATE_SIZE(1); index++)
    {
        TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), iir.state[index]);
    }

    iir_free(&iir);
}

void test_iir_process_block_gives_the_same_result_as_single_samples(void)
{
    iir_t first = iir_alloc(2);
    iir_t second = iir_alloc(2);
    iir_design_low_pass(&first, REAL_C(0.2));
    iir_design_low_pass(&second, REAL_C(0.2));

    real_t input[20];
    real_t output[20];
    for(uint32_t index = 0; index < 20; index++)
    {
        input[index] = REAL_SIN(REAL_C(0.4)*(real_t)index);
    }

    iir_process_block(&first, input, output, 20);

    for(uint32_t index = 0; index < 20; index++)
    {
        // Keep the result in a variable. The macro TEST_ASSERT_EQUAL_REAL
        // writes its first argument two times, thus a call to the filter
        // inside that argument would move the filter two samples forward.
        real_t expected = iir_process_sample(&second, input[index]);
        TEST_ASSERT_EQUAL_REAL(expected, output[index]);
    }

    iir_free(&first);
    iir_free(&second);
}

void test_iir_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    real_t coefficient[IIR_COEFFICIENT_SIZE(2)];
    real_t state[IIR_STATE_SIZE(2)];

    iir_t dynamic_iir = iir_alloc(2);
    iir_t static_iir = iir_static_alloc(2, coefficient, state);

    iir_design_high_pass(&dynamic_iir, REAL_C(0.3));
    iir_design_high_pass(&static_iir, REAL_C(0.3));

    for(uint32_t index = 0; index < 100; index++)
    {
        real_t sample = REAL_SIN(REAL_C(0.2)*(real_t)index) + REAL_C(0.5);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                 iir_process_sample(&dynamic_iir, sample),
                                 iir_process_sample(&static_iir, sample));
    }

    iir_free(&dynamic_iir);
    iir_free(&static_iir);
}

void test_iir_free_releases_a_dynamic_filter(void)
{
    iir_t iir = iir_alloc(2);

    iir_free(&iir);

    TEST_ASSERT_NULL(iir.coefficient);
    TEST_ASSERT_EQUAL(false, iir.dynamic_alloc);

    iir_free(&iir);
    TEST_ASSERT_NULL(iir.coefficient);
}

void test_iir_is_valid_cutoff(void)
{
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(REAL_C(0.1)));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(REAL_C(0.01)));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(IIR_MIN_CUTOFF));

    // At and above half the sample rate there is nothing to pass.
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.5)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.6)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.0)));
}

void test_the_limit_of_the_cutoff_follows_the_width_of_the_build(void)
{
    // The limit is not one number. It is what the digits of the build can
    // carry, and a wider build carries a lower cutoff.
#if defined(SPTK_REAL_64)
    // A thousand times lower. A high pass at 0.5 Hz against 32 kHz is a cutoff
    // of 0.000016, which is out of reach at 32 bits and easy here.
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(REAL_C(0.000016)));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(REAL_C(0.000001)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.0000001)));
#else
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.000016)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(REAL_C(0.0001)));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(REAL_C(0.001)));
#endif
}

void test_a_design_at_the_limit_still_gives_the_right_gain(void)
{
    // The limit is set where the answer is still right. This holds it at
    // whichever width the build has, thus neither limit can drift away from
    // what the arithmetic really gives.
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL(true, iir_design_low_pass(&iir, IIR_MIN_CUTOFF));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0),
                            iir_get_gain(&iir, REAL_C(0.0)));

    iir_free(&iir);
}

void test_iir_design_refuses_a_cutoff_that_is_too_low(void)
{
    iir_t iir = iir_alloc(2);

    // Build a filter that is good, and hold what it does.
    TEST_ASSERT_EQUAL(true, iir_design_low_pass(&iir, REAL_C(0.05)));
    real_t before = iir_get_gain(&iir, REAL_C(0.02));

    // A design that cannot be held must say so and must change nothing.
    TEST_ASSERT_EQUAL(false, iir_design_low_pass(&iir, TOO_LOW));
    TEST_ASSERT_EQUAL(false, iir_design_high_pass(&iir, TOO_LOW));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), before, iir_get_gain(&iir, REAL_C(0.02)));

    iir_free(&iir);
}


void test_iir_design_notch_stops_one_frequency_and_passes_the_rest(void)
{
    // The hum of the mains at 50 Hz against a sample rate of 1000 Hz.
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_notch(&iir, REAL_C(0.05), REAL_C(30.0)));

    // Nothing passes at the frequency itself.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), iir_get_gain(&iir, REAL_C(0.05)));

    // Everything else passes untouched, close by and far away.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.04)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.06)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.001)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.4)));

    iir_free(&iir);
}

void test_iir_design_notch_gets_narrower_as_the_quality_rises(void)
{
    iir_t wide = iir_alloc(1);
    iir_t narrow = iir_alloc(1);

    iir_design_notch(&wide, REAL_C(0.05), REAL_C(5.0));
    iir_design_notch(&narrow, REAL_C(0.05), REAL_C(50.0));

    // Both stop the frequency itself.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), iir_get_gain(&wide, REAL_C(0.05)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), iir_get_gain(&narrow, REAL_C(0.05)));

    // A little away from it, the narrow one has already let go.
    TEST_ASSERT_TRUE(iir_get_gain(&narrow, REAL_C(0.055)) > iir_get_gain(&wide, REAL_C(0.055)));

    iir_free(&wide);
    iir_free(&narrow);
}

void test_iir_design_notch_refuses_what_it_cannot_hold(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, TOO_LOW, REAL_C(30.0)));
    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, REAL_C(0.05), REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, REAL_C(0.05), -REAL_C(1.0)));

    iir_free(&iir);
}

void test_iir_design_peak_passes_one_frequency_and_stops_the_rest(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_peak(&iir, REAL_C(0.05), REAL_C(30.0)));

    // The gain is one at the frequency, however narrow the band is.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.05)));

    // Everything else falls away on both sides.
    TEST_ASSERT_TRUE(iir_get_gain(&iir, REAL_C(0.01)) < REAL_C(0.05));
    TEST_ASSERT_TRUE(iir_get_gain(&iir, REAL_C(0.1)) < REAL_C(0.05));
    TEST_ASSERT_TRUE(iir_get_gain(&iir, REAL_C(0.4)) < REAL_C(0.05));

    iir_free(&iir);
}

void test_iir_design_peak_and_notch_are_two_sides_of_the_same_thing(void)
{
    // They share their poles and differ in their zeros, thus what one passes
    // the other stops. Their gains squared must add to about one.
    iir_t notch = iir_alloc(1);
    iir_t peak = iir_alloc(1);

    iir_design_notch(&notch, REAL_C(0.05), REAL_C(20.0));
    iir_design_peak(&peak, REAL_C(0.05), REAL_C(20.0));

    real_t test[4] = {REAL_C(0.02), REAL_C(0.045), REAL_C(0.05), REAL_C(0.09)};

    for(uint32_t index = 0; index < 4u; index++)
    {
        real_t one = iir_get_gain(&notch, test[index]);
        real_t other = iir_get_gain(&peak, test[index]);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0), (one * one) + (other * other));
    }

    iir_free(&notch);
    iir_free(&peak);
}

void test_iir_design_band_pass(void)
{
    iir_t iir = iir_alloc(4);

    TEST_ASSERT_EQUAL(true, iir_design_band_pass(&iir, REAL_C(0.05), REAL_C(0.20)));

    // The gain is one in the middle of the band.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.10)));

    // Each cutoff stands where the gain is the root of a half, which is what
    // a cutoff means.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.7071), iir_get_gain(&iir, REAL_C(0.05)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.7071), iir_get_gain(&iir, REAL_C(0.20)));

    // Well outside the band, almost nothing passes.
    TEST_ASSERT_TRUE(iir_get_gain(&iir, REAL_C(0.005)) < REAL_C(0.01));
    TEST_ASSERT_TRUE(iir_get_gain(&iir, REAL_C(0.45)) < REAL_C(0.01));

    iir_free(&iir);
}

void test_iir_design_band_pass_needs_an_even_number_of_sections(void)
{
    // Half of the sections make each edge, thus an odd number cannot be
    // shared and the design must say so rather than build something else.
    iir_t odd = iir_alloc(3);
    iir_t even = iir_alloc(2);

    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&odd, REAL_C(0.05), REAL_C(0.20)));
    TEST_ASSERT_EQUAL(true, iir_design_band_pass(&even, REAL_C(0.05), REAL_C(0.20)));

    iir_free(&odd);
    iir_free(&even);
}

void test_iir_design_band_pass_refuses_a_band_that_is_not_a_band(void)
{
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, REAL_C(0.20), REAL_C(0.05)));
    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, REAL_C(0.05), REAL_C(0.05)));
    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, TOO_LOW, REAL_C(0.20)));

    iir_free(&iir);
}

void test_iir_design_band_stop(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_band_stop(&iir, REAL_C(0.04), REAL_C(0.06)));

    // The middle of the band is the GEOMETRIC mean of the two edges, which is
    // 0.049 and not 0.05. A filter of this kind is symmetric in the ratio of
    // the frequencies and not in their difference.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), iir_get_gain(&iir, REAL_C(0.049)));

    // Each edge stands where the gain is the root of a half.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.7071), iir_get_gain(&iir, REAL_C(0.04)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.7071), iir_get_gain(&iir, REAL_C(0.06)));

    // Well away from the band, everything passes.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.001)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(1.0), iir_get_gain(&iir, REAL_C(0.4)));

    iir_free(&iir);
}

void test_iir_design_band_stop_refuses_a_band_that_is_not_a_band(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(false, iir_design_band_stop(&iir, REAL_C(0.06), REAL_C(0.04)));
    TEST_ASSERT_EQUAL(false, iir_design_band_stop(&iir, TOO_LOW, REAL_C(0.06)));

    iir_free(&iir);
}

void test_iir_notch_takes_a_hum_out_of_a_signal(void)
{
    // The whole point, worked through on a signal: a slow wave that must be
    // kept, with a hum at 50 Hz three times as large that must go.
    //
    // The test measures energy and not each sample. A filter shifts the phase
    // of what it passes, thus the wave comes out a little later than it went
    // in, and comparing sample against sample would measure that shift and not
    // the hum.
    iir_t iir = iir_alloc(1);
    iir_design_notch(&iir, REAL_C(0.05), REAL_C(30.0));

    real_t wanted_energy = REAL_C(0.0);
    real_t mixed_energy = REAL_C(0.0);
    real_t result_energy = REAL_C(0.0);
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 4000u; index++)
    {
        real_t time = (real_t)index;
        real_t wanted = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.005) * time);
        real_t mixed = wanted + (REAL_C(3.0) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.05) * time));

        real_t result = iir_process_sample(&iir, mixed);

        // Look only after the filter has settled.
        if(index > 1000u)
        {
            wanted_energy += wanted * wanted;
            mixed_energy += mixed * mixed;
            result_energy += result * result;
            counted++;
        }
    }

    real_t wanted_rms = REAL_SQRT(wanted_energy / (real_t)counted);
    real_t mixed_rms = REAL_SQRT(mixed_energy / (real_t)counted);
    real_t result_rms = REAL_SQRT(result_energy / (real_t)counted);

    // What went in holds far more energy than the wave alone.
    TEST_ASSERT_TRUE(mixed_rms > (REAL_C(2.5) * wanted_rms));

    // What comes out holds the energy of the wave and nothing more, thus the
    // hum is gone and the wave is kept.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05) * wanted_rms, wanted_rms, result_rms);

    iir_free(&iir);
}
