#include "unity.h"
#include "real_assert.h"
#include "hht.h"
#include "hilbert.h"
#include "fft.h"
#include "cnum.h"
#include "imf.h"
#include "emd.h"
#include "cspline.h"
#include "peakdetect.h"
#include "valleydetect.h"
#include "binarysearch.h"
#include <stdlib.h>
#include <math.h>

#define SIZE            128u
#define NUMBER_OF_IMF   3u
#define PI              REAL_C(3.14159265358979323846)

static fft_t fft;
static cnum_t work[SIZE];
static real_t amplitude[NUMBER_OF_IMF * SIZE];
static real_t frequency[NUMBER_OF_IMF * (SIZE - 1)];

void setUp(void)
{
    fft = fft_alloc(SIZE);
}

void tearDown(void)
{
    fft_free(&fft);
}

void test_hht_transform_imf_of_a_single_cosine(void)
{
    // One intrinsic mode function that holds a plain cosine. The transform
    // must give an amplitude that does not change and the frequency of that
    // cosine.
    real_t x[SIZE];
    real_t y[SIZE];
    imf_t imf = imf_static_alloc(SIZE, x, y);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        x[index] = (real_t)index;
        y[index] = REAL_C(3.0) * REAL_COS((REAL_C(2.0)*PI*REAL_C(12.0)*(real_t)index)/(real_t)SIZE);
    }

    hht_transform_imf(&fft, &imf, work, amplitude, frequency, (real_t)SIZE);

    for(uint32_t index = 8; index < (SIZE - 8); index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(3.0), amplitude[index]);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(12.0), frequency[index]);
    }
}

void test_hht_mean_frequency(void)
{
    // Every amplitude is the same, thus the mean is the plain mean of the
    // frequencies.
    real_t small_amplitude[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};
    real_t small_frequency[3] = {REAL_C(10.0), REAL_C(20.0), REAL_C(30.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(20.0),
                             hht_mean_frequency(small_amplitude, small_frequency, 4));
}

void test_hht_mean_frequency_gives_little_weight_to_a_small_amplitude(void)
{
    // The point with the frequency 100 holds almost no amplitude, thus it must
    // move the mean very little.
    real_t small_amplitude[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(0.01), REAL_C(1.0)};
    real_t small_frequency[3] = {REAL_C(10.0), REAL_C(10.0), REAL_C(100.0)};

    real_t mean = hht_mean_frequency(small_amplitude, small_frequency, 4);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(10.0), mean);
}

void test_hht_mean_frequency_of_a_signal_with_no_amplitude_is_zero(void)
{
    real_t small_amplitude[3] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    real_t small_frequency[2] = {REAL_C(10.0), REAL_C(20.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                             hht_mean_frequency(small_amplitude, small_frequency, 3));
}

void test_hht_transform_of_several_functions_writes_each_one_after_the_other(void)
{
    real_t x[NUMBER_OF_IMF][SIZE];
    real_t y[NUMBER_OF_IMF][SIZE];
    imf_t imf[NUMBER_OF_IMF];
    real_t cycles[NUMBER_OF_IMF] = {REAL_C(20.0), REAL_C(10.0), REAL_C(4.0)};

    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf[which] = imf_static_alloc(SIZE, x[which], y[which]);
        for(uint32_t index = 0; index < SIZE; index++)
        {
            x[which][index] = (real_t)index;
            y[which][index] = REAL_COS((REAL_C(2.0)*PI*cycles[which]*(real_t)index)/(real_t)SIZE);
        }
    }

    hht_transform(&fft, imf, NUMBER_OF_IMF, work, amplitude, frequency, (real_t)SIZE);

    // Each part of the result must hold the frequency of its own function.
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        real_t mean = hht_mean_frequency(&amplitude[which*SIZE],
                                        &frequency[which*(SIZE-1)], SIZE);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), cycles[which], mean);
    }
}

void test_hht_the_decomposition_and_the_transform_work_together(void)
{
    // This is the whole Hilbert-Huang transform. The signal holds a fast part
    // and a slow part. The decomposition must take them apart, and the
    // transform must then give a higher frequency for the first function than
    // for the second one.
    static real_t x[SIZE];
    static real_t y[SIZE];
    static real_t residue[SIZE];
    static real_t working_buffer[SIZE];
    static real_t peak_index_buffer[SIZE];
    static real_t valley_index_buffer[SIZE];
    imf_t imf[NUMBER_OF_IMF];

    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf[which] = imf_alloc(SIZE);
    }

    for(uint32_t index = 0; index < SIZE; index++)
    {
        x[index] = (real_t)index;
        y[index] = REAL_COS((REAL_C(2.0)*PI*REAL_C(24.0)*(real_t)index)/(real_t)SIZE)
                   + REAL_COS((REAL_C(2.0)*PI*REAL_C(4.0)*(real_t)index)/(real_t)SIZE);
    }

    emd_t emd = emd_alloc(SIZE);
    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);
    uint32_t count = emd_sift(&emd, 5);

    TEST_ASSERT_GREATER_THAN(1, count);

    hht_transform(&fft, imf, count, work, amplitude, frequency, (real_t)SIZE);

    real_t first = hht_mean_frequency(&amplitude[0], &frequency[0], SIZE);
    real_t second = hht_mean_frequency(&amplitude[SIZE], &frequency[SIZE-1], SIZE);

    // The decomposition gives the fast part first.
    TEST_ASSERT_TRUE(first > second);
    TEST_ASSERT_TRUE(first > REAL_C(0.0));

    emd_free(emd);
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf_free(imf[which]);
    }
}
