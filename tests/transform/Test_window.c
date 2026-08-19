#include "unity.h"
#include "window.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.001f
#define SIZE        64u

static float buffer[SIZE];

void setUp(void)
{

}

void tearDown(void)
{

}

// The highest side lobe of a window, in decibels below its peak.
//
// The test works this out with a plain transform of its own and not with the
// fft module, so that a fault in one module cannot hide a fault in the other.
// The window is padded, thus the shape between the bins can be seen.
static float highest_side_lobe(window_kind_t kind, float parameter)
{
    const uint32_t pad = 2048u;
    float window[SIZE];
    float magnitude[1024];

    window_build_with(window, SIZE, kind, parameter);

    for(uint32_t bin = 0; bin < (pad / 2u); bin++)
    {
        float real = 0.0f;
        float imaginary = 0.0f;
        for(uint32_t n = 0; n < SIZE; n++)
        {
            float angle = (-2.0f * 3.14159265f * (float)bin * (float)n) / (float)pad;
            real += window[n] * cosf(angle);
            imaginary += window[n] * sinf(angle);
        }
        magnitude[bin] = sqrtf((real * real) + (imaginary * imaginary));
    }

    // The main lobe ends where the magnitude stops falling.
    uint32_t start = 1;
    while((start < ((pad / 2u) - 1u)) && (magnitude[start] < magnitude[start - 1u]))
    {
        start++;
    }

    float highest = 0.0f;
    for(uint32_t bin = start; bin < (pad / 2u); bin++)
    {
        if(magnitude[bin] > highest)
        {
            highest = magnitude[bin];
        }
    }

    return 20.0f * log10f(highest / magnitude[0]);
}

void test_window_is_valid_kind(void)
{
    TEST_ASSERT_EQUAL(true, window_is_valid_kind(WINDOW_RECTANGULAR));
    TEST_ASSERT_EQUAL(true, window_is_valid_kind(WINDOW_KAISER));
    TEST_ASSERT_EQUAL(false, window_is_valid_kind((window_kind_t)(WINDOW_KAISER + 1)));
}

void test_window_takes_a_parameter(void)
{
    TEST_ASSERT_EQUAL(false, window_takes_a_parameter(WINDOW_HANN));
    TEST_ASSERT_EQUAL(false, window_takes_a_parameter(WINDOW_BLACKMAN));
    TEST_ASSERT_EQUAL(true, window_takes_a_parameter(WINDOW_TUKEY));
    TEST_ASSERT_EQUAL(true, window_takes_a_parameter(WINDOW_KAISER));
}

void test_window_rectangular_is_all_ones(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, buffer[index]);
    }
}

void test_window_hann_falls_to_nothing_at_both_ends(void)
{
    window_build(buffer, SIZE, WINDOW_HANN);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, buffer[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, buffer[SIZE - 1u]);
    // The middle of a window of an even size stands beside the peak, thus it
    // is near one and not exactly one.
    TEST_ASSERT_TRUE(buffer[SIZE / 2u] > 0.99f);
}

void test_window_hamming_does_not_fall_to_nothing(void)
{
    window_build(buffer, SIZE, WINDOW_HAMMING);

    // This is what parts a Hamming window from a Hann one: its ends hold 0.08.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.08f, buffer[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.08f, buffer[SIZE - 1u]);
}

void test_every_window_is_symmetric(void)
{
    window_kind_t kind[] = {WINDOW_HANN, WINDOW_HAMMING, WINDOW_BLACKMAN,
                            WINDOW_BLACKMAN_HARRIS, WINDOW_TUKEY, WINDOW_KAISER};

    for(uint32_t k = 0; k < 6u; k++)
    {
        window_build_with(buffer, SIZE, kind[k], 5.0f);

        for(uint32_t index = 0; index < (SIZE / 2u); index++)
        {
            TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, buffer[index],
                                     buffer[SIZE - 1u - index]);
        }
    }
}

void test_window_of_one_sample_holds_one(void)
{
    float single = 2.0f;

    window_build(&single, 1u, WINDOW_HANN);

    // A window of one sample cannot fall at its ends, thus it must not give
    // zero. Zero would take the whole signal away.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, single);
}

void test_window_value_outside_the_window_is_nothing(void)
{
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f,
                             window_value(SIZE, SIZE, WINDOW_HANN, 0.0f));
}

void test_window_tukey_at_nothing_is_rectangular(void)
{
    window_build_with(buffer, SIZE, WINDOW_TUKEY, 0.0f);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, buffer[index]);
    }
}

void test_window_tukey_at_one_is_hann(void)
{
    float hann[SIZE];

    window_build_with(buffer, SIZE, WINDOW_TUKEY, 1.0f);
    window_build(hann, SIZE, WINDOW_HANN);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, hann[index], buffer[index]);
    }
}

void test_window_tukey_holds_its_middle(void)
{
    window_build_with(buffer, SIZE, WINDOW_TUKEY, 0.5f);

    // Half of the window falls, thus the middle half stays at one.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, buffer[SIZE / 2u]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, buffer[0]);
}

void test_window_kaiser_at_nothing_is_rectangular(void)
{
    window_build_with(buffer, SIZE, WINDOW_KAISER, 0.0f);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, buffer[index]);
    }
}

void test_window_kaiser_falls_further_as_beta_grows(void)
{
    float small[SIZE];
    float large[SIZE];

    window_build_with(small, SIZE, WINDOW_KAISER, 2.0f);
    window_build_with(large, SIZE, WINDOW_KAISER, 8.0f);

    TEST_ASSERT_TRUE(large[0] < small[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, large[SIZE / 2u]);
}

void test_window_kaiser_beta_follows_the_rule(void)
{
    // The rule of Kaiser, at the two values that the header names.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.653f, window_kaiser_beta(60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, window_kaiser_beta(20.0f));
    TEST_ASSERT_TRUE(window_kaiser_beta(100.0f) > window_kaiser_beta(60.0f));

    // The sign of the level must not matter.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, window_kaiser_beta(60.0f),
                             window_kaiser_beta(-60.0f));
}

void test_window_coherent_gain(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, window_coherent_gain(buffer, SIZE));

    // A Hann window halves the height of a tone. This is the number that a
    // reading must be divided by, and forgetting it is the usual fault.
    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, window_coherent_gain(buffer, SIZE));
}

void test_window_noise_gain(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, window_noise_gain(buffer, SIZE));

    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.6076f, window_noise_gain(buffer, SIZE));
}

void test_window_noise_bandwidth(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, window_noise_bandwidth(buffer, SIZE));

    // A Hann window gives 1.5 bins. This is the known value of the window.
    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 1.5f, window_noise_bandwidth(buffer, SIZE));

    window_build(buffer, SIZE, WINDOW_BLACKMAN);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 1.73f, window_noise_bandwidth(buffer, SIZE));
}

void test_window_apply(void)
{
    float input[4] = {2.0f, 2.0f, 2.0f, 2.0f};
    float window[4] = {0.0f, 0.5f, 0.5f, 0.0f};
    float output[4];

    window_apply(window, input, output, 4u);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, output[3]);
}

void test_window_apply_can_write_over_its_input(void)
{
    float signal[4] = {2.0f, 2.0f, 2.0f, 2.0f};
    float window[4] = {0.0f, 0.5f, 0.5f, 0.0f};

    window_apply(window, signal, signal, 4u);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, signal[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, signal[1]);
}

void test_the_side_lobes_are_where_the_table_says(void)
{
    // The table in the header sets out what each window is for. If these
    // numbers move, that table is no longer true and the reader is misled.
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -13.3f,
                             highest_side_lobe(WINDOW_RECTANGULAR, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -31.5f, highest_side_lobe(WINDOW_HANN, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -42.4f, highest_side_lobe(WINDOW_HAMMING, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, -58.1f, highest_side_lobe(WINDOW_BLACKMAN, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(3.0f, -92.1f,
                             highest_side_lobe(WINDOW_BLACKMAN_HARRIS, 0.0f));
}

void test_the_side_lobes_of_kaiser_are_not_the_stop_band_of_its_rule(void)
{
    // The header warns that these two numbers are far apart, and this test
    // holds that warning true. A beta for a stop band of 60 dB gives a window
    // whose own side lobes stand near 42 dB down, not 60.
    float beta = window_kaiser_beta(60.0f);

    TEST_ASSERT_FLOAT_WITHIN(2.0f, -41.6f, highest_side_lobe(WINDOW_KAISER, beta));
}
