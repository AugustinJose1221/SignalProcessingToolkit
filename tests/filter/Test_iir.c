#include "unity.h"
#include "iir.h"
#include <stdlib.h>
#include <math.h>

#define PI      3.14159265358979323846f

void setUp(void)
{

}

void tearDown(void)
{

}

// Measure the size of the answer of the filter at the given frequency. The
// first samples hold the start of the filter, thus the measurement leaves
// them out.
static float measure_gain(iir_t* iir, float frequency)
{
    const uint32_t samples = 2000;
    float largest_output = 0.0f;
    float largest_input = 0.0f;

    iir_reset(iir);

    for(uint32_t index = 0; index < samples; index++)
    {
        float sample = sinf(2.0f*PI*frequency*(float)index);
        float result = iir_process_sample(iir, sample);

        if(index > (samples/2))
        {
            if(fabsf(result) > largest_output)
            {
                largest_output = fabsf(result);
            }
            // A sine that is sampled does not reach its full size at every
            // frequency, thus the gain is the largest output divided by the
            // largest input.
            if(fabsf(sample) > largest_input)
            {
                largest_input = fabsf(sample);
            }
        }
    }

    if(largest_input == 0.0f)
    {
        return 0.0f;
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
    float coefficient[IIR_COEFFICIENT_SIZE(2)];
    float state[IIR_STATE_SIZE(2)];

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

    TEST_ASSERT_EQUAL_FLOAT(1.0f, iir_process_sample(&iir, 1.0f));
    TEST_ASSERT_EQUAL_FLOAT(-4.5f, iir_process_sample(&iir, -4.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, iir_get_gain(&iir, 0.25f));

    iir_free(&iir);
}

void test_iir_set_section_divides_by_the_first_coefficient(void)
{
    iir_t iir = iir_alloc(1);

    // Give the coefficients with a0 of 2. The filter must divide every
    // coefficient by that value.
    iir_set_section(&iir, 0, 2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL_FLOAT(1.0f, iir.coefficient[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, iir_process_sample(&iir, 5.0f));

    iir_free(&iir);
}

void test_iir_the_low_pass_filter_lets_a_low_frequency_pass(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, 0.1f);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, measure_gain(&iir, 0.01f));
    TEST_ASSERT_TRUE(measure_gain(&iir, 0.35f) < 0.05f);

    iir_free(&iir);
}

void test_iir_the_high_pass_filter_lets_a_high_frequency_pass(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_high_pass(&iir, 0.25f);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, measure_gain(&iir, 0.45f));
    TEST_ASSERT_TRUE(measure_gain(&iir, 0.05f) < 0.05f);

    iir_free(&iir);
}

void test_iir_the_gain_at_the_cutoff_is_the_half_power_point(void)
{
    // A filter of Butterworth gives the gain 1/sqrt(2) at its cutoff, which is
    // about 0.7071, for every order.
    float expected = 1.0f / sqrtf(2.0f);

    for(uint32_t sections = 1; sections <= 3; sections++)
    {
        iir_t low = iir_alloc(sections);
        iir_design_low_pass(&low, 0.2f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, iir_get_gain(&low, 0.2f));
        iir_free(&low);

        iir_t high = iir_alloc(sections);
        iir_design_high_pass(&high, 0.2f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, iir_get_gain(&high, 0.2f));
        iir_free(&high);
    }
}

void test_iir_the_low_pass_filter_passes_a_constant_signal_unchanged(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, 0.15f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, iir_get_gain(&iir, 0.0f));

    // Send a signal that does not change through the filter. After the start
    // the output must be the same as the input.
    for(uint32_t index = 0; index < 500; index++)
    {
        iir_process_sample(&iir, 3.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, iir_process_sample(&iir, 3.0f));

    iir_free(&iir);
}

void test_iir_the_high_pass_filter_stops_a_constant_signal(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_high_pass(&iir, 0.15f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, iir_get_gain(&iir, 0.0f));

    for(uint32_t index = 0; index < 500; index++)
    {
        iir_process_sample(&iir, 3.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, iir_process_sample(&iir, 3.0f));

    iir_free(&iir);
}

void test_iir_a_filter_of_a_higher_order_gives_a_sharper_edge(void)
{
    // Well above the cutoff, a filter with more sections must let less
    // through.
    iir_t small = iir_alloc(1);
    iir_t large = iir_alloc(3);
    iir_design_low_pass(&small, 0.1f);
    iir_design_low_pass(&large, 0.1f);

    float small_gain = iir_get_gain(&small, 0.25f);
    float large_gain = iir_get_gain(&large, 0.25f);

    TEST_ASSERT_TRUE(large_gain < small_gain);

    iir_free(&small);
    iir_free(&large);
}

void test_iir_get_gain_agrees_with_a_measurement(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, 0.12f);

    float frequencies[4] = {0.02f, 0.08f, 0.20f, 0.40f};

    for(uint32_t index = 0; index < 4; index++)
    {
        float calculated = iir_get_gain(&iir, frequencies[index]);
        float measured = measure_gain(&iir, frequencies[index]);
        TEST_ASSERT_FLOAT_WITHIN(0.03f, calculated, measured);
    }

    iir_free(&iir);
}

void test_iir_the_filter_stays_stable(void)
{
    // A filter with feedback can run away if its coefficients are bad. Send a
    // long signal through it and examine that the output stays small.
    iir_t iir = iir_alloc(3);
    iir_design_low_pass(&iir, 0.05f);

    for(uint32_t index = 0; index < 5000; index++)
    {
        float sample = sinf(0.01f*(float)index) + sinf(2.0f*(float)index);
        float result = iir_process_sample(&iir, sample);

        TEST_ASSERT_TRUE(fabsf(result) < 10.0f);
        TEST_ASSERT_FALSE(isnan(result));
    }

    iir_free(&iir);
}

void test_iir_reset_clears_the_state(void)
{
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, 0.2f);

    for(uint32_t index = 0; index < 100; index++)
    {
        iir_process_sample(&iir, 5.0f);
    }

    iir_reset(&iir);

    for(uint32_t index = 0; index < IIR_STATE_SIZE(1); index++)
    {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, iir.state[index]);
    }

    iir_free(&iir);
}

void test_iir_process_block_gives_the_same_result_as_single_samples(void)
{
    iir_t first = iir_alloc(2);
    iir_t second = iir_alloc(2);
    iir_design_low_pass(&first, 0.2f);
    iir_design_low_pass(&second, 0.2f);

    float input[20];
    float output[20];
    for(uint32_t index = 0; index < 20; index++)
    {
        input[index] = sinf(0.4f*(float)index);
    }

    iir_process_block(&first, input, output, 20);

    for(uint32_t index = 0; index < 20; index++)
    {
        // Keep the result in a variable. The macro TEST_ASSERT_EQUAL_FLOAT
        // writes its first argument two times, thus a call to the filter
        // inside that argument would move the filter two samples forward.
        float expected = iir_process_sample(&second, input[index]);
        TEST_ASSERT_EQUAL_FLOAT(expected, output[index]);
    }

    iir_free(&first);
    iir_free(&second);
}

void test_iir_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    float coefficient[IIR_COEFFICIENT_SIZE(2)];
    float state[IIR_STATE_SIZE(2)];

    iir_t dynamic_iir = iir_alloc(2);
    iir_t static_iir = iir_static_alloc(2, coefficient, state);

    iir_design_high_pass(&dynamic_iir, 0.3f);
    iir_design_high_pass(&static_iir, 0.3f);

    for(uint32_t index = 0; index < 100; index++)
    {
        float sample = sinf(0.2f*(float)index) + 0.5f;
        TEST_ASSERT_FLOAT_WITHIN(0.0001f,
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
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(0.1f));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(0.01f));
    TEST_ASSERT_EQUAL(true, iir_is_valid_cutoff(IIR_MIN_CUTOFF));

    // Under the limit a section in single precision loses its accuracy.
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(0.0001f));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(0.0f));

    // At and above half the sample rate there is nothing to pass.
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(0.5f));
    TEST_ASSERT_EQUAL(false, iir_is_valid_cutoff(0.6f));
}

void test_iir_design_refuses_a_cutoff_that_is_too_low(void)
{
    iir_t iir = iir_alloc(2);

    // Build a filter that is good, and hold what it does.
    TEST_ASSERT_EQUAL(true, iir_design_low_pass(&iir, 0.05f));
    float before = iir_get_gain(&iir, 0.02f);

    // A design that cannot be held must say so and must change nothing.
    TEST_ASSERT_EQUAL(false, iir_design_low_pass(&iir, 0.00001f));
    TEST_ASSERT_EQUAL(false, iir_design_high_pass(&iir, 0.00001f));

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, before, iir_get_gain(&iir, 0.02f));

    iir_free(&iir);
}

void test_iir_design_takes_a_cutoff_at_the_limit(void)
{
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL(true, iir_design_low_pass(&iir, IIR_MIN_CUTOFF));

    // At the limit the gain at zero frequency must still be near one. This is
    // the measurement that sets the limit, thus the test holds it.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, iir_get_gain(&iir, 0.0f));

    iir_free(&iir);
}

void test_iir_design_notch_stops_one_frequency_and_passes_the_rest(void)
{
    // The hum of the mains at 50 Hz against a sample rate of 1000 Hz.
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_notch(&iir, 0.05f, 30.0f));

    // Nothing passes at the frequency itself.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, iir_get_gain(&iir, 0.05f));

    // Everything else passes untouched, close by and far away.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, iir_get_gain(&iir, 0.04f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, iir_get_gain(&iir, 0.06f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, iir_get_gain(&iir, 0.001f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, iir_get_gain(&iir, 0.4f));

    iir_free(&iir);
}

void test_iir_design_notch_gets_narrower_as_the_quality_rises(void)
{
    iir_t wide = iir_alloc(1);
    iir_t narrow = iir_alloc(1);

    iir_design_notch(&wide, 0.05f, 5.0f);
    iir_design_notch(&narrow, 0.05f, 50.0f);

    // Both stop the frequency itself.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, iir_get_gain(&wide, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, iir_get_gain(&narrow, 0.05f));

    // A little away from it, the narrow one has already let go.
    TEST_ASSERT_TRUE(iir_get_gain(&narrow, 0.055f) > iir_get_gain(&wide, 0.055f));

    iir_free(&wide);
    iir_free(&narrow);
}

void test_iir_design_notch_refuses_what_it_cannot_hold(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, 0.00001f, 30.0f));
    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, 0.05f, 0.0f));
    TEST_ASSERT_EQUAL(false, iir_design_notch(&iir, 0.05f, -1.0f));

    iir_free(&iir);
}

void test_iir_design_peak_passes_one_frequency_and_stops_the_rest(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_peak(&iir, 0.05f, 30.0f));

    // The gain is one at the frequency, however narrow the band is.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, iir_get_gain(&iir, 0.05f));

    // Everything else falls away on both sides.
    TEST_ASSERT_TRUE(iir_get_gain(&iir, 0.01f) < 0.05f);
    TEST_ASSERT_TRUE(iir_get_gain(&iir, 0.1f) < 0.05f);
    TEST_ASSERT_TRUE(iir_get_gain(&iir, 0.4f) < 0.05f);

    iir_free(&iir);
}

void test_iir_design_peak_and_notch_are_two_sides_of_the_same_thing(void)
{
    // They share their poles and differ in their zeros, thus what one passes
    // the other stops. Their gains squared must add to about one.
    iir_t notch = iir_alloc(1);
    iir_t peak = iir_alloc(1);

    iir_design_notch(&notch, 0.05f, 20.0f);
    iir_design_peak(&peak, 0.05f, 20.0f);

    float test[4] = {0.02f, 0.045f, 0.05f, 0.09f};

    for(uint32_t index = 0; index < 4u; index++)
    {
        float one = iir_get_gain(&notch, test[index]);
        float other = iir_get_gain(&peak, test[index]);
        TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, (one * one) + (other * other));
    }

    iir_free(&notch);
    iir_free(&peak);
}

void test_iir_design_band_pass(void)
{
    iir_t iir = iir_alloc(4);

    TEST_ASSERT_EQUAL(true, iir_design_band_pass(&iir, 0.05f, 0.20f));

    // The gain is one in the middle of the band.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, iir_get_gain(&iir, 0.10f));

    // Each cutoff stands where the gain is the root of a half, which is what
    // a cutoff means.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.7071f, iir_get_gain(&iir, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.7071f, iir_get_gain(&iir, 0.20f));

    // Well outside the band, almost nothing passes.
    TEST_ASSERT_TRUE(iir_get_gain(&iir, 0.005f) < 0.01f);
    TEST_ASSERT_TRUE(iir_get_gain(&iir, 0.45f) < 0.01f);

    iir_free(&iir);
}

void test_iir_design_band_pass_needs_an_even_number_of_sections(void)
{
    // Half of the sections make each edge, thus an odd number cannot be
    // shared and the design must say so rather than build something else.
    iir_t odd = iir_alloc(3);
    iir_t even = iir_alloc(2);

    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&odd, 0.05f, 0.20f));
    TEST_ASSERT_EQUAL(true, iir_design_band_pass(&even, 0.05f, 0.20f));

    iir_free(&odd);
    iir_free(&even);
}

void test_iir_design_band_pass_refuses_a_band_that_is_not_a_band(void)
{
    iir_t iir = iir_alloc(2);

    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, 0.20f, 0.05f));
    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, 0.05f, 0.05f));
    TEST_ASSERT_EQUAL(false, iir_design_band_pass(&iir, 0.00001f, 0.20f));

    iir_free(&iir);
}

void test_iir_design_band_stop(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(true, iir_design_band_stop(&iir, 0.04f, 0.06f));

    // The middle of the band is the GEOMETRIC mean of the two edges, which is
    // 0.049 and not 0.05. A filter of this kind is symmetric in the ratio of
    // the frequencies and not in their difference.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, iir_get_gain(&iir, 0.049f));

    // Each edge stands where the gain is the root of a half.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.7071f, iir_get_gain(&iir, 0.04f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.7071f, iir_get_gain(&iir, 0.06f));

    // Well away from the band, everything passes.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, iir_get_gain(&iir, 0.001f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, iir_get_gain(&iir, 0.4f));

    iir_free(&iir);
}

void test_iir_design_band_stop_refuses_a_band_that_is_not_a_band(void)
{
    iir_t iir = iir_alloc(1);

    TEST_ASSERT_EQUAL(false, iir_design_band_stop(&iir, 0.06f, 0.04f));
    TEST_ASSERT_EQUAL(false, iir_design_band_stop(&iir, 0.00001f, 0.06f));

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
    iir_design_notch(&iir, 0.05f, 30.0f);

    float wanted_energy = 0.0f;
    float mixed_energy = 0.0f;
    float result_energy = 0.0f;
    uint32_t counted = 0;

    for(uint32_t index = 0; index < 4000u; index++)
    {
        float time = (float)index;
        float wanted = sinf(2.0f * PI * 0.005f * time);
        float mixed = wanted + (3.0f * sinf(2.0f * PI * 0.05f * time));

        float result = iir_process_sample(&iir, mixed);

        // Look only after the filter has settled.
        if(index > 1000u)
        {
            wanted_energy += wanted * wanted;
            mixed_energy += mixed * mixed;
            result_energy += result * result;
            counted++;
        }
    }

    float wanted_rms = sqrtf(wanted_energy / (float)counted);
    float mixed_rms = sqrtf(mixed_energy / (float)counted);
    float result_rms = sqrtf(result_energy / (float)counted);

    // What went in holds far more energy than the wave alone.
    TEST_ASSERT_TRUE(mixed_rms > (2.5f * wanted_rms));

    // What comes out holds the energy of the wave and nothing more, thus the
    // hum is gone and the wave is kept.
    TEST_ASSERT_FLOAT_WITHIN(0.05f * wanted_rms, wanted_rms, result_rms);

    iir_free(&iir);
}
