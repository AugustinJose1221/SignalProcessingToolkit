#include "unity.h"
#include "fir.h"
#include <stdlib.h>
#include <math.h>

#define PI      3.14159265358979323846f

void setUp(void)
{

}

void tearDown(void)
{

}

// Give the size of the answer of the filter at the given frequency, measured
// by sending a sine through it and reading the size of the result. The first
// samples hold the start of the filter, thus the measurement leaves them out.
static float measure_gain(fir_t* fir, float frequency, uint32_t samples)
{
    float largest_output = 0.0f;
    float largest_input = 0.0f;

    fir_reset(fir);

    for(uint32_t index = 0; index < samples; index++)
    {
        float sample = sinf(2.0f*PI*frequency*(float)index);
        float result = fir_process_sample(fir, sample);

        if(index > (2*fir->length))
        {
            if(fabsf(result) > largest_output)
            {
                largest_output = fabsf(result);
            }
            // A sine that is sampled does not reach its full size at every
            // frequency. At the frequency 0.4 the largest sample is 0.951.
            // Thus the gain is the largest output divided by the largest
            // input, and not the largest output alone.
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
    float coefficient[9];
    float history[9];

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

    fir_set_coefficient(&fir, 0, 0.25f);
    fir_set_coefficient(&fir, 1, 0.5f);
    fir_set_coefficient(&fir, 2, 0.25f);

    TEST_ASSERT_EQUAL_FLOAT(0.25f, fir_get_coefficient(&fir, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, fir_get_coefficient(&fir, 1));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, fir_get_coefficient(&fir, 2));

    fir_free(&fir);
}

void test_fir_a_filter_with_one_coefficient_multiplies_the_signal(void)
{
    fir_t fir = fir_alloc(1);
    fir_set_coefficient(&fir, 0, 2.0f);

    TEST_ASSERT_EQUAL_FLOAT(2.0f, fir_process_sample(&fir, 1.0f));
    TEST_ASSERT_EQUAL_FLOAT(-6.0f, fir_process_sample(&fir, -3.0f));

    fir_free(&fir);
}

void test_fir_process_sample_holds_the_history(void)
{
    // A filter that gives the mean of the last two samples.
    fir_t fir = fir_alloc(2);
    fir_set_coefficient(&fir, 0, 0.5f);
    fir_set_coefficient(&fir, 1, 0.5f);

    // The history holds zero at the start.
    TEST_ASSERT_EQUAL_FLOAT(0.5f, fir_process_sample(&fir, 1.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.5f, fir_process_sample(&fir, 2.0f));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, fir_process_sample(&fir, 3.0f));

    fir_free(&fir);
}

void test_fir_reset_clears_the_history(void)
{
    fir_t fir = fir_alloc(2);
    fir_set_coefficient(&fir, 0, 0.5f);
    fir_set_coefficient(&fir, 1, 0.5f);

    fir_process_sample(&fir, 10.0f);
    fir_reset(&fir);

    TEST_ASSERT_EQUAL(0, fir.position);
    // After the reset the filter behaves as a new filter.
    TEST_ASSERT_EQUAL_FLOAT(0.5f, fir_process_sample(&fir, 1.0f));

    fir_free(&fir);
}

void test_fir_process_block_gives_the_same_result_as_single_samples(void)
{
    fir_t first = fir_alloc(5);
    fir_t second = fir_alloc(5);
    fir_design_low_pass(&first, 0.2f);
    fir_design_low_pass(&second, 0.2f);

    float input[10];
    float output[10];
    for(uint32_t index = 0; index < 10; index++)
    {
        input[index] = sinf(0.5f*(float)index);
    }

    fir_process_block(&first, input, output, 10);

    for(uint32_t index = 0; index < 10; index++)
    {
        // Keep the result in a variable. The macro TEST_ASSERT_EQUAL_FLOAT
        // writes its first argument two times, one time for the tolerance and
        // one time for the value. A call to the filter inside that argument
        // would run two times and move the filter two samples forward.
        float expected = fir_process_sample(&second, input[index]);
        TEST_ASSERT_EQUAL_FLOAT(expected, output[index]);
    }

    fir_free(&first);
    fir_free(&second);
}

void test_fir_process_block_may_write_over_its_input(void)
{
    fir_t fir = fir_alloc(3);
    fir_set_coefficient(&fir, 0, 1.0f);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    fir_process_block(&fir, data, data, 4);

    TEST_ASSERT_EQUAL_FLOAT(1.0f, data[0]);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, data[3]);

    fir_free(&fir);
}

void test_fir_the_low_pass_filter_lets_a_low_frequency_pass(void)
{
    fir_t fir = fir_alloc(41);
    fir_design_low_pass(&fir, 0.1f);

    // A frequency well below the cutoff must pass almost unchanged.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, measure_gain(&fir, 0.02f, 400));
    // A frequency well above the cutoff must almost go away.
    TEST_ASSERT_TRUE(measure_gain(&fir, 0.3f, 400) < 0.05f);

    fir_free(&fir);
}

void test_fir_the_high_pass_filter_lets_a_high_frequency_pass(void)
{
    fir_t fir = fir_alloc(41);
    fir_design_high_pass(&fir, 0.25f);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, measure_gain(&fir, 0.4f, 400));
    TEST_ASSERT_TRUE(measure_gain(&fir, 0.05f, 400) < 0.05f);

    fir_free(&fir);
}

void test_fir_the_band_pass_filter_lets_only_the_band_pass(void)
{
    fir_t fir = fir_alloc(61);
    fir_design_band_pass(&fir, 0.15f, 0.30f);

    // Inside the band.
    TEST_ASSERT_TRUE(measure_gain(&fir, 0.22f, 600) > 0.85f);
    // Below the band and above the band.
    TEST_ASSERT_TRUE(measure_gain(&fir, 0.03f, 600) < 0.1f);
    TEST_ASSERT_TRUE(measure_gain(&fir, 0.45f, 600) < 0.1f);

    fir_free(&fir);
}

void test_fir_get_gain_agrees_with_a_measurement(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, 0.15f);

    float frequencies[4] = {0.02f, 0.10f, 0.30f, 0.45f};

    for(uint32_t index = 0; index < 4; index++)
    {
        float calculated = fir_get_gain(&fir, frequencies[index]);
        float measured = measure_gain(&fir, frequencies[index], 600);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, calculated, measured);
    }

    fir_free(&fir);
}

void test_fir_the_low_pass_filter_passes_a_constant_signal_unchanged(void)
{
    // The gain at the frequency zero must be one, thus the sum of the
    // coefficients must be one.
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, 0.2f);

    float sum = 0.0f;
    for(uint32_t index = 0; index < fir.length; index++)
    {
        sum += fir_get_coefficient(&fir, index);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, sum);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, fir_get_gain(&fir, 0.0f));

    fir_free(&fir);
}

void test_fir_the_high_pass_filter_stops_a_constant_signal(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_high_pass(&fir, 0.2f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fir_get_gain(&fir, 0.0f));

    fir_free(&fir);
}

void test_fir_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    float coefficient[21];
    float history[21];

    fir_t dynamic_fir = fir_alloc(21);
    fir_t static_fir = fir_static_alloc(21, coefficient, history);

    fir_design_low_pass(&dynamic_fir, 0.2f);
    fir_design_low_pass(&static_fir, 0.2f);

    for(uint32_t index = 0; index < 50; index++)
    {
        float sample = sinf(0.3f*(float)index) + cosf(1.1f*(float)index);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f,
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
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(101, 0.05f));
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(101, 0.02f));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, 0.01f));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, 0.002f));

    // The same cutoff becomes valid when the filter is long enough.
    TEST_ASSERT_EQUAL(true, fir_is_valid_cutoff(1001, 0.01f));

    // The turn needs room at the top of the band as well.
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(101, 0.49f));
    TEST_ASSERT_EQUAL(false, fir_is_valid_cutoff(0, 0.1f));
}

void test_fir_is_valid_band(void)
{
    TEST_ASSERT_EQUAL(true, fir_is_valid_band(101, 0.05f, 0.15f));

    // A band narrower than the turn leaves no frequency that passes fully.
    TEST_ASSERT_EQUAL(false, fir_is_valid_band(101, 0.10f, 0.11f));

    // An edge that is itself too low makes the whole band invalid.
    TEST_ASSERT_EQUAL(false, fir_is_valid_band(101, 0.005f, 0.15f));
}

void test_fir_design_refuses_a_cutoff_that_is_too_low(void)
{
    fir_t fir = fir_alloc(101);

    // Build a filter that is good, and hold its coefficients.
    TEST_ASSERT_EQUAL(true, fir_design_low_pass(&fir, 0.10f));
    float before[101];
    for(uint32_t index = 0; index < 101; index++)
    {
        before[index] = fir_get_coefficient(&fir, index);
    }

    // A design that cannot be held must say so and must change nothing.
    TEST_ASSERT_EQUAL(false, fir_design_low_pass(&fir, 0.002f));
    TEST_ASSERT_EQUAL(false, fir_design_high_pass(&fir, 0.002f));
    TEST_ASSERT_EQUAL(false, fir_design_band_pass(&fir, 0.10f, 0.105f));

    for(uint32_t index = 0; index < 101; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, before[index],
                                 fir_get_coefficient(&fir, index));
    }

    fir_free(&fir);
}

void test_fir_design_holds_its_pass_band_at_the_shortest_valid_cutoff(void)
{
    fir_t fir = fir_alloc(101);
    float turn = FIR_TRANSITION / 101.0f;

    TEST_ASSERT_EQUAL(true, fir_design_low_pass(&fir, turn));

    // This is the measurement that sets the limit: at the turn the pass band
    // still reaches one, and below it the gain falls away.
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, fir_get_gain(&fir, 0.0f));

    fir_free(&fir);
}
