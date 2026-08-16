#include "unity.h"
#include "hilbert.h"
#include "fft.h"
#include "cnum.h"
#include <stdlib.h>
#include <math.h>

#define SIZE        128u
#define PI          3.14159265358979323846f

static fft_t fft;
static float signal[SIZE];
static cnum_t analytic[SIZE];

void setUp(void)
{
    fft = fft_alloc(SIZE);
}

void tearDown(void)
{
    fft_free(&fft);
}

// A cosine of the given number of cycles over the window.
static void fill_cosine(float cycles, float amplitude)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal[index] = amplitude * cosf((2.0f*PI*cycles*(float)index)/(float)SIZE);
    }
}

void test_hilbert_the_real_part_holds_the_signal_again(void)
{
    // The analytic signal holds the signal itself in the real part.
    fill_cosine(5.0f, 1.0f);

    hilbert_analytic_signal(&fft, signal, analytic);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, signal[index], analytic[index].re);
    }
}

void test_hilbert_the_imaginary_part_of_a_cosine_is_a_sine(void)
{
    // The Hilbert transform moves a cosine by a quarter turn, thus it gives a
    // sine of the same frequency.
    const float cycles = 8.0f;
    fill_cosine(cycles, 1.0f);

    hilbert_analytic_signal(&fft, signal, analytic);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        float expected = sinf((2.0f*PI*cycles*(float)index)/(float)SIZE);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, analytic[index].im);
    }
}

void test_hilbert_the_amplitude_of_a_cosine_does_not_change(void)
{
    // A cosine of a fixed amplitude gives an envelope that does not change.
    fill_cosine(6.0f, 2.5f);
    float amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, amplitude[index]);
    }
}

void test_hilbert_the_amplitude_follows_the_envelope(void)
{
    // A cosine whose amplitude falls from 2 to 1 must give an envelope that
    // falls in the same way.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float envelope = 2.0f - ((float)index / (float)SIZE);
        signal[index] = envelope * cosf((2.0f*PI*16.0f*(float)index)/(float)SIZE);
    }
    float amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    // Leave the ends out. The transform takes the signal as one that repeats,
    // thus the two ends of a signal that does not repeat hold an error.
    for(uint32_t index = 16; index < (SIZE - 16); index++)
    {
        float expected = 2.0f - ((float)index / (float)SIZE);
        TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, amplitude[index]);
    }
}

void test_hilbert_the_amplitude_is_never_less_than_zero(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal[index] = (float)index - 60.0f;
    }
    float amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_TRUE(amplitude[index] >= 0.0f);
    }
}

void test_hilbert_the_phase_lies_between_minus_pi_and_pi(void)
{
    fill_cosine(9.0f, 1.0f);
    float phase[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_phase(analytic, phase, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_TRUE(phase[index] >= -PI - 0.001f);
        TEST_ASSERT_TRUE(phase[index] <= PI + 0.001f);
    }
}

void test_hilbert_the_frequency_of_a_cosine_is_the_frequency_of_that_cosine(void)
{
    // A window of 128 points at 128 hertz holds one second. A cosine of 10
    // cycles over that window is 10 hertz.
    const float cycles = 10.0f;
    fill_cosine(cycles, 1.0f);
    float frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (float)SIZE);

    for(uint32_t index = 4; index < (SIZE - 5); index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.1f, cycles, frequency[index]);
    }
}

void test_hilbert_the_frequency_follows_a_signal_that_gets_faster(void)
{
    // The frequency of this signal rises with the time. The frequency at the
    // end must be larger than the frequency at the start.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float time = (float)index / (float)SIZE;
        float phase = 2.0f*PI*((5.0f*time) + (10.0f*time*time));
        signal[index] = cosf(phase);
    }
    float frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (float)SIZE);

    float early = frequency[SIZE/4];
    float late = frequency[(3*SIZE)/4];

    TEST_ASSERT_TRUE(late > early);
    TEST_ASSERT_TRUE(early > 0.0f);
}

void test_hilbert_the_phase_jump_does_not_look_like_a_large_frequency(void)
{
    // The phase jumps from pi to -pi at each turn. Without a correction such a
    // jump would look like a very large frequency.
    fill_cosine(20.0f, 1.0f);
    float frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (float)SIZE);

    // No frequency may go above half the sample rate.
    for(uint32_t index = 0; index < (SIZE - 1); index++)
    {
        TEST_ASSERT_TRUE(fabsf(frequency[index]) <= (float)SIZE/2.0f);
    }
}
