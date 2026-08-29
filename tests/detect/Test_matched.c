#include "unity.h"
#include "real_assert.h"
#include "matched.h"
#include "stats.h"
#include <math.h>

#define PATTERN_LENGTH  16u
#define SIGNAL_LENGTH   1000u
#define BURIED_AT       317u

static real_t pattern[PATTERN_LENGTH];
static real_t signal[SIGNAL_LENGTH];
static real_t score[SIGNAL_LENGTH];
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

// A chirp: a shape that looks like nothing else and like no shift of itself,
// which is what a matched filter is at its best on.
static void build_pattern(void)
{
    for(uint32_t index = 0; index < PATTERN_LENGTH; index++)
    {
        real_t part = (real_t)index / (real_t)PATTERN_LENGTH;

        pattern[index] = REAL_SIN(REAL_C(6.28318530718) * part * part
                                  * REAL_C(4.0));
    }
}

static void build_signal(real_t loudness, real_t noise)
{
    seed = 7u;

    for(uint32_t index = 0; index < SIGNAL_LENGTH; index++)
    {
        signal[index] = noise * rough();
    }

    for(uint32_t index = 0; index < PATTERN_LENGTH; index++)
    {
        signal[BURIED_AT + index] += loudness * pattern[index];
    }
}

void test_matched_is_valid_length(void)
{
    TEST_ASSERT_EQUAL(false, matched_is_valid_length(0u));
    TEST_ASSERT_EQUAL(true, matched_is_valid_length(1u));
    TEST_ASSERT_EQUAL(true, matched_is_valid_length(MATCHED_LARGEST_LENGTH));
    TEST_ASSERT_EQUAL(false,
                      matched_is_valid_length(MATCHED_LARGEST_LENGTH + 1u));
}

void test_matched_a_filter_that_holds_no_shape_says_nothing(void)
{
    matched_t matched = matched_make();

    TEST_ASSERT_EQUAL(false, matched.designed);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            matched_score_at(&matched, signal));

    uint32_t where = 99u;
    real_t best = REAL_C(99.0);

    TEST_ASSERT_EQUAL(false, matched_best(&matched, signal, SIGNAL_LENGTH,
                                          &where, &best));
    TEST_ASSERT_EQUAL(99u, where);
}

// A shape with no energy would be found everywhere with equal strength, thus it
// is refused rather than answered.
void test_matched_a_shape_of_nothing_is_refused(void)
{
    matched_t matched = matched_make();
    real_t empty[4] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};

    TEST_ASSERT_EQUAL(false, matched_design(&matched, empty, 4u));
    TEST_ASSERT_EQUAL(false, matched.designed);

    build_pattern();
    TEST_ASSERT_EQUAL(false, matched_design(&matched, pattern, 0u));
}

// THE REASON THE MODULE EXISTS. The shape is buried under noise louder than
// itself, and the filter still says where it is.
void test_matched_finds_a_shape_buried_under_noise(void)
{
    matched_t matched = matched_make();

    build_pattern();
    TEST_ASSERT_EQUAL(true, matched_design(&matched, pattern,
                                           PATTERN_LENGTH));

    // The shape reaches half of what the noise does, thus at no single sample
    // can it be seen at all.
    build_signal(REAL_C(0.5), REAL_C(1.0));

    uint32_t where = 0u;
    real_t best = REAL_C(0.0);

    TEST_ASSERT_EQUAL(true, matched_best(&matched, signal, SIGNAL_LENGTH,
                                         &where, &best));

    TEST_ASSERT_EQUAL(BURIED_AT, where);
}

// The score is in units of the noise: a reading of pure noise of a given
// standard deviation gives a score of about that standard deviation, whatever
// the shape is and however loud it is. That is what lets one threshold serve
// every shape.
void test_matched_the_score_is_in_units_of_the_noise(void)
{
    matched_t matched = matched_make();

    build_pattern();

    real_t louder[PATTERN_LENGTH];

    for(uint32_t index = 0; index < PATTERN_LENGTH; index++)
    {
        louder[index] = REAL_C(100.0) * pattern[index];
    }

    real_t noises[2] = {REAL_C(1.0), REAL_C(4.0)};

    for(uint32_t which = 0; which < 2u; which++)
    {
        build_signal(REAL_C(0.0), noises[which]);

        for(uint32_t turn = 0; turn < 2u; turn++)
        {
            TEST_ASSERT_EQUAL(true,
                              matched_design(&matched,
                                             (turn == 0u) ? pattern : louder,
                                             PATTERN_LENGTH));

            TEST_ASSERT_EQUAL(true, matched_score_block(&matched, signal,
                                                        SIGNAL_LENGTH, score));

            uint32_t offsets = MATCHED_SCORE_COUNT(SIGNAL_LENGTH,
                                                   PATTERN_LENGTH);

            // The reading is noise alone, thus the spread of the score must be
            // the spread of the reading. MEASURED AND NOT ASSUMED: rough gives
            // an even spread between minus one and one, whose standard
            // deviation is one divided by the root of three and not one.
            real_t spread = stats_deviation(signal, SIGNAL_LENGTH);

            // A tenth of room, because the reading is one run and not the
            // average of many.
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.1) * spread, spread,
                                    stats_deviation(score, offsets));
        }
    }
}

void test_matched_scoring_a_block_agrees_with_scoring_one_offset(void)
{
    matched_t matched = matched_make();

    build_pattern();
    TEST_ASSERT_EQUAL(true, matched_design(&matched, pattern,
                                           PATTERN_LENGTH));
    build_signal(REAL_C(1.0), REAL_C(1.0));

    TEST_ASSERT_EQUAL(true, matched_score_block(&matched, signal,
                                                SIGNAL_LENGTH, score));

    uint32_t offsets = MATCHED_SCORE_COUNT(SIGNAL_LENGTH, PATTERN_LENGTH);

    for(uint32_t offset = 0; offset < offsets; offset++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                                matched_score_at(&matched, &signal[offset]),
                                score[offset]);
    }
}

// A reading shorter than the shape has no offset where the shape lies whole
// inside it, thus there is nothing to score.
void test_matched_a_reading_shorter_than_the_shape_is_refused(void)
{
    matched_t matched = matched_make();

    build_pattern();
    TEST_ASSERT_EQUAL(true, matched_design(&matched, pattern,
                                           PATTERN_LENGTH));

    uint32_t where = 5u;
    real_t best = REAL_C(5.0);

    TEST_ASSERT_EQUAL(false, matched_score_block(&matched, signal,
                                                 PATTERN_LENGTH - 1u, score));
    TEST_ASSERT_EQUAL(false, matched_best(&matched, signal,
                                          PATTERN_LENGTH - 1u, &where, &best));

    // A reading of exactly the length of the shape holds one offset.
    TEST_ASSERT_EQUAL(true, matched_score_block(&matched, signal,
                                                PATTERN_LENGTH, score));
    TEST_ASSERT_EQUAL(1u, MATCHED_SCORE_COUNT(PATTERN_LENGTH,
                                              PATTERN_LENGTH));
}

// The threshold is worked out from the normal distribution. These are the
// numbers a table gives.
void test_matched_threshold_for_agrees_with_the_table(void)
{
    real_t rates[4] = {REAL_C(0.1), REAL_C(0.05), REAL_C(0.01),
                       REAL_C(0.001)};
    real_t table[4] = {REAL_C(1.281552), REAL_C(1.644854), REAL_C(2.326348),
                       REAL_C(3.090232)};

    for(uint32_t index = 0; index < 4u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), table[index],
                                matched_threshold_for(rates[index], 1u));
    }

    // Far out in the tail, which is where a real search stands.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.753424),
                            matched_threshold_for(REAL_C(0.000001), 1u));
}

// THE MISTAKE THE OFFSET COUNT EXISTS TO STOP. A rate that is right for one
// offset cries wolf once per reading when a thousand offsets are looked at.
void test_matched_the_threshold_rises_with_the_number_of_offsets(void)
{
    real_t one = matched_threshold_for(REAL_C(0.01), 1u);
    real_t many = matched_threshold_for(REAL_C(0.01), 10000u);

    TEST_ASSERT_TRUE(many > one);

    // Sharing the rate among the offsets is the same as asking for that much
    // smaller a rate at a single offset.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                            matched_threshold_for(REAL_C(0.000001), 1u), many);
}

void test_matched_threshold_for_refuses_a_rate_it_cannot_answer(void)
{
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            matched_threshold_for(REAL_C(0.0), 10u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            matched_threshold_for(REAL_C(1.0), 10u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            matched_threshold_for(-REAL_C(0.1), 10u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            matched_threshold_for(REAL_C(0.01), 0u));
}

// The threshold and the score are made for each other: a reading of pure noise
// must cross the threshold about as rarely as the rate says.
void test_matched_a_threshold_holds_the_false_alarms_it_promised(void)
{
    matched_t matched = matched_make();

    build_pattern();
    TEST_ASSERT_EQUAL(true, matched_design(&matched, pattern,
                                           PATTERN_LENGTH));

    uint32_t offsets = MATCHED_SCORE_COUNT(SIGNAL_LENGTH, PATTERN_LENGTH);
    real_t threshold = matched_threshold_for(REAL_C(0.05), offsets);

    uint32_t alarms = 0u;

    for(uint32_t run = 0; run < 40u; run++)
    {
        seed = 1000u + run;

        for(uint32_t index = 0; index < SIGNAL_LENGTH; index++)
        {
            signal[index] = rough();
        }

        // rough gives an even spread and not a normal one, thus the reading is
        // scaled to a standard deviation of one before the threshold is read
        // against it. The score adds sixteen of them together, which is what
        // makes the score itself close to normal whatever one sample is.
        real_t spread = stats_deviation(signal, SIGNAL_LENGTH);

        TEST_ASSERT_EQUAL(true, matched_score_block(&matched, signal,
                                                    SIGNAL_LENGTH, score));

        for(uint32_t offset = 0; offset < offsets; offset++)
        {
            if((score[offset] / spread) > threshold)
            {
                alarms++;
                break;
            }
        }
    }

    // 40 readings at a rate of one in twenty is two expected. Anything under a
    // fifth of the readings says the threshold is doing its work; a threshold
    // that was far too low would fire on nearly all of them.
    TEST_ASSERT_TRUE(alarms <= 8u);
}

void test_a_rate_of_false_alarms_that_is_almost_certain_gives_a_threshold_below_zero(void)
{
    // A caller asking to be wrong 99 times in 100 is asking for a threshold
    // that fires on almost anything, and such a threshold stands BELOW the
    // level of the noise. The fit has its own branch for that end, and the
    // branch is here for completeness rather than for use.
    //
    // The answer must still be a real number and must still be ordered: asking
    // to be wrong more often can only lower the threshold.
    real_t nearly_always = matched_threshold_for(REAL_C(0.99), 1u);
    real_t often = matched_threshold_for(REAL_C(0.98), 1u);
    real_t seldom = matched_threshold_for(REAL_C(0.001), 1u);

    TEST_ASSERT_TRUE(nearly_always < REAL_C(0.0));
    TEST_ASSERT_TRUE(often < REAL_C(0.0));
    TEST_ASSERT_TRUE(nearly_always < often);
    TEST_ASSERT_TRUE(often < seldom);

    // The value is the point that 99 in 100 of a normal spread stand above,
    // which is 2.326 below the middle.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(-2.326), nearly_always);
}

void test_a_rate_of_false_alarms_that_means_nothing_is_refused(void)
{
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           matched_threshold_for(REAL_C(0.0), 1u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           matched_threshold_for(REAL_C(1.0), 1u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           matched_threshold_for(REAL_C(-0.5), 1u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           matched_threshold_for(REAL_C(0.01), 0u));
}
