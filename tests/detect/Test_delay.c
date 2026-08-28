#include "unity.h"
#include "real_assert.h"
#include "delay.h"
#include "fft.h"
#include "cnum.h"
#include "correlate.h"
#include "peakdetect.h"
#include <math.h>

#define SIZE            1024u
#define LARGEST_LAG     64u
#define TWO_PI          REAL_C(6.28318530717958647692)

static real_t first[SIZE];
static real_t second[SIZE];
static real_t work[DELAY_WORK_COUNT(LARGEST_LAG)];
static cnum_t first_work[SIZE];
static cnum_t second_work[SIZE];
static uint32_t seed;

void setUp(void)
{
    seed = 1u;
}

void tearDown(void)
{

}

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

// Build a pair where the second stands a known distance behind the first. The
// distance need not be a whole number of samples, which is the point: the
// signal is worked out from where it stands in time, thus a delay of a third of
// a sample is a real delay and not a rounded one.
//
// A band of tones rather than one tone, so that the delay is not the same as a
// delay of one turn more or less.
//
// AND THE TONES MUST NOT SHARE A SHORT PERIOD. Tones at whole multiples of one
// frequency add up to a signal that repeats at that frequency, thus the two
// readings agree just as well at the delay wanted and at that delay a whole
// period either way. Measured with tones at multiples of a 64 sample period and
// a wanted delay of 7, the search found -57 and was right to.
//
// AND EACH TONE SITS EXACTLY ON A BIN OF THE TRANSFORM. A tone between two bins
// spreads across the whole spectrum, and the spread carries a turn of half a
// turn from one bin to the next that has nothing to do with the delay. The way
// that reads the delay from the phase then reads that instead: measured with
// tones off the bins, a delay of 7 came back as 3.1.
//
// AND THE TONES FILL A WHOLE BAND WITH NO GAPS IN IT. The way that reads the
// phase works from how far the phase turns from ONE BIN TO THE NEXT. Where a
// bin and the bin beside it are not both loud, that step says nothing, and a
// signal of a few tones far apart leaves every step saying nothing: measured
// with nine tones spread across the band, a delay of 7 came back as 1.6. A band
// filled bin by bin is what a real reading looks like, and it is what this way
// is for.
#define LOWEST_BIN      40u
#define HIGHEST_BIN     120u

static void build_pair(real_t behind, real_t noise)
{
    seed = 3u;

    // A phase of its own for each bin, so that the block is a spread of noise
    // across the band rather than one loud click.
    real_t phase[HIGHEST_BIN + 1u];

    for(uint32_t bin = LOWEST_BIN; bin <= HIGHEST_BIN; bin++)
    {
        phase[bin] = TWO_PI * rough();
    }

    seed = 3u;

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t at = (real_t)index;
        real_t back = at - behind;

        first[index] = REAL_C(0.0);
        second[index] = REAL_C(0.0);

        for(uint32_t bin = LOWEST_BIN; bin <= HIGHEST_BIN; bin++)
        {
            real_t turn = TWO_PI * (real_t)bin / (real_t)SIZE;

            first[index] += REAL_SIN((turn * at) + phase[bin]);
            second[index] += REAL_SIN((turn * back) + phase[bin]);
        }

        first[index] += noise * rough();
        second[index] += noise * rough();
    }
}

void test_delay_is_valid_way(void)
{
    TEST_ASSERT_EQUAL(true, delay_is_valid_way(DELAY_CORRELATE));
    TEST_ASSERT_EQUAL(true, delay_is_valid_way(DELAY_PHASE));
    TEST_ASSERT_EQUAL(false, delay_is_valid_way((delay_way_t)7));
}

// Three points fix one curve, and the top of that curve is the answer.
void test_delay_refine_peak_finds_the_top_of_a_curve(void)
{
    // A curve whose top stands a quarter of a step to the right of the middle
    // point: values of a curve of the second order read at -1, 0 and 1 with its
    // top at 0.25.
    real_t values[3];

    for(uint32_t index = 0; index < 3u; index++)
    {
        real_t at = (real_t)index - REAL_C(1.0) - REAL_C(0.25);

        values[index] = REAL_C(4.0) - (at * at);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.25),
                            delay_refine_peak(values, 3u, 1u));

    // And exactly on the middle point where the three are even.
    real_t even[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            delay_refine_peak(even, 3u, 1u));
}

// There are not three points at either end, and three points that do not bend
// downwards hold no top between them.
void test_delay_refine_peak_gives_nothing_where_there_is_no_peak(void)
{
    real_t values[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            delay_refine_peak(values, 4u, 0u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            delay_refine_peak(values, 4u, 3u));
    // Rising through the middle, thus no top there.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            delay_refine_peak(values, 4u, 2u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            delay_refine_peak(values, 2u, 1u));
}

// A whole number of samples, which both ways must get exactly right.
void test_delay_finds_a_whole_number_of_samples_either_way_round(void)
{
    fft_t fft = fft_alloc(SIZE);
    real_t behinds[3] = {REAL_C(7.0), REAL_C(0.0), -REAL_C(11.0)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        build_pair(behinds[which], REAL_C(0.0));

        real_t found = REAL_C(0.0);
        real_t strength = REAL_C(0.0);

        TEST_ASSERT_EQUAL(true, delay_by_correlation(first, second, SIZE,
                                                     LARGEST_LAG, work,
                                                     &found, &strength));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), behinds[which], found);
        TEST_ASSERT_TRUE(strength > REAL_C(0.9));

        TEST_ASSERT_EQUAL(true, delay_by_phase(first, second, SIZE, &fft,
                                               first_work, second_work,
                                               &found));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), behinds[which], found);
    }

    fft_free(&fft);
}

// THE REASON THE MODULE EXISTS. A delay that is not a whole number of samples
// is a real delay, and rounding it to a sample throws away what was measured.
void test_delay_finds_a_part_of_a_sample(void)
{
    fft_t fft = fft_alloc(SIZE);
    real_t behinds[4] = {REAL_C(3.25), REAL_C(3.5), REAL_C(3.75),
                         -REAL_C(2.4)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        build_pair(behinds[which], REAL_C(0.0));

        real_t found = REAL_C(0.0);

        TEST_ASSERT_EQUAL(true, delay_by_phase(first, second, SIZE, &fft,
                                               first_work, second_work,
                                               &found));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), behinds[which], found);

        TEST_ASSERT_EQUAL(true, delay_by_correlation(first, second, SIZE,
                                                     LARGEST_LAG, work,
                                                     &found, NULL));

        // The curve fitted through three points is not the shape of the peak,
        // thus this way leans towards the nearer neighbour. It is still far
        // closer than the nearest whole sample.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), behinds[which], found);
    }

    fft_free(&fft);
}

// The way from the phase is the finer of the two, which is the reason to pay
// for a transform.
void test_delay_the_phase_is_finer_than_the_correlation(void)
{
    fft_t fft = fft_alloc(SIZE);

    build_pair(REAL_C(5.4), REAL_C(0.0));

    real_t by_phase = REAL_C(0.0);
    real_t by_correlation = REAL_C(0.0);

    TEST_ASSERT_EQUAL(true, delay_by_phase(first, second, SIZE, &fft,
                                           first_work, second_work,
                                           &by_phase));
    TEST_ASSERT_EQUAL(true, delay_by_correlation(first, second, SIZE,
                                                 LARGEST_LAG, work,
                                                 &by_correlation, NULL));

    TEST_ASSERT_TRUE(REAL_ABS(by_phase - REAL_C(5.4))
                     < REAL_ABS(by_correlation - REAL_C(5.4)));

    fft_free(&fft);
}

// A pair with a delay in it and noise on top of it. The delay is still there.
void test_delay_holds_up_under_noise(void)
{
    fft_t fft = fft_alloc(SIZE);

    build_pair(REAL_C(6.5), REAL_C(1.0));

    real_t found = REAL_C(0.0);
    real_t strength = REAL_C(0.0);

    TEST_ASSERT_EQUAL(true, delay_by_correlation(first, second, SIZE,
                                                 LARGEST_LAG, work, &found,
                                                 &strength));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.3), REAL_C(6.5), found);
    TEST_ASSERT_TRUE(strength > REAL_C(0.5));

    TEST_ASSERT_EQUAL(true, delay_by_phase(first, second, SIZE, &fft,
                                           first_work, second_work, &found));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.3), REAL_C(6.5), found);

    fft_free(&fft);
}

// THE STRENGTH MUST BE READ. Two readings with nothing in common still have a
// place where they agree best, and the delay to that place says nothing. The
// strength is the only thing that says so.
void test_delay_says_how_much_the_two_readings_agree(void)
{
    build_pair(REAL_C(6.0), REAL_C(0.0));

    // Throw the second reading away and put noise in its place.
    seed = 99u;

    for(uint32_t index = 0; index < SIZE; index++)
    {
        second[index] = rough();
    }

    real_t found = REAL_C(0.0);
    real_t strength = REAL_C(0.0);

    TEST_ASSERT_EQUAL(true, delay_by_correlation(first, second, SIZE,
                                                 LARGEST_LAG, work, &found,
                                                 &strength));

    // There is an answer, and the strength says not to believe it.
    TEST_ASSERT_TRUE(strength < REAL_C(0.3));
}

void test_delay_refuses_what_it_cannot_measure(void)
{
    fft_t fft = fft_alloc(SIZE);

    build_pair(REAL_C(4.0), REAL_C(0.0));

    real_t found = REAL_C(77.0);

    // A largest lag as long as the reading leaves no overlap at all.
    TEST_ASSERT_EQUAL(false, delay_by_correlation(first, second, SIZE, SIZE,
                                                  work, &found, NULL));
    // And a lag of nothing has no range to search.
    TEST_ASSERT_EQUAL(false, delay_by_correlation(first, second, SIZE, 0u,
                                                  work, &found, NULL));
    // A reading shorter than the transform.
    TEST_ASSERT_EQUAL(false, delay_by_phase(first, second, SIZE / 2u, &fft,
                                            first_work, second_work, &found));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(77.0), found);

    fft_free(&fft);
}

// Nothing in either reading. There is no phase to have a slope, and the answer
// is nothing rather than whatever the rounding of an empty sum gives.
void test_delay_by_phase_of_two_empty_readings_is_nothing(void)
{
    fft_t fft = fft_alloc(SIZE);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        first[index] = REAL_C(0.0);
        second[index] = REAL_C(0.0);
    }

    real_t found = REAL_C(77.0);

    TEST_ASSERT_EQUAL(true, delay_by_phase(first, second, SIZE, &fft,
                                           first_work, second_work, &found));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), found);

    fft_free(&fft);
}
