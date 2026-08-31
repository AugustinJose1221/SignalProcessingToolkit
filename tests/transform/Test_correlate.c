#include "unity.h"
#include "real_assert.h"
#include "correlate.h"
#include "fft.h"
#include "cnum.h"
#include "stats.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_correlate_is_valid_scaling(void)
{
    TEST_ASSERT_EQUAL(true, correlate_is_valid_scaling(CORRELATE_RAW));
    TEST_ASSERT_EQUAL(true, correlate_is_valid_scaling(CORRELATE_COEFFICIENT));
    TEST_ASSERT_EQUAL(false, correlate_is_valid_scaling(
                          (correlate_scaling_t)(CORRELATE_COEFFICIENT + 1)));
}

void test_a_signal_matches_itself_perfectly_at_no_lag(void)
{
    real_t data[8] = {REAL_C(1.0), REAL_C(-2.0), REAL_C(3.0), REAL_C(0.5),
                      REAL_C(-1.0), REAL_C(2.0), REAL_C(0.0), REAL_C(1.5)};
    real_t out[4];

    TEST_ASSERT_EQUAL(true, correlate_auto(data, 8u, out, 3u,
                                           CORRELATE_COEFFICIENT));

    // A coefficient of 1 at no lag, and never more than 1 anywhere.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), out[0]);
    for(uint32_t lag = 1; lag <= 3u; lag++)
    {
        TEST_ASSERT_TRUE(out[lag] <= REAL_C(1.0) + TOLERANCE);
        TEST_ASSERT_TRUE(out[lag] >= REAL_C(-1.0) - TOLERANCE);
    }
}

void test_the_raw_sum_is_the_sum_of_the_products(void)
{
    real_t data[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};
    real_t out[3];

    correlate_auto(data, 4u, out, 2u, CORRELATE_RAW);

    // At no lag: 1 + 4 + 9 + 16 = 30.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(30.0), out[0]);
    // At a lag of one: 1*2 + 2*3 + 3*4 = 20.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(20.0), out[1]);
    // At a lag of two: 1*3 + 2*4 = 11.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(11.0), out[2]);
}

void test_the_two_ways_of_dividing_differ_where_the_overlap_is_short(void)
{
    real_t data[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};
    real_t biased[3];
    real_t unbiased[3];

    correlate_auto(data, 4u, biased, 2u, CORRELATE_BIASED);
    correlate_auto(data, 4u, unbiased, 2u, CORRELATE_UNBIASED);

    // At no lag every sample overlaps, thus the two agree.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, biased[0], unbiased[0]);

    // At a lag of two only two samples overlap. Dividing by the whole size
    // makes the answer look weaker than it is.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(11.0) / REAL_C(4.0), biased[2]);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(11.0) / REAL_C(2.0), unbiased[2]);
}

void test_the_coefficient_takes_the_mean_off_and_the_raw_sum_does_not(void)
{
    // This is the trap the header names. A signal that never goes below zero
    // matches itself at every lag, because the product of two positive numbers
    // is positive whatever the lag.
    real_t data[32];
    real_t raw[16];
    real_t coefficient[16];

    for(uint32_t index = 0; index < 32u; index++)
    {
        // A wave that repeats every 8 samples, carried on a large level.
        data[index] = REAL_C(1000.0)
                      + REAL_SIN(REAL_C(2.0) * PI * (real_t)index / REAL_C(8.0));
    }

    correlate_auto(data, 32u, raw, 15u, CORRELATE_RAW);
    correlate_auto(data, 32u, coefficient, 15u, CORRELATE_COEFFICIENT);

    // The raw sum is large and positive at every lag, thus it says nothing
    // about where the signal repeats.
    for(uint32_t lag = 0; lag <= 15u; lag++)
    {
        TEST_ASSERT_TRUE(raw[lag] > REAL_C(0.0));
    }

    // The coefficient finds the period: near 1 at a lag of 8, and near -1 at
    // a lag of 4, where the wave stands upside down against itself.
    TEST_ASSERT_TRUE(coefficient[8] > REAL_C(0.9));
    TEST_ASSERT_TRUE(coefficient[4] < REAL_C(-0.9));
}

void test_correlate_best_lag_finds_a_period(void)
{
    // The whole of finding a period in one call.
    real_t data[256];
    real_t work[100];

    for(uint32_t index = 0; index < 256u; index++)
    {
        data[index] = REAL_SIN(REAL_C(2.0) * PI * (real_t)index / REAL_C(37.0));
    }

    real_t strength = REAL_C(0.0);
    uint32_t period = correlate_best_lag(data, 256u, work, 10u, 99u, &strength);

    TEST_ASSERT_EQUAL(37, period);
    // A signal that truly repeats gives a strength near one.
    TEST_ASSERT_TRUE(strength > REAL_C(0.9));
}

void test_correlate_best_lag_says_when_nothing_repeats(void)
{
    // A signal that holds no period must give a small strength, so that a
    // caller can tell the difference. This is what parts a scanner from a
    // quiet room in a recording of a heart.
    real_t data[256];
    real_t work[100];
    uint32_t seed = 1u;

    for(uint32_t index = 0; index < 256u; index++)
    {
        seed = (seed * 1103515245u) + 12345u;
        data[index] = (real_t)((seed >> 16) % 1000u) / REAL_C(1000.0);
    }

    real_t strength = REAL_C(0.0);
    correlate_best_lag(data, 256u, work, 10u, 99u, &strength);

    TEST_ASSERT_TRUE(strength < REAL_C(0.5));
}

void test_correlate_best_lag_refuses_a_range_that_says_nothing(void)
{
    real_t data[16];
    real_t work[16];
    real_t strength = REAL_C(1.0);

    for(uint32_t index = 0; index < 16u; index++) { data[index] = REAL_C(1.0); }

    // A lag of nothing is not allowed: every signal matches itself there.
    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, work, 0u, 8u, &strength));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), strength);

    // A lag as long as the signal leaves nothing to correlate.
    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, work, 4u, 16u, &strength));
    // A range the wrong way round.
    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, work, 8u, 4u, &strength));
}

void test_correlate_cross_finds_the_delay_between_two_signals(void)
{
    // The other question the module answers: two recordings of the same thing,
    // one later than the other.
    real_t first[128];
    real_t second[128];
    real_t out[40];
    const uint32_t delay = 13u;

    for(uint32_t index = 0; index < 128u; index++)
    {
        real_t value = REAL_SIN(REAL_C(0.3) * (real_t)index)
                       + REAL_SIN(REAL_C(0.07) * (real_t)index);
        first[index] = value;
        second[index] = (index >= delay)
                        ? (REAL_SIN(REAL_C(0.3) * (real_t)(index - delay))
                           + REAL_SIN(REAL_C(0.07) * (real_t)(index - delay)))
                        : REAL_C(0.0);
    }

    // The lag moves the SECOND argument later in time, thus the delayed
    // recording goes second and the peak stands at the delay. Putting them the
    // other way round asks for a lag of minus 13, which this range does not
    // hold.
    correlate_cross(first, second, 128u, out, 39u, CORRELATE_COEFFICIENT);

    uint32_t best = 0;
    for(uint32_t lag = 1; lag <= 39u; lag++)
    {
        if(out[lag] > out[best]) { best = lag; }
    }

    TEST_ASSERT_EQUAL(delay, best);
}

void test_correlate_refuses_what_it_cannot_do(void)
{
    real_t data[8];
    real_t out[8];

    for(uint32_t index = 0; index < 8u; index++) { data[index] = REAL_C(1.0); }

    // A lag as long as the signal leaves no overlap.
    TEST_ASSERT_EQUAL(false, correlate_auto(data, 8u, out, 8u, CORRELATE_RAW));
    TEST_ASSERT_EQUAL(false, correlate_auto(data, 0u, out, 0u, CORRELATE_RAW));
    TEST_ASSERT_EQUAL(false, correlate_auto(data, 8u, out, 4u,
                          (correlate_scaling_t)(CORRELATE_COEFFICIENT + 1)));
}

void test_correlate_transform_size(void)
{
    // At least twice the size, and a power of two.
    TEST_ASSERT_EQUAL(256, correlate_transform_size(128));
    TEST_ASSERT_EQUAL(512, correlate_transform_size(129));
    TEST_ASSERT_EQUAL(512, correlate_transform_size(256));
    TEST_ASSERT_EQUAL(0, correlate_transform_size(0));
}

void test_the_transform_gives_the_same_answer_as_the_plain_way(void)
{
    // The reason the fast way may be trusted at all. Both must agree at every
    // lag and for every scaling.
    const uint32_t size = 128u;
    real_t data[128];
    real_t plain[64];
    real_t fast[64];

    for(uint32_t index = 0; index < size; index++)
    {
        data[index] = REAL_SIN(REAL_C(0.21) * (real_t)index)
                      + (REAL_C(0.4) * REAL_COS(REAL_C(0.05) * (real_t)index))
                      + REAL_C(3.0);
    }

    uint32_t transform = correlate_transform_size(size);
    cnum_t* work = (cnum_t*)malloc(sizeof(cnum_t) * transform);
    real_t* window = (real_t*)malloc(sizeof(real_t) * transform);
    fft_t fft = fft_alloc(transform);

    // The three scalings that are sums. The coefficient is not among them,
    // because a transform cannot give one, and the module says so.
    correlate_scaling_t scaling[3] = {CORRELATE_RAW, CORRELATE_BIASED,
                                      CORRELATE_UNBIASED};

    for(uint32_t which = 0; which < 3u; which++)
    {
        TEST_ASSERT_EQUAL(true, correlate_auto(data, size, plain, 63u,
                                               scaling[which]));
        TEST_ASSERT_EQUAL(true, correlate_auto_by_transform(data, size, fast,
                              63u, scaling[which], &fft, work, window));

        for(uint32_t lag = 0; lag <= 63u; lag++)
        {
            // The two take different roads to the same number, thus they part
            // in the last digits only. The tolerance follows the size of the
            // answer rather than being one number for every scaling.
            real_t size_of = REAL_ABS(plain[lag]);
            if(size_of < REAL_C(1.0)) { size_of = REAL_C(1.0); }
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.002) * size_of,
                                    plain[lag], fast[lag]);
        }
    }

    // The coefficient is refused, and the header says why.
    TEST_ASSERT_EQUAL(false, correlate_auto_by_transform(data, size, fast, 63u,
                          CORRELATE_COEFFICIENT, &fft, work, window));

    fft_free(&fft);
    free(work);
    free(window);
}

void test_the_transform_refuses_a_transform_of_the_wrong_size(void)
{
    // A caller that made the transform for another size would get an answer
    // that looks right and is not, thus the module examines it.
    real_t data[64];
    real_t out[32];
    cnum_t work[256];
    real_t window[256];

    for(uint32_t index = 0; index < 64u; index++) { data[index] = REAL_C(1.0); }

    fft_t wrong = fft_alloc(64);
    TEST_ASSERT_EQUAL(false, correlate_auto_by_transform(data, 64u, out, 31u,
                          CORRELATE_RAW, &wrong, work, window));
    fft_free(&wrong);

    fft_t right = fft_alloc(correlate_transform_size(64));
    TEST_ASSERT_EQUAL(true, correlate_auto_by_transform(data, 64u, out, 31u,
                          CORRELATE_RAW, &right, work, window));
    fft_free(&right);
}

void test_a_range_of_lags_that_does_not_fit_inside_the_signal_gives_nothing(void)
{
    // The lag of 0 must be left out: every signal matches itself perfectly
    // there and that answer says nothing. A range that reaches as far as the
    // signal is long has no samples left to overlap.
    real_t data[16];
    real_t room[32];
    real_t strength = REAL_C(9.0);

    for(uint32_t index = 0; index < 16u; index++)
    {
        data[index] = (real_t)sin(0.5 * (double)index);
    }

    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, room, 0u, 8u,
                                            &strength));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), strength);

    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, room, 6u, 4u,
                                            &strength));
    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, room, 2u, 16u,
                                            &strength));
    TEST_ASSERT_EQUAL(0, correlate_best_lag(data, 16u, room, 2u, 20u,
                                            &strength));
}

void test_a_lag_that_leaves_fewer_than_two_samples_overlapping_says_nothing(void)
{
    // A correlation coefficient is worked out from how the two lists move
    // about their own means. One pair of samples has no movement to speak of,
    // thus there is no coefficient and the answer must be 0.
    real_t data[8];
    real_t room[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        data[index] = (real_t)index;
    }

    TEST_ASSERT_TRUE(correlate_auto(data, 8u, room, 7u,
                                    CORRELATE_COEFFICIENT));

    // The last lag leaves one pair of samples overlapping.
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), room[7]);
}

void test_a_correlation_that_reaches_past_the_signal_is_refused(void)
{
    real_t data[8] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0),
                      REAL_C(5.0), REAL_C(6.0), REAL_C(7.0), REAL_C(8.0)};
    real_t room[16];
    uint32_t transform = correlate_transform_size(8u);
    fft_t fft = fft_alloc(transform);
    cnum_t* work = (cnum_t*)malloc(sizeof(cnum_t) * transform);
    real_t* window = (real_t*)malloc(sizeof(real_t) * transform);

    TEST_ASSERT_FALSE(correlate_auto_by_transform(data, 0u, room, 0u,
                                                  CORRELATE_RAW, &fft, work,
                                                  window));
    TEST_ASSERT_FALSE(correlate_auto_by_transform(data, 8u, room, 8u,
                                                  CORRELATE_RAW, &fft, work,
                                                  window));
    TEST_ASSERT_FALSE(correlate_auto_by_transform(data, 8u, room, 4u,
                                                  CORRELATE_COEFFICIENT, &fft,
                                                  work, window));

    free(work);
    free(window);
    fft_free(&fft);
}
