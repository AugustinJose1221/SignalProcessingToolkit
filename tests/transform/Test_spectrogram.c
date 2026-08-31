#include "unity.h"
#include "real_assert.h"
#include "spectrogram.h"
#include "stft.h"
#include "cnum.h"
#include "fft.h"
#include "window.h"
#include <math.h>
#include <stdlib.h>

#define BLOCK       64u
#define SIZE        256u
#define BINS        (BLOCK / 2u + 1u)
#define TONE        8u

static real_t signal_buffer[SIZE];
static cnum_t frames[SIZE * BINS];
static real_t values[SIZE * BINS];

void setUp(void)
{

}

void tearDown(void)
{

}

// Build a wave of the given amplitude sitting exactly on bin TONE, run the
// short-time transform over it, and give back the number of frames.
static uint32_t transform_a_tone(stft_t* stft, real_t amplitude,
                                 window_kind_t kind)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal_buffer[index] = amplitude
                               * REAL_SIN(REAL_C(2.0) * REAL_PI * (real_t)TONE
                                          * (real_t)index / (real_t)BLOCK);
    }

    stft_design(stft, BLOCK / 2u, kind, REAL_C(0.0));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);

    stft_forward(stft, signal_buffer, SIZE, frames, count * BINS);

    return count;
}

void test_spectrogram_is_valid_kind(void)
{
    TEST_ASSERT_EQUAL(true, spectrogram_is_valid_kind(SPECTROGRAM_AMPLITUDE));
    TEST_ASSERT_EQUAL(true, spectrogram_is_valid_kind(SPECTROGRAM_POWER));
    TEST_ASSERT_EQUAL(true, spectrogram_is_valid_kind(SPECTROGRAM_DENSITY));
    TEST_ASSERT_EQUAL(true, spectrogram_is_valid_kind(SPECTROGRAM_DECIBEL));
    TEST_ASSERT_EQUAL(false, spectrogram_is_valid_kind((spectrogram_kind_t)9));
}

void test_spectrogram_value_count(void)
{
    stft_t stft = stft_alloc(BLOCK);

    TEST_ASSERT_EQUAL(BINS * 5u, spectrogram_value_count(&stft, 5u));

    stft_free(&stft);
}

void test_a_wave_of_amplitude_two_reads_two_whatever_the_window(void)
{
    // THE ONE THING THIS MODULE IS FOR. A transform of a longer block gives
    // larger numbers for the same signal and a window makes them smaller.
    // Corrected, the reading is the amplitude of the wave and nothing else.
    const window_kind_t kinds[3] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                                    WINDOW_BLACKMAN};

    for(uint32_t which = 0; which < 3u; which++)
    {
        stft_t stft = stft_alloc(BLOCK);
        uint32_t count = transform_a_tone(&stft, REAL_C(2.0), kinds[which]);

        TEST_ASSERT_EQUAL(true,
                          spectrogram_build(&stft, frames, count,
                                            SPECTROGRAM_AMPLITUDE,
                                            REAL_C(1000.0), values,
                                            count * BINS));

        // The second frame, which is well inside the signal.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.0),
                                values[BINS + TONE]);

        stft_free(&stft);
    }
}

void test_spectrogram_power_is_half_the_square_of_the_amplitude(void)
{
    stft_t stft = stft_alloc(BLOCK);
    uint32_t count = transform_a_tone(&stft, REAL_C(2.0), WINDOW_HANN);

    spectrogram_build(&stft, frames, count, SPECTROGRAM_POWER,
                      REAL_C(1000.0), values, count * BINS);

    // A wave of amplitude 2 has a mean power of 2, because it spends half its
    // time below the middle.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(2.0), values[BINS + TONE]);

    stft_free(&stft);
}

void test_spectrogram_bins_that_hold_nothing_read_as_nothing(void)
{
    stft_t stft = stft_alloc(BLOCK);
    uint32_t count = transform_a_tone(&stft, REAL_C(2.0), WINDOW_HANN);

    spectrogram_build(&stft, frames, count, SPECTROGRAM_AMPLITUDE,
                      REAL_C(1000.0), values, count * BINS);

    // Away from the tone and away from the two bins beside it, where the
    // window spreads a little, there is nothing.
    for(uint32_t bin = 0; bin < BINS; bin++)
    {
        if((bin < (TONE - 2u)) || (bin > (TONE + 2u)))
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(0.0),
                                    values[BINS + bin]);
        }
    }

    stft_free(&stft);
}

void test_spectrogram_decibels_have_a_floor_under_them(void)
{
    // A silent stretch is a thing that happens, and the logarithm of nothing
    // has no value. Without a floor the answer holds numbers that no picture
    // and no arithmetic can use.
    stft_t stft = stft_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal_buffer[index] = REAL_C(0.0);
    }

    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);
    stft_forward(&stft, signal_buffer, SIZE, frames, count * BINS);

    TEST_ASSERT_EQUAL(true,
                      spectrogram_build(&stft, frames, count,
                                        SPECTROGRAM_DECIBEL, REAL_C(1000.0),
                                        values, count * BINS));

    for(uint32_t index = 0; index < (count * BINS); index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), SPECTROGRAM_FLOOR_DECIBEL,
                                values[index]);
    }

    stft_free(&stft);
}

void test_spectrogram_decibels_of_a_known_wave(void)
{
    stft_t stft = stft_alloc(BLOCK);

    // An amplitude of the square root of two has a power of exactly 1, which
    // is 0 decibels.
    uint32_t count = transform_a_tone(&stft, REAL_SQRT(REAL_C(2.0)),
                                      WINDOW_HANN);

    spectrogram_build(&stft, frames, count, SPECTROGRAM_DECIBEL,
                      REAL_C(1000.0), values, count * BINS);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(0.0), values[BINS + TONE]);

    stft_free(&stft);
}

void test_spectrogram_largest_and_against_the_largest(void)
{
    stft_t stft = stft_alloc(BLOCK);
    uint32_t count = transform_a_tone(&stft, REAL_C(2.0), WINDOW_HANN);

    spectrogram_build(&stft, frames, count, SPECTROGRAM_DECIBEL,
                      REAL_C(1000.0), values, count * BINS);

    real_t largest = spectrogram_largest(values, count * BINS);

    TEST_ASSERT_EQUAL(true, spectrogram_against_the_largest(values,
                                                            count * BINS,
                                                            values));

    // The largest now reads 0 and nothing stands above it.
    real_t after = spectrogram_largest(values, count * BINS);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0), after);

    // And the reading that was largest before is the one at 0 now.
    TEST_ASSERT_TRUE(largest > REAL_C(0.0));

    for(uint32_t index = 0; index < (count * BINS); index++)
    {
        TEST_ASSERT_TRUE(values[index] <= REAL_C(0.001));
        TEST_ASSERT_TRUE(values[index] >= SPECTROGRAM_FLOOR_DECIBEL);
    }

    stft_free(&stft);
}

void test_spectrogram_density_does_not_move_when_the_block_gets_longer(void)
{
    // The density is power for each hertz, thus it is the one unit here that
    // does not change with the block. This holds that across a block of 64 and
    // a block of 256.
    const uint32_t blocks[2] = {64u, 256u};
    real_t reading[2];

    for(uint32_t which = 0; which < 2u; which++)
    {
        uint32_t block = blocks[which];
        uint32_t bins = STFT_BIN_COUNT(block);
        uint32_t tone = block / 8u;
        stft_t stft = stft_alloc(block);

        for(uint32_t index = 0; index < SIZE; index++)
        {
            signal_buffer[index] = REAL_C(2.0)
                                   * REAL_SIN(REAL_C(2.0) * REAL_PI
                                              * (real_t)tone * (real_t)index
                                              / (real_t)block);
        }

        stft_design(&stft, block / 2u, WINDOW_HANN, REAL_C(0.0));

        uint32_t count = stft_frame_count(SIZE, block, block / 2u);
        stft_forward(&stft, signal_buffer, SIZE, frames, count * bins);
        spectrogram_build(&stft, frames, count, SPECTROGRAM_DENSITY,
                          REAL_C(1000.0), values, count * bins);

        // The area under the curve is what a density means, thus the whole
        // peak is added and not the one bin at the top of it.
        //
        // A block of 256 in a signal of 256 gives one frame only, thus the
        // frame is chosen from the count and not written as 1.
        uint32_t frame = count / 2u;

        reading[which] = REAL_C(0.0);

        for(uint32_t bin = tone - 3u; bin <= (tone + 3u); bin++)
        {
            reading[which] += values[(frame * bins) + bin];
        }

        // Each bin stands for one bin width of hertz.
        reading[which] *= REAL_C(1000.0) / (real_t)block;

        stft_free(&stft);
    }

    // A wave of amplitude 2 has an area of 2 under the curve, at either block.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0), reading[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0), reading[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), reading[0], reading[1]);
}

void test_spectrogram_refuses_what_it_cannot_answer(void)
{
    stft_t stft = stft_alloc(BLOCK);

    // Before it is designed there is no window to correct for.
    TEST_ASSERT_EQUAL(false, spectrogram_build(&stft, frames, 2u,
                                               SPECTROGRAM_AMPLITUDE,
                                               REAL_C(1000.0), values,
                                               SIZE * BINS));

    uint32_t count = transform_a_tone(&stft, REAL_C(1.0), WINDOW_HANN);

    TEST_ASSERT_EQUAL(false, spectrogram_build(&stft, frames, count,
                                               (spectrogram_kind_t)9,
                                               REAL_C(1000.0), values,
                                               count * BINS));

    // Room for one value less than the answer needs.
    TEST_ASSERT_EQUAL(false, spectrogram_build(&stft, frames, count,
                                               SPECTROGRAM_AMPLITUDE,
                                               REAL_C(1000.0), values,
                                               (count * BINS) - 1u));

    // A density needs a sample rate, and the other units do not.
    TEST_ASSERT_EQUAL(false, spectrogram_build(&stft, frames, count,
                                               SPECTROGRAM_DENSITY,
                                               REAL_C(0.0), values,
                                               count * BINS));
    TEST_ASSERT_EQUAL(true, spectrogram_build(&stft, frames, count,
                                              SPECTROGRAM_AMPLITUDE,
                                              REAL_C(0.0), values,
                                              count * BINS));

    TEST_ASSERT_EQUAL(false, spectrogram_build(&stft, frames, 0u,
                                               SPECTROGRAM_AMPLITUDE,
                                               REAL_C(1000.0), values,
                                               count * BINS));

    stft_free(&stft);
}

void test_a_transform_that_was_never_built_has_no_values_to_count(void)
{
    stft_t stft = stft_alloc(63u);

    TEST_ASSERT_EQUAL(0, spectrogram_value_count(&stft, 4u));

    stft_free(&stft);
}

void test_a_picture_of_no_values_is_refused(void)
{
    real_t nothing[1] = {REAL_C(0.0)};
    real_t room[1];

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), spectrogram_largest(nothing, 0u));
    TEST_ASSERT_FALSE(spectrogram_against_the_largest(nothing, 0u, room));
}

void test_silence_reads_at_the_floor_and_no_lower(void)
{
    // A bin that holds nothing has a level of minus infinity, which cannot be
    // drawn and cannot be held in a float. The module stops at its floor.
    //
    // Every bin of a spectrogram of silence must sit exactly there: not below
    // it, and not at some very large negative number that a plot would stretch
    // itself around.
    stft_t stft = stft_alloc(BLOCK);
    TEST_ASSERT_TRUE(stft_design(&stft, BLOCK / 4u, WINDOW_HANN, REAL_C(0.0)));

    uint32_t frames = 4u;
    uint32_t bins = STFT_BIN_COUNT(BLOCK);
    uint32_t count = spectrogram_value_count(&stft, frames);

    cnum_t* spectrum = (cnum_t*)malloc(sizeof(cnum_t) * frames * bins);
    real_t* values = (real_t*)malloc(sizeof(real_t) * count);

    for(uint32_t index = 0; index < (frames * bins); index++)
    {
        spectrum[index] = cnum_zero();
    }

    TEST_ASSERT_TRUE(spectrogram_build(&stft, spectrum, frames,
                                       SPECTROGRAM_DECIBEL, REAL_C(1000.0),
                                       values, count));

    for(uint32_t index = 0; index < count; index++)
    {
        TEST_ASSERT_EQUAL_REAL(SPECTROGRAM_FLOOR_DECIBEL, values[index]);
    }

    free(spectrum);
    free(values);
    stft_free(&stft);
}
