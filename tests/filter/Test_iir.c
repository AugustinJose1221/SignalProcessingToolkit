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
