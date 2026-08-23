#include "unity.h"
#include "real_assert.h"
#include "filtfilt.h"
#include "iir.h"
#include "fir.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265358979323846)
#define SIZE        400u

static real_t input[SIZE];
static real_t output[SIZE];

void setUp(void)
{

}

void tearDown(void)
{

}

// Where the largest value of a signal stands.
static uint32_t place_of_peak(const real_t* data, uint32_t size)
{
    uint32_t best = 0;

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] > data[best]) { best = index; }
    }

    return best;
}

void test_filtfilt_padding(void)
{
    // Three times the filter, or as much of the signal as there is.
    TEST_ASSERT_EQUAL(30, filtfilt_padding(10, 400));
    TEST_ASSERT_EQUAL(19, filtfilt_padding(10, 20));
    TEST_ASSERT_EQUAL(0, filtfilt_padding(10, 0));
}

void test_a_peak_does_not_move(void)
{
    // The whole point. A single filter moves the peak; running both ways does
    // not, thus a measurement of WHERE something happened survives filtering.
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.05));

    for(uint32_t index = 0; index < SIZE; index++) { input[index] = REAL_C(0.0); }
    // A smooth peak, wide enough to pass the filter.
    for(uint32_t index = 180; index < 220u; index++)
    {
        input[index] = REAL_SIN(PI * (real_t)(index - 180u) / REAL_C(40.0));
    }

    uint32_t was = place_of_peak(input, SIZE);

    // One pass moves it.
    iir_reset(&iir);
    real_t once[SIZE];
    for(uint32_t index = 0; index < SIZE; index++)
    {
        once[index] = iir_process_sample(&iir, input[index]);
    }
    uint32_t after_one = place_of_peak(once, SIZE);
    TEST_ASSERT_TRUE(after_one > (was + 3u));

    // Both ways does not.
    TEST_ASSERT_EQUAL(true, filtfilt_iir(&iir, input, output, SIZE));
    uint32_t after_both = place_of_peak(output, SIZE);
    TEST_ASSERT_TRUE((after_both + 1u) >= was);
    TEST_ASSERT_TRUE(after_both <= (was + 1u));

    iir_free(&iir);
}

void test_a_wave_comes_out_lined_up_with_the_one_that_went_in(void)
{
    // The same thing measured on a wave: what comes out must match what went
    // in sample for sample, not merely have the same shape somewhere else.
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.1));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        input[index] = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02)
                                * (real_t)index);
    }

    filtfilt_iir(&iir, input, output, SIZE);

    // Well inside the band the wave passes almost untouched, and it must line
    // up exactly. The two ends are left out; they are examined on their own.
    real_t largest = REAL_C(0.0);
    for(uint32_t index = 50; index < (SIZE - 50u); index++)
    {
        real_t error = REAL_ABS(output[index] - input[index]);
        if(error > largest) { largest = error; }
    }

    TEST_ASSERT_TRUE(largest < REAL_C(0.02));

    iir_free(&iir);
}

void test_the_gain_is_squared(void)
{
    // A filter run twice does what it does twice. At the cutoff it passes
    // 0.707 in one pass and 0.5 in two, thus the band is narrower than the one
    // that was designed. A caller must know that, and filtfilt_gain says it.
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.1));

    real_t once = iir_get_gain(&iir, REAL_C(0.1));
    real_t twice = filtfilt_iir_gain(&iir, REAL_C(0.1));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.7071), once);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.5), twice);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), once * once, twice);

    iir_free(&iir);
}

void test_the_gain_that_is_squared_is_what_really_happens(void)
{
    // The number that filtfilt_gain gives must be what the signal really sees,
    // not a claim about the arithmetic.
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.1));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        input[index] = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.1) * (real_t)index);
    }

    filtfilt_iir(&iir, input, output, SIZE);

    real_t largest = REAL_C(0.0);
    for(uint32_t index = 100; index < (SIZE - 100u); index++)
    {
        if(REAL_ABS(output[index]) > largest) { largest = REAL_ABS(output[index]); }
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), filtfilt_iir_gain(&iir, REAL_C(0.1)),
                            largest);

    iir_free(&iir);
}

void test_the_two_ends_hold_no_false_swing(void)
{
    // A filter that started from nothing would answer the first sample as a
    // step, and running both ways would put that swing at BOTH ends. The
    // signal is carried outwards past each end to stop it.
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.05));

    // A signal that sits far from zero, which is where a filter starting from
    // nothing goes most wrong.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        input[index] = REAL_C(100.0);
    }

    filtfilt_iir(&iir, input, output, SIZE);

    // Every sample, the two ends among them, must still be 100.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), REAL_C(100.0), output[index]);
    }

    iir_free(&iir);
}

void test_filtfilt_can_write_over_its_input(void)
{
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.1));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        input[index] = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02)
                                * (real_t)index);
        output[index] = input[index];
    }

    real_t apart[SIZE];
    filtfilt_iir(&iir, input, apart, SIZE);
    filtfilt_iir(&iir, output, output, SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), apart[index], output[index]);
    }

    iir_free(&iir);
}

void test_filtfilt_refuses_a_signal_that_is_too_short(void)
{
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(0.1));

    // Two sections hold four samples of state.
    TEST_ASSERT_EQUAL(false, filtfilt_iir(&iir, input, output, 4u));
    TEST_ASSERT_EQUAL(true, filtfilt_iir(&iir, input, output, 5u));

    iir_free(&iir);
}

void test_filtfilt_with_a_finite_impulse_response(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, REAL_C(0.1));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        input[index] = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.02)
                                * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, filtfilt_fir(&fir, input, output, SIZE));

    // Lined up, as with the other kind.
    real_t largest = REAL_C(0.0);
    for(uint32_t index = 60; index < (SIZE - 60u); index++)
    {
        real_t error = REAL_ABS(output[index] - input[index]);
        if(error > largest) { largest = error; }
    }
    TEST_ASSERT_TRUE(largest < REAL_C(0.02));

    // And its gain is squared as well.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                            fir_get_gain(&fir, REAL_C(0.05))
                            * fir_get_gain(&fir, REAL_C(0.05)),
                            filtfilt_fir_gain(&fir, REAL_C(0.05)));

    fir_free(&fir);
}

void test_filtfilt_fir_refuses_a_signal_that_is_too_short(void)
{
    fir_t fir = fir_alloc(31);
    fir_design_low_pass(&fir, REAL_C(0.1));

    TEST_ASSERT_EQUAL(false, filtfilt_fir(&fir, input, output, 31u));
    TEST_ASSERT_EQUAL(true, filtfilt_fir(&fir, input, output, 32u));

    fir_free(&fir);
}

void test_the_edges_of_the_band_are_steeper_than_one_pass(void)
{
    // What is bought for the two prices: the same filter makes a turn twice as
    // steep, thus a caller can have a sharper edge without a longer filter.
    iir_t iir = iir_alloc(1);
    iir_design_low_pass(&iir, REAL_C(0.1));

    // Well into the stop band, twice through lets far less past.
    real_t once = iir_get_gain(&iir, REAL_C(0.3));
    real_t twice = filtfilt_iir_gain(&iir, REAL_C(0.3));

    TEST_ASSERT_TRUE(twice < (REAL_C(0.5) * once));

    iir_free(&iir);
}
