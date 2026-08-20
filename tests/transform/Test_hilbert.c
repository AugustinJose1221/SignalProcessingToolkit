#include "unity.h"
#include "real_assert.h"
#include "hilbert.h"
#include "fft.h"
#include "cnum.h"
#include <stdlib.h>
#include <math.h>

#define SIZE        128u
#define PI          REAL_C(3.14159265358979323846)

static fft_t fft;
static real_t signal[SIZE];
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
static void fill_cosine(real_t cycles, real_t amplitude)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal[index] = amplitude * REAL_COS((REAL_C(2.0)*PI*cycles*(real_t)index)/(real_t)SIZE);
    }
}

void test_hilbert_the_real_part_holds_the_signal_again(void)
{
    // The analytic signal holds the signal itself in the real part.
    fill_cosine(REAL_C(5.0), REAL_C(1.0));

    hilbert_analytic_signal(&fft, signal, analytic);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), signal[index], analytic[index].re);
    }
}

void test_hilbert_the_imaginary_part_of_a_cosine_is_a_sine(void)
{
    // The Hilbert transform moves a cosine by a quarter turn, thus it gives a
    // sine of the same frequency.
    const real_t cycles = REAL_C(8.0);
    fill_cosine(cycles, REAL_C(1.0));

    hilbert_analytic_signal(&fft, signal, analytic);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t expected = REAL_SIN((REAL_C(2.0)*PI*cycles*(real_t)index)/(real_t)SIZE);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), expected, analytic[index].im);
    }
}

void test_hilbert_the_amplitude_of_a_cosine_does_not_change(void)
{
    // A cosine of a fixed amplitude gives an envelope that does not change.
    fill_cosine(REAL_C(6.0), REAL_C(2.5));
    real_t amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.5), amplitude[index]);
    }
}

void test_hilbert_the_amplitude_follows_the_envelope(void)
{
    // A cosine whose amplitude falls from 2 to 1 must give an envelope that
    // falls in the same way.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t envelope = REAL_C(2.0) - ((real_t)index / (real_t)SIZE);
        signal[index] = envelope * REAL_COS((REAL_C(2.0)*PI*REAL_C(16.0)*(real_t)index)/(real_t)SIZE);
    }
    real_t amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    // Leave the ends out. The transform takes the signal as one that repeats,
    // thus the two ends of a signal that does not repeat hold an error.
    for(uint32_t index = 16; index < (SIZE - 16); index++)
    {
        real_t expected = REAL_C(2.0) - ((real_t)index / (real_t)SIZE);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), expected, amplitude[index]);
    }
}

void test_hilbert_the_amplitude_is_never_less_than_zero(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal[index] = (real_t)index - REAL_C(60.0);
    }
    real_t amplitude[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_amplitude(analytic, amplitude, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_TRUE(amplitude[index] >= REAL_C(0.0));
    }
}

void test_hilbert_the_phase_lies_between_minus_pi_and_pi(void)
{
    fill_cosine(REAL_C(9.0), REAL_C(1.0));
    real_t phase[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_phase(analytic, phase, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_TRUE(phase[index] >= -PI - REAL_C(0.001));
        TEST_ASSERT_TRUE(phase[index] <= PI + REAL_C(0.001));
    }
}

void test_hilbert_the_frequency_of_a_cosine_is_the_frequency_of_that_cosine(void)
{
    // A window of 128 points at 128 hertz holds one second. A cosine of 10
    // cycles over that window is 10 hertz.
    const real_t cycles = REAL_C(10.0);
    fill_cosine(cycles, REAL_C(1.0));
    real_t frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (real_t)SIZE);

    for(uint32_t index = 4; index < (SIZE - 5); index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), cycles, frequency[index]);
    }
}

void test_hilbert_the_frequency_follows_a_signal_that_gets_faster(void)
{
    // The frequency of this signal rises with the time. The frequency at the
    // end must be larger than the frequency at the start.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t time = (real_t)index / (real_t)SIZE;
        real_t phase = REAL_C(2.0)*PI*((REAL_C(5.0)*time) + (REAL_C(10.0)*time*time));
        signal[index] = REAL_COS(phase);
    }
    real_t frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (real_t)SIZE);

    real_t early = frequency[SIZE/4];
    real_t late = frequency[(3*SIZE)/4];

    TEST_ASSERT_TRUE(late > early);
    TEST_ASSERT_TRUE(early > REAL_C(0.0));
}

void test_hilbert_the_phase_jump_does_not_look_like_a_large_frequency(void)
{
    // The phase jumps from pi to -pi at each turn. Without a correction such a
    // jump would look like a very large frequency.
    fill_cosine(REAL_C(20.0), REAL_C(1.0));
    real_t frequency[SIZE];

    hilbert_analytic_signal(&fft, signal, analytic);
    hilbert_frequency(analytic, frequency, SIZE, (real_t)SIZE);

    // No frequency may go above half the sample rate.
    for(uint32_t index = 0; index < (SIZE - 1); index++)
    {
        TEST_ASSERT_TRUE(REAL_ABS(frequency[index]) <= (real_t)SIZE/REAL_C(2.0));
    }
}
