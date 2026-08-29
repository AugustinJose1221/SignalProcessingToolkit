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

// The gain of a filter at its largest and its smallest across a band.
static void iir_band_reach(iir_t* filter, real_t from, real_t to,
                           real_t* smallest, real_t* largest)
{
    *smallest = REAL_LARGEST;
    *largest = REAL_C(0.0);

    for(uint32_t step = 0; step <= 400u; step++)
    {
        real_t place = from + (((to - from) * (real_t)step) / REAL_C(400.0));
        real_t gain = iir_get_gain(filter, place);

        if(gain < *smallest) { *smallest = gain; }
        if(gain > *largest) { *largest = gain; }
    }
}

void test_iir_is_valid_shape(void)
{
    TEST_ASSERT_EQUAL(true, iir_is_valid_shape(IIR_BUTTERWORTH));
    TEST_ASSERT_EQUAL(true, iir_is_valid_shape(IIR_CHEBYSHEV_I));
    TEST_ASSERT_EQUAL(true, iir_is_valid_shape(IIR_CHEBYSHEV_II));
    TEST_ASSERT_EQUAL(true, iir_is_valid_shape(IIR_ELLIPTIC));
    TEST_ASSERT_EQUAL(false, iir_is_valid_shape((iir_shape_t)9));
}

void test_iir_tells_a_ripple_from_an_attenuation(void)
{
    // Two different quantities in two different ranges. Giving one where the
    // other belongs is the easy mistake, thus they are examined apart.
    TEST_ASSERT_EQUAL(true, iir_is_valid_ripple(REAL_C(1.0)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_ripple(REAL_C(60.0)));

    TEST_ASSERT_EQUAL(true, iir_is_valid_attenuation(REAL_C(60.0)));
    TEST_ASSERT_EQUAL(false, iir_is_valid_attenuation(REAL_C(1.0)));

    TEST_ASSERT_EQUAL(false,
                      iir_is_valid_attenuation(IIR_LARGEST_ATTENUATION
                                               + REAL_C(1.0)));
}

void test_a_chebyshev_ripples_by_exactly_what_was_asked(void)
{
    // The whole point of asking for a ripple is getting that ripple. Every
    // number of sections is walked, because an odd number and an even number
    // reach the top of the ripple at different places and both must be right.
    for(uint32_t sections = 1; sections <= 4u; sections++)
    {
        iir_t filter = iir_alloc(sections);

        TEST_ASSERT_EQUAL(true,
                          iir_design_low_pass_with(&filter, REAL_C(0.1),
                                                   IIR_CHEBYSHEV_I,
                                                   REAL_C(1.0),
                                                   REAL_C(60.0)));

        real_t smallest;
        real_t largest;

        iir_band_reach(&filter, REAL_C(0.0), REAL_C(0.1), &smallest,
                       &largest);

        // 1 dB is a ratio of 1.122 between the top and the bottom.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.005), REAL_C(1.122),
                                largest / smallest);

        // AND THE TOP OF THE RIPPLE STANDS AT 1, NOT ABOVE IT. A filter that
        // reached 1.122 would be amplifying the band it is meant to pass.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), largest);

        iir_free(&filter);
    }
}

void test_a_chebyshev_of_the_second_kind_is_flat_and_stops_where_it_should(void)
{
    // For this shape the cutoff is where the band that is STOPPED begins, thus
    // the answer is already all the way down at the cutoff itself.
    iir_t filter = iir_alloc(4);

    TEST_ASSERT_EQUAL(true,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               IIR_CHEBYSHEV_II, REAL_C(1.0),
                                               REAL_C(60.0)));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            iir_get_gain(&filter, REAL_C(0.0)));

    // 60 dB down is a thousandth.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0002), REAL_C(0.001),
                            iir_get_gain(&filter, REAL_C(0.1)));

    // And nothing above it climbs back out of the band that is stopped.
    real_t smallest;
    real_t largest;

    iir_band_reach(&filter, REAL_C(0.1), REAL_C(0.5), &smallest, &largest);

    TEST_ASSERT_TRUE(largest <= REAL_C(0.0011));

    iir_free(&filter);
}

void test_an_elliptic_ripples_in_both_bands_by_what_was_asked(void)
{
    // THE SHAPE THAT RIPPLES AT BOTH ENDS. Both ripples must be what was
    // asked, which is the whole of what makes it an elliptic filter and not
    // merely a sharp one.
    iir_t filter = iir_alloc(4);

    TEST_ASSERT_EQUAL(true,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               IIR_ELLIPTIC, REAL_C(1.0),
                                               REAL_C(60.0)));

    real_t smallest;
    real_t largest;

    iir_band_reach(&filter, REAL_C(0.0), REAL_C(0.1), &smallest, &largest);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.122), largest / smallest);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), largest);

    // The band that is stopped begins soon after the cutoff and holds.
    iir_band_reach(&filter, REAL_C(0.12), REAL_C(0.5), &smallest, &largest);

    TEST_ASSERT_TRUE(largest <= REAL_C(0.0012));

    iir_free(&filter);
}

void test_the_sharper_shapes_really_are_sharper(void)
{
    // The table in the header, held true. Each shape must reach 60 dB down
    // sooner than the one before it.
    const iir_shape_t order_of_sharpness[3] = {IIR_BUTTERWORTH,
                                               IIR_CHEBYSHEV_I,
                                               IIR_ELLIPTIC};
    real_t reached[3];

    for(uint32_t which = 0; which < 3u; which++)
    {
        iir_t filter = iir_alloc(4);

        TEST_ASSERT_EQUAL(true,
                          iir_design_low_pass_with(&filter, REAL_C(0.1),
                                                   order_of_sharpness[which],
                                                   REAL_C(1.0),
                                                   REAL_C(60.0)));

        reached[which] = REAL_C(0.5);

        for(uint32_t step = 0; step <= 2000u; step++)
        {
            real_t place = (real_t)step / REAL_C(4000.0);

            if(iir_get_gain(&filter, place) < REAL_C(0.001))
            {
                reached[which] = place;
                break;
            }
        }

        iir_free(&filter);
    }

    TEST_ASSERT_TRUE(reached[1] < reached[0]);
    TEST_ASSERT_TRUE(reached[2] < reached[1]);
}

void test_iir_sections_for_gives_a_filter_that_really_meets_it(void)
{
    // A number that does not meet the specification is worse than no number.
    // Every shape is asked, and every answer is built and measured.
    const iir_shape_t shapes[4] = {IIR_BUTTERWORTH, IIR_CHEBYSHEV_I,
                                   IIR_CHEBYSHEV_II, IIR_ELLIPTIC};
    const real_t pass_edge = REAL_C(0.1);
    const real_t stop_edge = REAL_C(0.15);

    for(uint32_t which = 0; which < 4u; which++)
    {
        uint32_t sections = iir_sections_for(shapes[which], pass_edge,
                                             stop_edge, REAL_C(1.0),
                                             REAL_C(60.0));

        TEST_ASSERT_TRUE(sections > 0u);

        iir_t filter = iir_alloc(sections);

        // Chebyshev II counts its cutoff at the far edge; the others at the
        // near one.
        real_t cutoff = (shapes[which] == IIR_CHEBYSHEV_II) ? stop_edge
                                                            : pass_edge;

        TEST_ASSERT_EQUAL(true,
                          iir_design_low_pass_with(&filter, cutoff,
                                                   shapes[which], REAL_C(1.0),
                                                   REAL_C(60.0)));

        real_t smallest;
        real_t largest;

        iir_band_reach(&filter, stop_edge, REAL_C(0.5), &smallest, &largest);

        // A thousandth is 60 dB down, with a little room for the rounding of
        // the measurement.
        TEST_ASSERT_TRUE(largest <= REAL_C(0.00105));

        iir_free(&filter);
    }
}

void test_an_elliptic_needs_the_fewest_sections(void)
{
    uint32_t butterworth = iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.1),
                                            REAL_C(0.15), REAL_C(1.0),
                                            REAL_C(60.0));
    uint32_t chebyshev = iir_sections_for(IIR_CHEBYSHEV_I, REAL_C(0.1),
                                          REAL_C(0.15), REAL_C(1.0),
                                          REAL_C(60.0));
    uint32_t elliptic = iir_sections_for(IIR_ELLIPTIC, REAL_C(0.1),
                                         REAL_C(0.15), REAL_C(1.0),
                                         REAL_C(60.0));

    TEST_ASSERT_EQUAL(9, butterworth);
    TEST_ASSERT_EQUAL(5, chebyshev);
    TEST_ASSERT_EQUAL(3, elliptic);
}

void test_iir_sections_for_refuses_what_cannot_be_asked(void)
{
    // The edges the wrong way round.
    TEST_ASSERT_EQUAL(0, iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.2),
                                          REAL_C(0.1), REAL_C(1.0),
                                          REAL_C(60.0)));

    // A ripple that belongs to the other band.
    TEST_ASSERT_EQUAL(0, iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.1),
                                          REAL_C(0.2), REAL_C(60.0),
                                          REAL_C(60.0)));

    TEST_ASSERT_EQUAL(0, iir_sections_for((iir_shape_t)9, REAL_C(0.1),
                                          REAL_C(0.2), REAL_C(1.0),
                                          REAL_C(60.0)));
}

void test_a_high_pass_of_every_shape_passes_the_top_and_stops_the_bottom(void)
{
    const iir_shape_t shapes[4] = {IIR_BUTTERWORTH, IIR_CHEBYSHEV_I,
                                   IIR_CHEBYSHEV_II, IIR_ELLIPTIC};

    for(uint32_t which = 0; which < 4u; which++)
    {
        iir_t filter = iir_alloc(3);

        TEST_ASSERT_EQUAL(true,
                          iir_design_high_pass_with(&filter, REAL_C(0.25),
                                                    shapes[which],
                                                    REAL_C(1.0),
                                                    REAL_C(60.0)));

        // Well above the cutoff it passes, and well below it does not.
        TEST_ASSERT_TRUE(iir_get_gain(&filter, REAL_C(0.45)) > REAL_C(0.85));
        TEST_ASSERT_TRUE(iir_get_gain(&filter, REAL_C(0.02)) < REAL_C(0.01));

        iir_free(&filter);
    }
}

void test_iir_design_with_refuses_what_it_cannot_build(void)
{
    iir_t filter = iir_alloc(2);

    TEST_ASSERT_EQUAL(false,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               (iir_shape_t)9, REAL_C(1.0),
                                               REAL_C(60.0)));

    // A ripple outside what a band that passes can hold.
    TEST_ASSERT_EQUAL(false,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               IIR_CHEBYSHEV_I, REAL_C(50.0),
                                               REAL_C(60.0)));

    // A stop band asked for below what it means to stop anything.
    TEST_ASSERT_EQUAL(false,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               IIR_CHEBYSHEV_II, REAL_C(1.0),
                                               REAL_C(0.5)));

    // A cutoff at or above half the sample rate.
    TEST_ASSERT_EQUAL(false,
                      iir_design_low_pass_with(&filter, REAL_C(0.5),
                                               IIR_BUTTERWORTH, REAL_C(1.0),
                                               REAL_C(60.0)));

    // Butterworth reads neither ripple, thus a value that would refuse another
    // shape must not refuse this one.
    TEST_ASSERT_EQUAL(true,
                      iir_design_low_pass_with(&filter, REAL_C(0.1),
                                               IIR_BUTTERWORTH, REAL_C(999.0),
                                               REAL_C(999.0)));

    iir_free(&filter);
}

void test_the_shaped_butterworth_is_the_plain_one(void)
{
    // Two roads to one filter must not give two filters.
    iir_t plain = iir_alloc(3);
    iir_t shaped = iir_alloc(3);

    iir_design_low_pass(&plain, REAL_C(0.2));
    iir_design_low_pass_with(&shaped, REAL_C(0.2), IIR_BUTTERWORTH,
                             REAL_C(1.0), REAL_C(60.0));

    for(uint32_t step = 0; step <= 40u; step++)
    {
        real_t place = (real_t)step / REAL_C(100.0);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.005), iir_get_gain(&plain, place),
                                iir_get_gain(&shaped, place));
    }

    iir_free(&plain);
    iir_free(&shaped);
}

void test_iir_phase_and_group_delay(void)
{
    iir_t filter = iir_alloc(4);

    iir_design_low_pass_with(&filter, REAL_C(0.1), IIR_BUTTERWORTH,
                             REAL_C(1.0), REAL_C(60.0));

    // The phase is an angle and cannot leave the turn it is measured in.
    for(uint32_t step = 1; step < 50u; step++)
    {
        real_t place = (real_t)step / REAL_C(100.0);
        real_t phase = iir_phase(&filter, place);

        TEST_ASSERT_TRUE(phase >= -REAL_C(3.1416));
        TEST_ASSERT_TRUE(phase <= REAL_C(3.1416));
    }

    // THE DELAY IS A REAL NUMBER OF SAMPLES AND IT RISES TOWARDS THE CUTOFF.
    // That rise is what changes the shape of a waveform, and it is the reason
    // this function exists.
    real_t low = iir_group_delay(&filter, REAL_C(0.02));
    real_t near_cutoff = iir_group_delay(&filter, REAL_C(0.09));

    TEST_ASSERT_TRUE(low > REAL_C(0.0));
    TEST_ASSERT_TRUE(near_cutoff > low);

    iir_free(&filter);
}

void test_a_filter_that_does_nothing_holds_back_nothing(void)
{
    // A filter that passes the signal through turns no phase, thus it holds
    // nothing back at any frequency.
    iir_t filter = iir_alloc(2);

    // A section that passes the signal through: one times the input and no
    // feedback at all.
    for(uint32_t section = 0; section < 2u; section++)
    {
        iir_set_section(&filter, section, REAL_C(1.0), REAL_C(0.0),
                        REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0));
    }

    for(uint32_t step = 1; step < 20u; step++)
    {
        real_t place = (real_t)step / REAL_C(50.0);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                                iir_phase(&filter, place));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                                iir_group_delay(&filter, place));
    }

    iir_free(&filter);
}

void test_iir_sections_for_never_asks_for_more_than_could_be_built(void)
{
    // THE FAULT THAT THE PROPERTY TESTS FOUND.
    //
    // For a small ripple the arithmetic that gives the order asks for the
    // measure of a modulus that has rounded to exactly 1, whose value is not
    // finite. Worked out anyway it gave an order of eight million, and a
    // caller that trusted it would have tried to allocate that many sections.
    //
    // The measure now has a closed form where the modulus is small, and no
    // answer above IIR_LARGEST_SECTIONS is given back at all.
    uint32_t sections = iir_sections_for(IIR_ELLIPTIC, REAL_C(0.05),
                                         REAL_C(0.075), REAL_C(0.0625),
                                         REAL_C(60.0));

    TEST_ASSERT_TRUE(sections > 0u);
    TEST_ASSERT_TRUE(sections <= IIR_LARGEST_SECTIONS);

    // And the answer is the right one, not merely a small one.
    TEST_ASSERT_EQUAL(4, sections);

    // Every shape, at every ripple, must stay inside what can be built.
    const iir_shape_t shapes[4] = {IIR_BUTTERWORTH, IIR_CHEBYSHEV_I,
                                   IIR_CHEBYSHEV_II, IIR_ELLIPTIC};
    const real_t ripples[4] = {REAL_C(0.0625), REAL_C(0.25), REAL_C(1.0),
                               REAL_C(3.0)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        for(uint32_t level = 0; level < 4u; level++)
        {
            uint32_t count = iir_sections_for(shapes[which], REAL_C(0.05),
                                              REAL_C(0.075), ripples[level],
                                              REAL_C(60.0));

            TEST_ASSERT_TRUE(count <= IIR_LARGEST_SECTIONS);
        }
    }
}

void test_an_elliptic_reaches_a_deep_stop_band_at_either_width(void)
{
    // Reaching 70 dB once needed a measure that a 32 bit build could not take,
    // and the design refused. It now uses the closed form of that measure
    // where the modulus is small, and answers at either width.
    iir_t filter = iir_alloc(6);

    TEST_ASSERT_EQUAL(true,
                      iir_design_low_pass_with(&filter, REAL_C(0.05),
                                               IIR_ELLIPTIC, REAL_C(1.0),
                                               REAL_C(70.0)));

    real_t worst = REAL_C(0.0);

    for(uint32_t step = 0; step <= 400u; step++)
    {
        real_t place = REAL_C(0.1)
                       + ((REAL_C(0.4) * (real_t)step) / REAL_C(400.0));
        real_t gain = iir_get_gain(&filter, place);

        if(gain > worst) { worst = gain; }
    }

    // 70 dB down is about a part in 3162.
    TEST_ASSERT_TRUE(worst < REAL_C(0.00035));

    iir_free(&filter);
}

// THE ASKS A DESIGN MUST TURN DOWN, AND THE TWO ENDS OF THE BAND.

void test_a_peak_with_a_centre_or_a_quality_that_means_nothing_is_refused(void)
{
    iir_t iir = iir_alloc(1u);

    TEST_ASSERT_FALSE(iir_design_peak(&iir, REAL_C(0.0), REAL_C(4.0)));
    TEST_ASSERT_FALSE(iir_design_peak(&iir, REAL_C(0.5), REAL_C(4.0)));
    TEST_ASSERT_FALSE(iir_design_peak(&iir, REAL_C(0.6), REAL_C(4.0)));
    TEST_ASSERT_FALSE(iir_design_peak(&iir, REAL_C(0.25), REAL_C(0.0)));
    TEST_ASSERT_FALSE(iir_design_peak(&iir, REAL_C(0.25), REAL_C(-2.0)));

    // And one that means something is taken.
    TEST_ASSERT_TRUE(iir_design_peak(&iir, REAL_C(0.25), REAL_C(4.0)));

    iir_free(&iir);
}

void test_a_specification_that_no_filter_could_meet_asks_for_no_sections(void)
{
    // The band that passes must end before the band that stops begins, and the
    // stop band must be further down than the pass band ripples. A
    // specification that breaks either is not a filter, and giving back a
    // number of sections would invite a caller to allocate them.
    TEST_ASSERT_EQUAL(0, iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.3),
                                          REAL_C(0.2), REAL_C(1.0),
                                          REAL_C(40.0)));
    TEST_ASSERT_EQUAL(0, iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.2),
                                          REAL_C(0.2), REAL_C(1.0),
                                          REAL_C(40.0)));
    TEST_ASSERT_EQUAL(0, iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.1),
                                          REAL_C(0.2), REAL_C(40.0),
                                          REAL_C(1.0)));
}

void test_a_specification_too_sharp_for_a_filter_of_biquads_asks_for_none(void)
{
    // A band that must fall 120 dB across a hair of the rate needs more
    // sections than this module will hold steady. The module says nothing
    // rather than hand back a number no filter could carry.
    uint32_t sections = iir_sections_for(IIR_BUTTERWORTH, REAL_C(0.2000),
                                         REAL_C(0.2001), REAL_C(0.001),
                                         REAL_C(120.0));

    TEST_ASSERT_EQUAL(0, sections);
}

void test_the_delay_is_measured_at_both_ends_of_the_band(void)
{
    // At 0 and at half the rate there is no room on one side for the pair of
    // places the measurement needs, thus the pair is moved inwards. A step
    // that reached outside the band would take a phase from a frequency that
    // does not exist.
    iir_t iir = iir_alloc(2u);
    TEST_ASSERT_TRUE(iir_design_low_pass(&iir, REAL_C(0.1)));

    real_t at_nothing = iir_group_delay(&iir, REAL_C(0.0));
    real_t at_half = iir_group_delay(&iir, REAL_C(0.5));

    // A delay is a number of samples. It must be a real number at both ends
    // and it must not be absurd.
    TEST_ASSERT_TRUE(at_nothing > REAL_C(-1.0));
    TEST_ASSERT_TRUE(at_nothing < REAL_C(1000.0));
    TEST_ASSERT_TRUE(at_half > REAL_C(-1.0));
    TEST_ASSERT_TRUE(at_half < REAL_C(1000.0));

    iir_free(&iir);
}

void test_the_delay_stays_sensible_where_the_phase_folds_over(void)
{
    // The phase comes back folded into one turn. A filter of many sections
    // turns its phase through several whole turns across the band, thus the
    // measurement steps across the fold again and again.
    //
    // Unfolding that is the whole of what makes the measurement work. Without
    // it the delay would jump by a whole turn divided by the step, which for
    // this filter is thousands of samples, at every fold.
    iir_t iir = iir_alloc(8u);
    TEST_ASSERT_TRUE(iir_design_low_pass(&iir, REAL_C(0.25)));

    for(uint32_t step = 1; step < 200u; step++)
    {
        real_t frequency = REAL_C(0.0025) * (real_t)step;
        real_t delay = iir_group_delay(&iir, frequency);

        // The delay of a filter of 8 sections cannot reach a hundred samples.
        // A fold that was not unfolded would give thousands.
        TEST_ASSERT_TRUE(delay > REAL_C(-100.0));
        TEST_ASSERT_TRUE(delay < REAL_C(100.0));
    }

    iir_free(&iir);
}
