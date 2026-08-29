#include "unity.h"
#include "real_assert.h"
#include "convolve.h"
#include "correlate.h"
#include "fft.h"
#include "cnum.h"
#include "fir.h"
#include "window.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_convolve_is_valid_mode(void)
{
    TEST_ASSERT_EQUAL(true, convolve_is_valid_mode(CONVOLVE_FULL));
    TEST_ASSERT_EQUAL(true, convolve_is_valid_mode(CONVOLVE_VALID));
    TEST_ASSERT_EQUAL(false, convolve_is_valid_mode(
                          (convolve_mode_t)(CONVOLVE_VALID + 1)));
}

void test_convolve_output_size(void)
{
    // Sliding a shape of 3 along a signal of 10 touches 12 places.
    TEST_ASSERT_EQUAL(12, convolve_output_size(10, 3, CONVOLVE_FULL));
    TEST_ASSERT_EQUAL(10, convolve_output_size(10, 3, CONVOLVE_SAME));
    // Only 8 of those have the shape wholly inside the signal.
    TEST_ASSERT_EQUAL(8, convolve_output_size(10, 3, CONVOLVE_VALID));

    // A shape longer than the signal never lies wholly inside it.
    TEST_ASSERT_EQUAL(0, convolve_output_size(3, 10, CONVOLVE_VALID));
    TEST_ASSERT_EQUAL(12, convolve_output_size(3, 10, CONVOLVE_FULL));

    TEST_ASSERT_EQUAL(0, convolve_output_size(0, 3, CONVOLVE_FULL));
    TEST_ASSERT_EQUAL(0, convolve_output_size(10, 0, CONVOLVE_FULL));
}

void test_convolve_by_hand(void)
{
    // Worked out on paper: {1,2,3} against {1,1} gives {1,3,5,3}.
    real_t signal[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t shape[2] = {REAL_C(1.0), REAL_C(1.0)};
    real_t output[4];

    TEST_ASSERT_EQUAL(true, convolve_direct(signal, 3u, shape, 2u, output,
                                            CONVOLVE_FULL));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), output[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), output[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), output[3]);
}

void test_a_shape_of_one_leaves_the_signal_alone(void)
{
    // Convolving with a single 1 must give the signal back exactly. Anything
    // else means the sliding or the lining up is wrong.
    real_t signal[5] = {REAL_C(3.0), REAL_C(-1.0), REAL_C(4.0), REAL_C(1.0),
                        REAL_C(-5.0)};
    real_t shape[1] = {REAL_C(1.0)};
    real_t output[5];

    convolve_direct(signal, 5u, shape, 1u, output, CONVOLVE_SAME);

    for(uint32_t index = 0; index < 5u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, signal[index], output[index]);
    }
}

void test_a_convolution_turns_the_shape_round_and_a_correlation_does_not(void)
{
    // THE USUAL CONFUSION, held here as a fact. The two are the same sum with
    // one difference, and for a shape that is not the same forwards and
    // backwards the answers differ.
    real_t signal[5] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                        REAL_C(0.0)};
    real_t shape[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t convolved[7];

    convolve_direct(signal, 5u, shape, 3u, convolved, CONVOLVE_FULL);

    // A single 1 at the start gives the shape back as it stands.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), convolved[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), convolved[1]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), convolved[2]);

    // The same shape, turned round, gives a different answer. That difference
    // IS the difference between the two operations.
    real_t turned[3] = {REAL_C(3.0), REAL_C(2.0), REAL_C(1.0)};
    real_t other[7];

    convolve_direct(signal, 5u, turned, 3u, other, CONVOLVE_FULL);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), other[0]);
    TEST_ASSERT_TRUE(REAL_ABS(other[0] - convolved[0]) > REAL_C(1.0));
}

void test_a_shape_that_is_the_same_both_ways_gives_the_same_as_a_correlation(void)
{
    // The other half of the same story: where the shape IS the same forwards
    // and backwards, the two operations agree, which is why so much code is
    // written on that assumption and works until it meets a shape that is not.
    real_t signal[8] = {REAL_C(1.0), REAL_C(4.0), REAL_C(2.0), REAL_C(-3.0),
                        REAL_C(5.0), REAL_C(0.0), REAL_C(1.0), REAL_C(2.0)};
    real_t shape[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(1.0)};
    real_t convolved[8];
    real_t correlated[6];

    convolve_direct(signal, 8u, shape, 3u, convolved, CONVOLVE_VALID);
    // A correlation of the signal against the shape at each lag. Only the
    // places where the two fully overlap are compared.
    for(uint32_t lag = 0; lag < 6u; lag++)
    {
        real_t total = REAL_C(0.0);
        for(uint32_t k = 0; k < 3u; k++)
        {
            total += signal[lag + k] * shape[k];
        }
        correlated[lag] = total;
    }

    for(uint32_t index = 0; index < 6u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, correlated[index], convolved[index]);
    }
}

void test_convolving_with_a_filter_is_the_same_as_running_the_filter(void)
{
    // Passing a signal through a filter IS a convolution with its
    // coefficients. If these two disagreed, one of the two modules would be
    // wrong about what a filter does.
    const uint32_t length = 21u;
    fir_t fir = fir_alloc(length);
    fir_design_low_pass(&fir, REAL_C(0.15));

    real_t shape[21];
    for(uint32_t index = 0; index < length; index++)
    {
        shape[index] = fir_get_coefficient(&fir, index);
    }

    real_t signal[200];
    real_t from_filter[200];
    // The full answer is longer than the signal by one less than the shape.
    real_t from_convolution[220];

    for(uint32_t index = 0; index < 200u; index++)
    {
        signal[index] = REAL_SIN(REAL_C(0.2) * (real_t)index)
                        + (REAL_C(0.4) * REAL_COS(REAL_C(0.03) * (real_t)index));
        from_filter[index] = fir_process_sample(&fir, signal[index]);
    }

    convolve_direct(signal, 200u, shape, length, from_convolution,
                    CONVOLVE_FULL);

    // The filter answers from the first sample, taking the samples before the
    // signal to be nothing, which is what CONVOLVE_FULL assumes as well.
    for(uint32_t index = 0; index < 200u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), from_filter[index],
                                from_convolution[index]);
    }

    fir_free(&fir);
}

void test_the_valid_mode_assumes_nothing_at_the_ends(void)
{
    // A signal of all ones convolved with a shape of three ones. Where the
    // shape lies wholly inside, every answer is 3. The full mode reports 1 and
    // 2 at the ends, and those came from a signal that was taken to be zero
    // outside itself rather than from anything measured.
    real_t signal[10];
    real_t full[12];
    real_t valid[8];

    for(uint32_t index = 0; index < 10u; index++) { signal[index] = REAL_C(1.0); }

    real_t shape[3] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};

    convolve_direct(signal, 10u, shape, 3u, full, CONVOLVE_FULL);
    convolve_direct(signal, 10u, shape, 3u, valid, CONVOLVE_VALID);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), full[0]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), full[1]);

    for(uint32_t index = 0; index < 8u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), valid[index]);
    }
}

void test_the_same_mode_lines_up_with_the_signal(void)
{
    // A single spike in the middle, smoothed by a shape of three. The answer
    // must still peak where the spike stood.
    real_t signal[11];
    real_t output[11];
    real_t shape[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(1.0)};

    for(uint32_t index = 0; index < 11u; index++) { signal[index] = REAL_C(0.0); }
    signal[5] = REAL_C(1.0);

    convolve_direct(signal, 11u, shape, 3u, output, CONVOLVE_SAME);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), output[5]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[4]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), output[6]);
}

void test_convolve_transform_size(void)
{
    // At least as long as the whole answer, and a power of two.
    TEST_ASSERT_EQUAL(128, convolve_transform_size(100, 20));
    // 200 and 60 give an answer of 259 places, thus 256 is not enough.
    TEST_ASSERT_EQUAL(512, convolve_transform_size(200, 60));
    TEST_ASSERT_EQUAL(0, convolve_transform_size(0, 4));
}

void test_the_transform_gives_the_same_answer_as_the_plain_way(void)
{
    // The reason the fast way may be trusted at all.
    const uint32_t signal_size = 200u;
    const uint32_t shape_size = 31u;
    real_t signal[200];
    real_t shape[31];

    for(uint32_t index = 0; index < signal_size; index++)
    {
        signal[index] = REAL_SIN(REAL_C(0.17) * (real_t)index)
                        + (REAL_C(0.3) * (real_t)(index % 7u));
    }
    for(uint32_t index = 0; index < shape_size; index++)
    {
        // A shape that is NOT the same forwards and backwards, so that the
        // turning round is exercised.
        shape[index] = REAL_EXP(-REAL_C(0.1) * (real_t)index);
    }

    uint32_t transform = convolve_transform_size(signal_size, shape_size);
    cnum_t* first = (cnum_t*)malloc(sizeof(cnum_t) * transform);
    cnum_t* second = (cnum_t*)malloc(sizeof(cnum_t) * transform);
    real_t* work = (real_t*)malloc(sizeof(real_t) * transform);
    fft_t fft = fft_alloc(transform);

    convolve_mode_t modes[3] = {CONVOLVE_FULL, CONVOLVE_SAME, CONVOLVE_VALID};

    for(uint32_t which = 0; which < 3u; which++)
    {
        uint32_t count = convolve_output_size(signal_size, shape_size,
                                              modes[which]);
        real_t* plain = (real_t*)malloc(sizeof(real_t) * count);
        real_t* fast = (real_t*)malloc(sizeof(real_t) * count);

        TEST_ASSERT_EQUAL(true, convolve_direct(signal, signal_size, shape,
                                                shape_size, plain,
                                                modes[which]));
        TEST_ASSERT_EQUAL(true, convolve_by_transform(signal, signal_size,
                              shape, shape_size, fast, modes[which], &fft,
                              first, second, work));

        for(uint32_t index = 0; index < count; index++)
        {
            real_t size_of = REAL_ABS(plain[index]);
            if(size_of < REAL_C(1.0)) { size_of = REAL_C(1.0); }
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.002) * size_of, plain[index],
                                    fast[index]);
        }

        free(plain);
        free(fast);
    }

    fft_free(&fft);
    free(first);
    free(second);
    free(work);
}

void test_the_transform_refuses_one_of_the_wrong_size(void)
{
    real_t signal[64];
    real_t shape[8];
    real_t output[71];
    cnum_t first[128];
    cnum_t second[128];
    real_t work[128];

    for(uint32_t index = 0; index < 64u; index++) { signal[index] = REAL_C(1.0); }
    for(uint32_t index = 0; index < 8u; index++) { shape[index] = REAL_C(1.0); }

    fft_t wrong = fft_alloc(64);
    TEST_ASSERT_EQUAL(false, convolve_by_transform(signal, 64u, shape, 8u,
                          output, CONVOLVE_FULL, &wrong, first, second, work));
    fft_free(&wrong);

    fft_t right = fft_alloc(convolve_transform_size(64, 8));
    TEST_ASSERT_EQUAL(true, convolve_by_transform(signal, 64u, shape, 8u,
                          output, CONVOLVE_FULL, &right, first, second, work));
    fft_free(&right);
}

void test_convolve_refuses_what_it_cannot_do(void)
{
    real_t signal[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};
    real_t shape[8] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0),
                       REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};
    real_t output[16];

    // A shape longer than the signal never lies wholly inside it.
    TEST_ASSERT_EQUAL(false, convolve_direct(signal, 4u, shape, 8u, output,
                                             CONVOLVE_VALID));
    TEST_ASSERT_EQUAL(false, convolve_direct(signal, 0u, shape, 8u, output,
                                             CONVOLVE_FULL));
    TEST_ASSERT_EQUAL(false, convolve_direct(signal, 4u, shape, 8u, output,
                          (convolve_mode_t)(CONVOLVE_VALID + 1)));
}

void test_a_shape_longer_than_the_signal_has_no_valid_answer(void)
{
    // CONVOLVE_VALID keeps only the places where the shape lies WHOLLY inside
    // the signal. A shape longer than the signal has no such place, thus there
    // is no answer to give and the module must refuse rather than write a
    // list of nothing.
    real_t signal[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t shape[5] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0),
                       REAL_C(1.0)};
    real_t output[8];

    TEST_ASSERT_EQUAL(0, convolve_output_size(3u, 5u, CONVOLVE_VALID));
    TEST_ASSERT_FALSE(convolve_direct(signal, 3u, shape, 5u, output,
                                      CONVOLVE_VALID));
}
