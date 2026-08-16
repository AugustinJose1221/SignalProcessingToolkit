#include "unity.h"
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
#define PI              3.14159265358979323846f

static fft_t fft;
static cnum_t work[SIZE];
static float amplitude[NUMBER_OF_IMF * SIZE];
static float frequency[NUMBER_OF_IMF * (SIZE - 1)];

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
    float x[SIZE];
    float y[SIZE];
    imf_t imf = imf_static_alloc(SIZE, x, y);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        x[index] = (float)index;
        y[index] = 3.0f * cosf((2.0f*PI*12.0f*(float)index)/(float)SIZE);
    }

    hht_transform_imf(&fft, &imf, work, amplitude, frequency, (float)SIZE);

    for(uint32_t index = 8; index < (SIZE - 8); index++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f, amplitude[index]);
        TEST_ASSERT_FLOAT_WITHIN(0.2f, 12.0f, frequency[index]);
    }
}

void test_hht_mean_frequency(void)
{
    // Every amplitude is the same, thus the mean is the plain mean of the
    // frequencies.
    float small_amplitude[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float small_frequency[3] = {10.0f, 20.0f, 30.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f,
                             hht_mean_frequency(small_amplitude, small_frequency, 4));
}

void test_hht_mean_frequency_gives_little_weight_to_a_small_amplitude(void)
{
    // The point with the frequency 100 holds almost no amplitude, thus it must
    // move the mean very little.
    float small_amplitude[4] = {1.0f, 1.0f, 0.01f, 1.0f};
    float small_frequency[3] = {10.0f, 10.0f, 100.0f};

    float mean = hht_mean_frequency(small_amplitude, small_frequency, 4);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, mean);
}

void test_hht_mean_frequency_of_a_signal_with_no_amplitude_is_zero(void)
{
    float small_amplitude[3] = {0.0f, 0.0f, 0.0f};
    float small_frequency[2] = {10.0f, 20.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                             hht_mean_frequency(small_amplitude, small_frequency, 3));
}

void test_hht_transform_of_several_functions_writes_each_one_after_the_other(void)
{
    float x[NUMBER_OF_IMF][SIZE];
    float y[NUMBER_OF_IMF][SIZE];
    imf_t imf[NUMBER_OF_IMF];
    float cycles[NUMBER_OF_IMF] = {20.0f, 10.0f, 4.0f};

    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf[which] = imf_static_alloc(SIZE, x[which], y[which]);
        for(uint32_t index = 0; index < SIZE; index++)
        {
            x[which][index] = (float)index;
            y[which][index] = cosf((2.0f*PI*cycles[which]*(float)index)/(float)SIZE);
        }
    }

    hht_transform(&fft, imf, NUMBER_OF_IMF, work, amplitude, frequency, (float)SIZE);

    // Each part of the result must hold the frequency of its own function.
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        float mean = hht_mean_frequency(&amplitude[which*SIZE],
                                        &frequency[which*(SIZE-1)], SIZE);
        TEST_ASSERT_FLOAT_WITHIN(0.5f, cycles[which], mean);
    }
}

void test_hht_the_decomposition_and_the_transform_work_together(void)
{
    // This is the whole Hilbert-Huang transform. The signal holds a fast part
    // and a slow part. The decomposition must take them apart, and the
    // transform must then give a higher frequency for the first function than
    // for the second one.
    static float x[SIZE];
    static float y[SIZE];
    static float residue[SIZE];
    static float working_buffer[SIZE];
    static float peak_index_buffer[SIZE];
    static float valley_index_buffer[SIZE];
    imf_t imf[NUMBER_OF_IMF];

    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf[which] = imf_alloc(SIZE);
    }

    for(uint32_t index = 0; index < SIZE; index++)
    {
        x[index] = (float)index;
        y[index] = cosf((2.0f*PI*24.0f*(float)index)/(float)SIZE)
                   + cosf((2.0f*PI*4.0f*(float)index)/(float)SIZE);
    }

    emd_t emd = emd_alloc(SIZE);
    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);
    uint32_t count = emd_sift(&emd, 5);

    TEST_ASSERT_GREATER_THAN(1, count);

    hht_transform(&fft, imf, count, work, amplitude, frequency, (float)SIZE);

    float first = hht_mean_frequency(&amplitude[0], &frequency[0], SIZE);
    float second = hht_mean_frequency(&amplitude[SIZE], &frequency[SIZE-1], SIZE);

    // The decomposition gives the fast part first.
    TEST_ASSERT_TRUE(first > second);
    TEST_ASSERT_TRUE(first > 0.0f);

    emd_free(emd);
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf_free(imf[which]);
    }
}
