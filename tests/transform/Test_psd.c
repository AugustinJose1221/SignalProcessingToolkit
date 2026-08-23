#include "unity.h"
#include "real_assert.h"
#include "psd.h"
#include "fft.h"
#include "window.h"
#include "cnum.h"
#include <stdlib.h>
#include <math.h>

#define PI          REAL_C(3.14159265358979323846)
#define RATE        REAL_C(1000.0)

static real_t signal[4096];

void setUp(void)
{

}

void tearDown(void)
{

}

// A wave of the given amplitude at the given frequency.
static void make_wave(real_t amplitude, real_t frequency, uint32_t size)
{
    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = amplitude
                        * REAL_SIN(REAL_C(2.0) * PI * frequency
                                   * (real_t)index / RATE);
    }
}

void test_psd_is_valid_block(void)
{
    TEST_ASSERT_EQUAL(true, psd_is_valid_block(256));
    TEST_ASSERT_EQUAL(true, psd_is_valid_block(4));
    // The transform takes a power of two only.
    TEST_ASSERT_EQUAL(false, psd_is_valid_block(100));
    TEST_ASSERT_EQUAL(false, psd_is_valid_block(2));
    TEST_ASSERT_EQUAL(false, psd_is_valid_block(0));
}

void test_psd_alloc_and_the_counts(void)
{
    psd_t psd = psd_alloc(256);

    TEST_ASSERT_EQUAL(256, psd.block);
    // Half the block plus one: from zero frequency up to half the rate.
    TEST_ASSERT_EQUAL(129, psd_bin_count(&psd));
    TEST_ASSERT_EQUAL(true, psd.dynamic_alloc);

    // 1024 samples, blocks of 256 that step by 128, thus 7 blocks.
    TEST_ASSERT_EQUAL(7, psd_block_count(&psd, 1024));
    TEST_ASSERT_EQUAL(1, psd_block_count(&psd, 256));
    TEST_ASSERT_EQUAL(0, psd_block_count(&psd, 255));

    psd_free(&psd);
}

void test_psd_static_alloc(void)
{
    real_t window[64];
    real_t windowed[64];
    cnum_t spectrum[64];
    fft_t fft = fft_alloc(64);

    psd_t psd = psd_static_alloc(64, window, windowed, spectrum, fft);

    TEST_ASSERT_EQUAL(false, psd.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(window, psd.window);

    psd_free(&psd);
    TEST_ASSERT_EQUAL_PTR(window, psd.window);

    fft_free(&fft);
}

void test_psd_design_refuses_what_it_cannot_do(void)
{
    psd_t psd = psd_alloc(64);

    TEST_ASSERT_EQUAL(true, psd_design(&psd, 32, WINDOW_HANN, REAL_C(0.0)));
    // The overlap must be below the block, or no block would ever move on.
    TEST_ASSERT_EQUAL(false, psd_design(&psd, 64, WINDOW_HANN, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, psd_design(&psd, 32,
                          (window_kind_t)(WINDOW_KAISER + 1), REAL_C(0.0)));

    psd_free(&psd);
}

void test_psd_bin_frequency_and_width(void)
{
    psd_t psd = psd_alloc(256);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            psd_bin_frequency(&psd, 0, RATE));
    // The last bin stands at half the sample rate.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), RATE / REAL_C(2.0),
                            psd_bin_frequency(&psd, 128, RATE));
    // A block of 256 at 1000 Hz gives bins about 3.9 Hz wide.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1000.0) / REAL_C(256.0),
                            psd_bin_width(&psd, RATE));

    psd_free(&psd);
}

void test_the_power_of_a_wave_does_not_depend_on_the_block(void)
{
    // The whole reason the scaling exists. A wave of amplitude A holds power
    // A*A/2, and that number must come out the same whatever the block.
    const real_t amplitude = REAL_C(3.0);
    const real_t expected = (amplitude * amplitude) / REAL_C(2.0);
    uint32_t blocks[3] = {128u, 256u, 512u};

    make_wave(amplitude, REAL_C(100.0), 4096u);

    for(uint32_t which = 0; which < 3u; which++)
    {
        psd_t psd = psd_alloc(blocks[which]);
        real_t* density = (real_t*)malloc(sizeof(real_t) * psd_bin_count(&psd));

        TEST_ASSERT_EQUAL(true, psd_estimate(&psd, signal, 4096u, RATE, density));

        real_t power = psd_band_power(&psd, density, RATE,
                                      REAL_C(80.0), REAL_C(120.0));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.05) * expected, expected, power);

        free(density);
        psd_free(&psd);
    }
}

void test_the_power_of_a_wave_does_not_depend_on_the_window(void)
{
    // Nor on the window, which is the correction that is most often left out.
    const real_t amplitude = REAL_C(2.0);
    const real_t expected = (amplitude * amplitude) / REAL_C(2.0);
    window_kind_t kinds[4] = {WINDOW_RECTANGULAR, WINDOW_HANN,
                              WINDOW_HAMMING, WINDOW_BLACKMAN};

    make_wave(amplitude, REAL_C(125.0), 4096u);

    for(uint32_t which = 0; which < 4u; which++)
    {
        psd_t psd = psd_alloc(256);
        real_t density[129];

        TEST_ASSERT_EQUAL(true, psd_design(&psd, 128, kinds[which], REAL_C(0.0)));
        TEST_ASSERT_EQUAL(true, psd_estimate(&psd, signal, 4096u, RATE, density));

        real_t power = psd_band_power(&psd, density, RATE,
                                      REAL_C(100.0), REAL_C(150.0));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.06) * expected, expected, power);

        psd_free(&psd);
    }
}

void test_the_power_of_a_wave_does_not_depend_on_the_overlap(void)
{
    const real_t amplitude = REAL_C(1.0);
    const real_t expected = (amplitude * amplitude) / REAL_C(2.0);
    uint32_t overlaps[3] = {0u, 128u, 192u};

    make_wave(amplitude, REAL_C(125.0), 4096u);

    for(uint32_t which = 0; which < 3u; which++)
    {
        psd_t psd = psd_alloc(256);
        real_t density[129];

        TEST_ASSERT_EQUAL(true, psd_design(&psd, overlaps[which], WINDOW_HANN,
                                           REAL_C(0.0)));
        TEST_ASSERT_EQUAL(true, psd_estimate(&psd, signal, 4096u, RATE, density));

        real_t power = psd_band_power(&psd, density, RATE,
                                      REAL_C(100.0), REAL_C(150.0));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.06) * expected, expected, power);

        psd_free(&psd);
    }
}

void test_the_whole_band_holds_the_power_of_the_signal(void)
{
    // Adding the density over every frequency gives the mean of the squares of
    // the signal. That is the rule of Parseval, and it holds the scaling
    // together from end to end.
    const real_t amplitude = REAL_C(2.5);
    psd_t psd = psd_alloc(256);
    real_t density[129];

    make_wave(amplitude, REAL_C(200.0), 4096u);

    psd_estimate(&psd, signal, 4096u, RATE, density);

    real_t whole = psd_band_power(&psd, density, RATE,
                                  REAL_C(0.0), RATE / REAL_C(2.0));
    real_t expected = (amplitude * amplitude) / REAL_C(2.0);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05) * expected, expected, whole);

    psd_free(&psd);
}

void test_the_peak_stands_at_the_frequency_of_the_wave(void)
{
    psd_t psd = psd_alloc(256);
    real_t density[129];

    make_wave(REAL_C(1.0), REAL_C(125.0), 4096u);
    psd_estimate(&psd, signal, 4096u, RATE, density);

    uint32_t best = 0;
    for(uint32_t bin = 1; bin < 129u; bin++)
    {
        if(density[bin] > density[best]) { best = bin; }
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(4.0), REAL_C(125.0),
                            psd_bin_frequency(&psd, best, RATE));

    psd_free(&psd);
}

void test_more_blocks_give_a_steadier_answer(void)
{
    // The trade that the method offers. The same noise, measured with few long
    // blocks and with many short ones. The many short ones must give a curve
    // that wanders less from bin to bin.
    uint32_t seed = 7u;
    for(uint32_t index = 0; index < 4096u; index++)
    {
        seed = (seed * 1103515245u) + 12345u;
        signal[index] = ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0))
                        - REAL_C(1.0);
    }

    real_t spread[2];
    uint32_t blocks[2] = {1024u, 64u};

    for(uint32_t which = 0; which < 2u; which++)
    {
        psd_t psd = psd_alloc(blocks[which]);
        uint32_t bins = psd_bin_count(&psd);
        real_t* density = (real_t*)malloc(sizeof(real_t) * bins);

        psd_estimate(&psd, signal, 4096u, RATE, density);

        // How far the answer moves from one bin to the next, as a part of its
        // own size. Noise that is truly flat should move little.
        real_t mean = REAL_C(0.0);
        for(uint32_t bin = 1; bin < (bins - 1u); bin++) { mean += density[bin]; }
        mean /= (real_t)(bins - 2u);

        real_t movement = REAL_C(0.0);
        for(uint32_t bin = 2; bin < (bins - 1u); bin++)
        {
            movement += REAL_ABS(density[bin] - density[bin - 1u]);
        }
        spread[which] = movement / (real_t)(bins - 3u) / mean;

        free(density);
        psd_free(&psd);
    }

    // Sixty-four sample blocks give many more of them, thus a steadier answer.
    TEST_ASSERT_TRUE(spread[1] < spread[0]);
}

void test_psd_estimate_refuses_a_signal_shorter_than_one_block(void)
{
    psd_t psd = psd_alloc(256);
    real_t density[129];

    make_wave(REAL_C(1.0), REAL_C(100.0), 128u);

    TEST_ASSERT_EQUAL(false, psd_estimate(&psd, signal, 128u, RATE, density));
    TEST_ASSERT_EQUAL(false, psd_estimate(&psd, signal, 1024u, REAL_C(0.0),
                                          density));

    psd_free(&psd);
}

void test_psd_band_power_of_a_band_that_is_not_a_band(void)
{
    psd_t psd = psd_alloc(64);
    real_t density[33];

    for(uint32_t bin = 0; bin < 33u; bin++) { density[bin] = REAL_C(1.0); }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            psd_band_power(&psd, density, RATE,
                                           REAL_C(200.0), REAL_C(100.0)));

    psd_free(&psd);
}
