#include "unity.h"
#include "real_assert.h"
#include "csd.h"
#include "cnum.h"
#include "fft.h"
#include "window.h"
#include <math.h>

#define BLOCK       64u
#define SIZE        1024u
#define BINS        (BLOCK / 2u + 1u)
#define RATE        REAL_C(640.0)

static real_t first_signal[SIZE];
static real_t second_signal[SIZE];
static real_t coherence[BINS];
static cnum_t cross[BINS];

static uint32_t seed;

void setUp(void)
{
    seed = 20260824u;
}

void tearDown(void)
{

}

static real_t noise(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

void test_csd_is_valid_block(void)
{
    TEST_ASSERT_EQUAL(true, csd_is_valid_block(256));
    TEST_ASSERT_EQUAL(false, csd_is_valid_block(300));
}

void test_csd_block_count(void)
{
    csd_t csd = csd_alloc(BLOCK);

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    // 1024 samples, blocks of 64, a step of 32.
    TEST_ASSERT_EQUAL(31, csd_block_count(&csd, SIZE));
    TEST_ASSERT_EQUAL(1, csd_block_count(&csd, BLOCK));
    TEST_ASSERT_EQUAL(0, csd_block_count(&csd, BLOCK - 1u));

    csd_free(&csd);
}

void test_csd_bin_frequency(void)
{
    csd_t csd = csd_alloc(BLOCK);

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(10.0),
                            csd_bin_frequency(&csd, 1, RATE));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(320.0),
                            csd_bin_frequency(&csd, 32, RATE));

    csd_free(&csd);
}

void test_a_signal_is_wholly_coherent_with_itself(void)
{
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = first_signal[index];
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, csd_coherence(&csd, first_signal, second_signal,
                                          SIZE, coherence));

    for(uint32_t bin = 0; bin < BINS; bin++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), coherence[bin]);
    }

    csd_free(&csd);
}

void test_two_signals_with_nothing_in_common_are_not_coherent(void)
{
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = noise();
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, csd_coherence(&csd, first_signal, second_signal,
                                          SIZE, coherence));

    // 31 blocks, thus a reading near 1/31 is what nothing in common looks
    // like. This is the number the header warns about and it is held here so
    // that nobody reads such a value as a relation.
    real_t total = REAL_C(0.0);

    for(uint32_t bin = 0; bin < BINS; bin++)
    {
        total += coherence[bin];
    }

    TEST_ASSERT_TRUE((total / (real_t)BINS) < REAL_C(0.15));

    csd_free(&csd);
}

void test_coherence_finds_the_one_frequency_that_two_signals_share(void)
{
    // THE QUESTION THE MODULE EXISTS FOR. Both signals hold a great deal of
    // noise and one tone in common. Neither signal on its own says which part
    // of it is shared.
    csd_t csd = csd_alloc(BLOCK);
    const uint32_t tone = 8u;

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t shared = REAL_SIN(REAL_C(2.0) * REAL_PI * (real_t)tone
                                 * (real_t)index / (real_t)BLOCK);

        first_signal[index] = shared + noise();
        second_signal[index] = shared + noise();
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));
    csd_coherence(&csd, first_signal, second_signal, SIZE, coherence);

    // At the tone the two signals explain each other well.
    TEST_ASSERT_TRUE(coherence[tone] > REAL_C(0.8));

    // Away from it they do not, though both signals are loud there.
    for(uint32_t bin = 0; bin < BINS; bin++)
    {
        if((bin < (tone - 2u)) || (bin > (tone + 2u)))
        {
            TEST_ASSERT_TRUE(coherence[bin] < REAL_C(0.5));
        }
    }

    csd_free(&csd);
}

void test_csd_refuses_too_few_blocks_to_mean_anything(void)
{
    // THE TRAP. A single block gives a coherence of exactly 1 for any two
    // signals whatever, because the arithmetic becomes a number divided by
    // itself. The module refuses rather than answering 1.
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = noise();
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    // Just enough samples for one block, and for one block less than the
    // smallest count that is allowed.
    TEST_ASSERT_EQUAL(false, csd_coherence(&csd, first_signal, second_signal,
                                           BLOCK, coherence));

    uint32_t few = ((CSD_SMALLEST_BLOCK_COUNT - 2u) * (BLOCK / 2u)) + BLOCK;

    TEST_ASSERT_EQUAL(false, csd_coherence(&csd, first_signal, second_signal,
                                           few, coherence));

    // One more block and it answers.
    uint32_t enough = ((CSD_SMALLEST_BLOCK_COUNT - 1u) * (BLOCK / 2u)) + BLOCK;

    TEST_ASSERT_EQUAL(true, csd_coherence(&csd, first_signal, second_signal,
                                          enough, coherence));

    csd_free(&csd);
}

void test_csd_estimate_puts_the_shared_power_where_it_belongs(void)
{
    csd_t csd = csd_alloc(BLOCK);
    const uint32_t tone = 8u;

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t shared = REAL_SIN(REAL_C(2.0) * REAL_PI * (real_t)tone
                                 * (real_t)index / (real_t)BLOCK);

        first_signal[index] = shared;
        second_signal[index] = shared;
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, csd_estimate(&csd, first_signal, second_signal,
                                         SIZE, RATE, cross));

    // Where both signals are the same, the cross spectrum is the density of
    // either one, thus the area under the peak is the power of the wave, which
    // for an amplitude of 1 is a half.
    real_t area = REAL_C(0.0);

    for(uint32_t bin = tone - 3u; bin <= (tone + 3u); bin++)
    {
        area += cnum_real(cross[bin]);
    }

    area *= RATE / (real_t)BLOCK;

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.5), area);

    // And the two signals being the same, nothing lags behind anything.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                            cnum_imaginary(cross[tone]));

    csd_free(&csd);
}

void test_csd_transfer_gives_the_gain_of_what_lies_between(void)
{
    // Measure in and out, and the gain at every frequency comes back at once.
    // Here what lies between is a plain doubling.
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = REAL_C(2.0) * first_signal[index];
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, csd_transfer(&csd, first_signal, second_signal,
                                         SIZE, cross));

    for(uint32_t bin = 1; bin < (BINS - 1u); bin++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.0),
                                cnum_magnitude(cross[bin]));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                                cnum_imaginary(cross[bin]));
    }

    csd_free(&csd);
}

void test_csd_transfer_is_blind_to_noise_added_after(void)
{
    // The reason this estimate is the one to use. Noise on the OUTPUT does not
    // bend the answer, because it has nothing in common with the input and
    // averages away.
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = (REAL_C(2.0) * first_signal[index])
                               + (REAL_C(0.5) * noise());
    }

    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));
    csd_transfer(&csd, first_signal, second_signal, SIZE, cross);

    // The gain still reads about 2 even with noise a quarter of its size on
    // the output.
    real_t total = REAL_C(0.0);

    for(uint32_t bin = 1; bin < (BINS - 1u); bin++)
    {
        total += cnum_magnitude(cross[bin]);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0),
                            total / (real_t)(BINS - 2u));

    csd_free(&csd);
}

void test_csd_refuses_what_it_cannot_answer(void)
{
    csd_t csd = csd_alloc(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = noise();
    }

    // Before it is designed there is no window and no overlap.
    TEST_ASSERT_EQUAL(false, csd_coherence(&csd, first_signal, second_signal,
                                           SIZE, coherence));

    // An overlap as long as the block would never move along the signal.
    TEST_ASSERT_EQUAL(false, csd_design(&csd, BLOCK, WINDOW_HANN,
                                        REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, csd_design(&csd, BLOCK / 2u, (window_kind_t)99,
                                        REAL_C(0.0)));

    TEST_ASSERT_EQUAL(true, csd_design(&csd, BLOCK / 2u, WINDOW_HANN,
                                       REAL_C(0.0)));

    // A density needs a sample rate.
    TEST_ASSERT_EQUAL(false, csd_estimate(&csd, first_signal, second_signal,
                                          SIZE, REAL_C(0.0), cross));

    csd_free(&csd);
}

void test_csd_static_alloc(void)
{
    static real_t window[BLOCK];
    static real_t windowed[BLOCK];
    static cnum_t first[BLOCK];
    static cnum_t second[BLOCK];
    static cnum_t shared[BINS];
    static real_t first_power[BINS];
    static real_t second_power[BINS];
    static cnum_t twiddle[BLOCK / 2u];
    static uint32_t reverse[BLOCK];

    fft_t fft = fft_static_alloc(BLOCK, twiddle, reverse);
    csd_t csd = csd_static_alloc(BLOCK, window, windowed, first, second,
                                 shared, first_power, second_power, fft);

    TEST_ASSERT_EQUAL(BLOCK, csd.block);
    TEST_ASSERT_EQUAL(true, csd_design(&csd, BLOCK / 2u, WINDOW_HANN,
                                       REAL_C(0.0)));

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first_signal[index] = noise();
        second_signal[index] = first_signal[index];
    }

    TEST_ASSERT_EQUAL(true, csd_coherence(&csd, first_signal, second_signal,
                                          SIZE, coherence));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), coherence[4]);

    csd_free(&csd);
}
