#include "unity.h"
#include "real_assert.h"
#include "window.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.001)
#define SIZE        64u

static real_t buffer[SIZE];

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
static real_t highest_side_lobe(window_kind_t kind, real_t parameter)
{
    const uint32_t pad = 2048u;
    real_t window[SIZE];
    real_t magnitude[1024];

    window_build_with(window, SIZE, kind, parameter);

    for(uint32_t bin = 0; bin < (pad / 2u); bin++)
    {
        real_t real = REAL_C(0.0);
        real_t imaginary = REAL_C(0.0);
        for(uint32_t n = 0; n < SIZE; n++)
        {
            real_t angle = (-REAL_C(2.0) * REAL_C(3.14159265) * (real_t)bin * (real_t)n) / (real_t)pad;
            real += window[n] * REAL_COS(angle);
            imaginary += window[n] * REAL_SIN(angle);
        }
        magnitude[bin] = REAL_SQRT((real * real) + (imaginary * imaginary));
    }

    // The main lobe ends where the magnitude stops falling.
    uint32_t start = 1;
    while((start < ((pad / 2u) - 1u)) && (magnitude[start] < magnitude[start - 1u]))
    {
        start++;
    }

    real_t highest = REAL_C(0.0);
    for(uint32_t bin = start; bin < (pad / 2u); bin++)
    {
        if(magnitude[bin] > highest)
        {
            highest = magnitude[bin];
        }
    }

    return REAL_C(20.0) * REAL_LOG10(highest / magnitude[0]);
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
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), buffer[index]);
    }
}

void test_window_hann_falls_to_nothing_at_both_ends(void)
{
    window_build(buffer, SIZE, WINDOW_HANN);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), buffer[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), buffer[SIZE - 1u]);
    // The middle of a window of an even size stands beside the peak, thus it
    // is near one and not exactly one.
    TEST_ASSERT_TRUE(buffer[SIZE / 2u] > REAL_C(0.99));
}

void test_window_hamming_does_not_fall_to_nothing(void)
{
    window_build(buffer, SIZE, WINDOW_HAMMING);

    // This is what parts a Hamming window from a Hann one: its ends hold 0.08.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.08), buffer[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.08), buffer[SIZE - 1u]);
}

void test_every_window_is_symmetric(void)
{
    window_kind_t kind[] = {WINDOW_HANN, WINDOW_HAMMING, WINDOW_BLACKMAN,
                            WINDOW_BLACKMAN_HARRIS, WINDOW_TUKEY, WINDOW_KAISER};

    for(uint32_t k = 0; k < 6u; k++)
    {
        window_build_with(buffer, SIZE, kind[k], REAL_C(5.0));

        for(uint32_t index = 0; index < (SIZE / 2u); index++)
        {
            TEST_ASSERT_REAL_WITHIN(TOLERANCE, buffer[index],
                                     buffer[SIZE - 1u - index]);
        }
    }
}

void test_window_of_one_sample_holds_one(void)
{
    real_t single = REAL_C(2.0);

    window_build(&single, 1u, WINDOW_HANN);

    // A window of one sample cannot fall at its ends, thus it must not give
    // zero. Zero would take the whole signal away.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), single);
}

void test_window_value_outside_the_window_is_nothing(void)
{
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0),
                             window_value(SIZE, SIZE, WINDOW_HANN, REAL_C(0.0)));
}

void test_window_tukey_at_nothing_is_rectangular(void)
{
    window_build_with(buffer, SIZE, WINDOW_TUKEY, REAL_C(0.0));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), buffer[index]);
    }
}

void test_window_tukey_at_one_is_hann(void)
{
    real_t hann[SIZE];

    window_build_with(buffer, SIZE, WINDOW_TUKEY, REAL_C(1.0));
    window_build(hann, SIZE, WINDOW_HANN);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, hann[index], buffer[index]);
    }
}

void test_window_tukey_holds_its_middle(void)
{
    window_build_with(buffer, SIZE, WINDOW_TUKEY, REAL_C(0.5));

    // Half of the window falls, thus the middle half stays at one.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), buffer[SIZE / 2u]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), buffer[0]);
}

void test_window_kaiser_at_nothing_is_rectangular(void)
{
    window_build_with(buffer, SIZE, WINDOW_KAISER, REAL_C(0.0));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), buffer[index]);
    }
}

void test_window_kaiser_falls_further_as_beta_grows(void)
{
    real_t small[SIZE];
    real_t large[SIZE];

    window_build_with(small, SIZE, WINDOW_KAISER, REAL_C(2.0));
    window_build_with(large, SIZE, WINDOW_KAISER, REAL_C(8.0));

    TEST_ASSERT_TRUE(large[0] < small[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), large[SIZE / 2u]);
}

void test_window_kaiser_beta_follows_the_rule(void)
{
    // The rule of Kaiser, at the two values that the header names.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(5.653), window_kaiser_beta(REAL_C(60.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), window_kaiser_beta(REAL_C(20.0)));
    TEST_ASSERT_TRUE(window_kaiser_beta(REAL_C(100.0)) > window_kaiser_beta(REAL_C(60.0)));

    // The sign of the level must not matter.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), window_kaiser_beta(REAL_C(60.0)),
                             window_kaiser_beta(-REAL_C(60.0)));
}

void test_window_coherent_gain(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), window_coherent_gain(buffer, SIZE));

    // A Hann window halves the height of a tone. This is the number that a
    // reading must be divided by, and forgetting it is the usual fault.
    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.5), window_coherent_gain(buffer, SIZE));
}

void test_window_noise_gain(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), window_noise_gain(buffer, SIZE));

    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.6076), window_noise_gain(buffer, SIZE));
}

void test_window_noise_bandwidth(void)
{
    window_build(buffer, SIZE, WINDOW_RECTANGULAR);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), window_noise_bandwidth(buffer, SIZE));

    // A Hann window gives 1.5 bins. This is the known value of the window.
    window_build(buffer, SIZE, WINDOW_HANN);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), REAL_C(1.5), window_noise_bandwidth(buffer, SIZE));

    window_build(buffer, SIZE, WINDOW_BLACKMAN);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), REAL_C(1.73), window_noise_bandwidth(buffer, SIZE));
}

void test_window_apply(void)
{
    real_t input[4] = {REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0)};
    real_t window[4] = {REAL_C(0.0), REAL_C(0.5), REAL_C(0.5), REAL_C(0.0)};
    real_t output[4];

    window_apply(window, input, output, 4u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), output[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), output[3]);
}

void test_window_apply_can_write_over_its_input(void)
{
    real_t signal[4] = {REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0)};
    real_t window[4] = {REAL_C(0.0), REAL_C(0.5), REAL_C(0.5), REAL_C(0.0)};

    window_apply(window, signal, signal, 4u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), signal[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), signal[1]);
}

void test_the_side_lobes_are_where_the_table_says(void)
{
    // The table in the header sets out what each window is for. If these
    // numbers move, that table is no longer true and the reader is misled.
    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), -REAL_C(13.3),
                             highest_side_lobe(WINDOW_RECTANGULAR, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), -REAL_C(31.5), highest_side_lobe(WINDOW_HANN, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), -REAL_C(42.4), highest_side_lobe(WINDOW_HAMMING, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), -REAL_C(58.1), highest_side_lobe(WINDOW_BLACKMAN, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(3.0), -REAL_C(92.1),
                             highest_side_lobe(WINDOW_BLACKMAN_HARRIS, REAL_C(0.0)));
}

void test_the_side_lobes_of_kaiser_are_not_the_stop_band_of_its_rule(void)
{
    // The header warns that these two numbers are far apart, and this test
    // holds that warning true. A beta for a stop band of 60 dB gives a window
    // whose own side lobes stand near 42 dB down, not 60.
    real_t beta = window_kaiser_beta(REAL_C(60.0));

    TEST_ASSERT_REAL_WITHIN(REAL_C(2.0), -REAL_C(41.6), highest_side_lobe(WINDOW_KAISER, beta));
}

void test_window_is_valid_size(void)
{
    // A symmetric window of two values is its two ends, and the ends are where
    // a taper is nothing. The values are right; two ends really are all there
    // is. But the header tells a caller to DIVIDE by the coherent gain, and
    // for a hann window of 2 that is a division by nothing.
    TEST_ASSERT_EQUAL(false, window_is_valid_size(2, WINDOW_HANN));
    TEST_ASSERT_EQUAL(false, window_is_valid_size(2, WINDOW_BLACKMAN));
    TEST_ASSERT_EQUAL(false, window_is_valid_size(2, WINDOW_KAISER));

    // A rectangular window takes nothing away, thus it has no ends to fall at.
    TEST_ASSERT_EQUAL(true, window_is_valid_size(2, WINDOW_RECTANGULAR));

    // A size of 1 is the single value 1, which is well defined and useful.
    TEST_ASSERT_EQUAL(true, window_is_valid_size(1, WINDOW_HANN));

    TEST_ASSERT_EQUAL(true, window_is_valid_size(3, WINDOW_HANN));
    TEST_ASSERT_EQUAL(true, window_is_valid_size(1024, WINDOW_BLACKMAN));

    TEST_ASSERT_EQUAL(false, window_is_valid_size(0, WINDOW_HANN));
    TEST_ASSERT_EQUAL(false, window_is_valid_size(8, (window_kind_t)99));
}

void test_a_size_the_module_accepts_has_a_gain_worth_dividing_by(void)
{
    // The reason window_is_valid_size exists at all.
    const window_kind_t kinds[5] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                                    WINDOW_HAMMING, WINDOW_BLACKMAN,
                                    WINDOW_BLACKMAN_HARRIS};
    real_t window[16];

    for(uint32_t which = 0; which < 5u; which++)
    {
        for(uint32_t size = 1; size <= 16u; size++)
        {
            if(!window_is_valid_size(size, kinds[which]))
            {
                continue;
            }

            window_build(window, size, kinds[which]);

            TEST_ASSERT_TRUE(window_coherent_gain(window, size)
                             > REAL_C(0.01));
        }
    }
}
