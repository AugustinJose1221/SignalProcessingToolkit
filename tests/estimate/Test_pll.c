#include "unity.h"
#include "real_assert.h"
#include "pll.h"
#include <math.h>

#define PI      REAL_C(3.14159265358979323846)
#define RATE    REAL_C(1000.0)
#define TONE    REAL_C(100.0)

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

// Run a tone of the given frequency through a loop and give what it settled on.
static real_t follow(pll_t* loop, real_t frequency, real_t noise,
                     uint32_t count)
{
    real_t phase = REAL_C(0.0);
    real_t total = REAL_C(0.0);
    uint32_t used = 0u;

    for(uint32_t index = 0; index < count; index++)
    {
        phase += frequency / RATE;

        real_t in = REAL_SIN(REAL_C(2.0) * PI * phase) + (noise * rough());

        pll_process_sample(loop, in);

        if(index >= (count / 2u))
        {
            total += pll_get_frequency(loop, RATE);
            used++;
        }
    }

    return total / (real_t)used;
}

void test_pll_is_valid_bandwidth_and_damping(void)
{
    TEST_ASSERT_EQUAL(true, pll_is_valid_bandwidth(REAL_C(0.01)));
    TEST_ASSERT_EQUAL(false, pll_is_valid_bandwidth(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, pll_is_valid_bandwidth(-REAL_C(0.01)));
    // Past this the loop follows its own detector rather than the signal.
    TEST_ASSERT_EQUAL(false, pll_is_valid_bandwidth(REAL_C(0.3)));

    TEST_ASSERT_EQUAL(true, pll_is_valid_damping(REAL_C(0.707)));
    TEST_ASSERT_EQUAL(false, pll_is_valid_damping(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, pll_is_valid_damping(REAL_C(10.0)));
}

void test_pll_design_refuses_what_it_cannot_follow(void)
{
    pll_t loop = pll_make();

    // At or above half the rate a tone cannot be told from a lower one.
    TEST_ASSERT_EQUAL(false, pll_design(&loop, RATE, RATE, REAL_C(0.01),
                                        REAL_C(0.707)));
    TEST_ASSERT_EQUAL(false, pll_design(&loop, REAL_C(0.0), RATE,
                                        REAL_C(0.01), REAL_C(0.707)));
    TEST_ASSERT_EQUAL(false, pll_design(&loop, TONE, REAL_C(0.0),
                                        REAL_C(0.01), REAL_C(0.707)));
    TEST_ASSERT_EQUAL(false, pll_design(&loop, TONE, RATE, REAL_C(0.5),
                                        REAL_C(0.707)));

    TEST_ASSERT_EQUAL(false, loop.designed);

    // A loop that was never designed says nothing rather than answering about
    // a tone nobody described.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            pll_process_sample(&loop, REAL_C(1.0)));

    TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.005),
                                       REAL_C(0.707)));
    TEST_ASSERT_EQUAL(true, loop.designed);
}

// THE REASON THE MODULE EXISTS. The loop is told to look near one frequency and
// given another, and it must walk to where the tone really is.
void test_pll_follows_a_tone_away_from_where_it_was_told_to_look(void)
{
    real_t offsets[5] = {-REAL_C(4.0), -REAL_C(2.0), REAL_C(0.0),
                         REAL_C(2.0), REAL_C(4.0)};

    for(uint32_t which = 0; which < 5u; which++)
    {
        pll_t loop = pll_make();

        TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.01),
                                           REAL_C(0.707)));

        real_t wanted = TONE + offsets[which];
        real_t found = follow(&loop, wanted, REAL_C(0.0), 40000u);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), wanted, found);

        // And it says it has found something.
        TEST_ASSERT_TRUE(pll_lock_quality(&loop) > REAL_C(0.8));
    }
}

// THE NUMBER THAT MUST BE READ. A loop given noise and no tone settles
// somewhere and reports a frequency exactly as confidently as it reports a real
// one. The lock quality is the only thing that tells the two apart.
void test_pll_says_when_it_has_found_nothing(void)
{
    pll_t with_tone = pll_make();
    pll_t without = pll_make();

    TEST_ASSERT_EQUAL(true, pll_design(&with_tone, TONE, RATE, REAL_C(0.005),
                                       REAL_C(0.707)));
    TEST_ASSERT_EQUAL(true, pll_design(&without, TONE, RATE, REAL_C(0.005),
                                       REAL_C(0.707)));

    seed = 5u;
    follow(&with_tone, TONE, REAL_C(0.3), 40000u);

    seed = 5u;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        pll_process_sample(&without, rough());
    }

    TEST_ASSERT_TRUE(pll_lock_quality(&with_tone) > REAL_C(0.8));
    TEST_ASSERT_TRUE(pll_lock_quality(&without) < REAL_C(0.4));

    // It still reports a frequency, which is the trap.
    TEST_ASSERT_TRUE(pll_get_frequency(&without, RATE) > REAL_C(0.0));
}

// THE LOOP DIVIDES BY HOW LOUD THE SIGNAL IS. Without that the gain would be
// the gain the caller asked for multiplied by the loudness of whatever
// arrived, thus a quiet tone would never arrive and a loud one would be
// unstable, and the bandwidth would mean nothing.
void test_pll_follows_a_quiet_tone_as_well_as_a_loud_one(void)
{
    real_t heights[4] = {REAL_C(0.01), REAL_C(0.2), REAL_C(1.0),
                         REAL_C(50.0)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        pll_t loop = pll_make();

        TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.01),
                                           REAL_C(0.707)));

        real_t wanted = TONE + REAL_C(3.0);
        real_t phase = REAL_C(0.0);
        real_t total = REAL_C(0.0);
        uint32_t used = 0u;

        for(uint32_t index = 0; index < 40000u; index++)
        {
            phase += wanted / RATE;

            pll_process_sample(&loop, heights[which]
                               * REAL_SIN(REAL_C(2.0) * PI * phase));

            if(index >= 20000u)
            {
                total += pll_get_frequency(&loop, RATE);
                used++;
            }
        }

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.3), wanted,
                                total / (real_t)used);
        TEST_ASSERT_TRUE(pll_lock_quality(&loop) > REAL_C(0.8));
    }
}

// A WIDER LOOP LETS MORE NOISE INTO THE ANSWER. That is the whole of the trade
// and it must hold in the numbers.
void test_pll_a_wider_loop_wanders_more(void)
{
    real_t bandwidths[3] = {REAL_C(0.001), REAL_C(0.005), REAL_C(0.02)};
    real_t wander[3];

    for(uint32_t which = 0; which < 3u; which++)
    {
        pll_t loop = pll_make();

        TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE,
                                           bandwidths[which],
                                           REAL_C(0.707)));

        seed = 11u;

        real_t phase = REAL_C(0.0);
        real_t total = REAL_C(0.0);
        real_t squared = REAL_C(0.0);
        uint32_t used = 0u;

        for(uint32_t index = 0; index < 60000u; index++)
        {
            phase += TONE / RATE;

            pll_process_sample(&loop, REAL_SIN(REAL_C(2.0) * PI * phase)
                               + rough());

            if(index >= 30000u)
            {
                real_t found = pll_get_frequency(&loop, RATE);

                total += found;
                squared += found * found;
                used++;
            }
        }

        real_t mean = total / (real_t)used;

        wander[which] = REAL_SQRT((squared / (real_t)used) - (mean * mean));
    }

    TEST_ASSERT_TRUE(wander[1] > wander[0]);
    TEST_ASSERT_TRUE(wander[2] > wander[1]);
}

// The tone it gives back holds the frequency and the phase of what arrived and
// none of its noise, which is what recovering a carrier means.
void test_pll_gives_back_a_clean_tone(void)
{
    pll_t loop = pll_make();

    TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.002),
                                       REAL_C(0.707)));

    seed = 3u;

    real_t phase = REAL_C(0.0);
    real_t together = REAL_C(0.0);
    real_t mine = REAL_C(0.0);
    real_t theirs = REAL_C(0.0);

    for(uint32_t index = 0; index < 60000u; index++)
    {
        phase += TONE / RATE;

        real_t clean = REAL_SIN(REAL_C(2.0) * PI * phase);
        real_t got = pll_process_sample(&loop, clean + rough());

        if(index >= 30000u)
        {
            together += got * clean;
            mine += got * got;
            theirs += clean * clean;
        }
    }

    // How well the tone it gives back lines up with the tone that was really
    // there, under noise as loud as the tone itself.
    real_t alike = together / REAL_SQRT(mine * theirs);

    TEST_ASSERT_TRUE(alike > REAL_C(0.95));
}

void test_pll_reset_puts_it_back_where_it_was_told_to_look(void)
{
    pll_t loop = pll_make();

    TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.01),
                                       REAL_C(0.707)));

    follow(&loop, TONE + REAL_C(4.0), REAL_C(0.0), 20000u);

    pll_reset(&loop);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), TONE,
                            pll_get_frequency(&loop, RATE));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            pll_get_phase(&loop));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            pll_lock_quality(&loop));

    // And what it was designed with is kept.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.01), loop.bandwidth);
}

void test_pll_says_how_far_it_reaches_and_how_long_it_takes(void)
{
    pll_t loop = pll_make();

    TEST_ASSERT_EQUAL(true, pll_design(&loop, TONE, RATE, REAL_C(0.005),
                                       REAL_C(0.707)));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.005),
                            pll_pull_range(&loop));
    TEST_ASSERT_EQUAL(400u, pll_settle_samples(&loop));

    // The phase never leaves its turn, however long it runs.
    for(uint32_t index = 0; index < 5000u; index++)
    {
        pll_process_sample(&loop, REAL_SIN((real_t)index));

        TEST_ASSERT_TRUE(pll_get_phase(&loop) >= REAL_C(0.0));
        TEST_ASSERT_TRUE(pll_get_phase(&loop) < REAL_C(1.0));
    }
}
