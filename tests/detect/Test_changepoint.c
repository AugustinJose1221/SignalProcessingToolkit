#include "unity.h"
#include "real_assert.h"
#include "changepoint.h"
#include <math.h>

static uint32_t seed;

void setUp(void)
{
    seed = 1u;
}

void tearDown(void)
{

}

// An even spread between minus one and one has a standard deviation of one
// divided by the root of three, thus it is scaled here to a spread of one. The
// watcher works in units of the spread, and giving it the wrong spread is the
// same as giving it the wrong threshold.
static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;

    real_t even = ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0))
                  - REAL_C(1.0);

    return even * REAL_C(1.7320508);
}

void test_changepoint_refuses_what_it_cannot_watch(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(false, watcher.designed);

    // A reading that never wanders needs none of this.
    TEST_ASSERT_EQUAL(false, changepoint_design(&watcher, REAL_C(0.0),
                                                REAL_C(0.0), REAL_C(1.0),
                                                REAL_C(5.0)));
    TEST_ASSERT_EQUAL(false, changepoint_design(&watcher, REAL_C(0.0),
                                                -REAL_C(1.0), REAL_C(1.0),
                                                REAL_C(5.0)));
    // A change of nothing is found everywhere.
    TEST_ASSERT_EQUAL(false, changepoint_design(&watcher, REAL_C(0.0),
                                                REAL_C(1.0), REAL_C(0.0),
                                                REAL_C(5.0)));
    // A threshold of nothing is crossed by the first sample.
    TEST_ASSERT_EQUAL(false, changepoint_design(&watcher, REAL_C(0.0),
                                                REAL_C(1.0), REAL_C(1.0),
                                                REAL_C(0.0)));

    TEST_ASSERT_EQUAL(false, watcher.designed);

    // And a watcher that was never designed says nothing rather than answering
    // about a reading nobody described.
    TEST_ASSERT_EQUAL(CHANGEPOINT_NONE,
                      changepoint_process_sample(&watcher, REAL_C(1000.0)));
}

// A reading that has not changed must not raise an alarm, however long it runs.
// This is the half of the trade that is easy to lose.
void test_changepoint_says_nothing_about_a_reading_that_did_not_change(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(20.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    seed = 5u;

    uint32_t alarms = 0u;

    for(uint32_t index = 0; index < 20000u; index++)
    {
        if(changepoint_process_sample(&watcher, REAL_C(20.0) + rough())
           != CHANGEPOINT_NONE)
        {
            alarms++;
        }
    }

    // MEASURED AND NOT GUESSED. On two million samples of this same even
    // spread at this threshold, a wrong alarm came about once in every 372
    // samples, thus twenty thousand samples give about 54 of them. The bound
    // here is twice that, which leaves room for one run being one run and still
    // catches a watcher that has stopped holding its sums at nothing.
    TEST_ASSERT_TRUE(alarms < 120u);
}

// THE REASON THE MODULE EXISTS. The change is smaller than the noise, thus no
// threshold on one sample can find it, and this finds it.
void test_changepoint_finds_a_change_smaller_than_the_noise(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(20.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    seed = 11u;

    // Two hundred samples of an ordinary reading, and then the level steps up
    // by three quarters of the noise. No single sample says anything.
    for(uint32_t index = 0; index < 200u; index++)
    {
        TEST_ASSERT_EQUAL(CHANGEPOINT_NONE,
                          changepoint_process_sample(&watcher,
                                                     REAL_C(20.0) + rough()));
    }

    changepoint_way_t said = CHANGEPOINT_NONE;
    uint32_t took = 0u;

    for(uint32_t index = 0; index < 400u; index++)
    {
        took++;
        said = changepoint_process_sample(&watcher,
                                          REAL_C(20.75) + rough());

        if(said != CHANGEPOINT_NONE)
        {
            break;
        }
    }

    TEST_ASSERT_EQUAL(CHANGEPOINT_ROSE, said);

    // And it says when the change began, which is the number that says which
    // batch or which shift it was. It cannot be exact, because the noise
    // decides which sample the sum last touched nothing on.
    uint32_t ago = changepoint_began_ago(&watcher);

    TEST_ASSERT_TRUE(ago > 0u);
    TEST_ASSERT_TRUE(ago <= took);
    // The alarm arrives late by design, and the change is found to have begun
    // within a few dozen samples of where it really did.
    TEST_ASSERT_TRUE((took - ago) < 60u);
}

// A fall is counted apart from a rise, because the two usually mean different
// things about the thing being watched.
void test_changepoint_tells_a_fall_from_a_rise(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    seed = 17u;

    changepoint_way_t said = CHANGEPOINT_NONE;

    for(uint32_t index = 0; (index < 400u) && (said == CHANGEPOINT_NONE);
        index++)
    {
        said = changepoint_process_sample(&watcher, -REAL_C(1.0) + rough());
    }

    TEST_ASSERT_EQUAL(CHANGEPOINT_FELL, said);
}

// The units are what let one threshold serve every reading. A reading in
// millivolts and the same reading in volts must behave the same way.
void test_changepoint_works_in_units_of_the_spread(void)
{
    changepoint_t small = changepoint_make();
    changepoint_t large = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&small, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));
    TEST_ASSERT_EQUAL(true, changepoint_design(&large, REAL_C(1000.0),
                                               REAL_C(500.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    seed = 23u;

    for(uint32_t index = 0; index < 500u; index++)
    {
        real_t noise = rough();

        changepoint_way_t one = changepoint_process_sample(&small,
                                                           REAL_C(0.8)
                                                           + noise);
        changepoint_way_t other = changepoint_process_sample(
            &large, REAL_C(1000.0) + (REAL_C(500.0) * (REAL_C(0.8) + noise)));

        TEST_ASSERT_EQUAL(one, other);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                changepoint_running_high(&small),
                                changepoint_running_high(&large));
    }
}

// The sum is held at nothing from below. A long quiet spell must not build up
// credit that a later change spends, or the alarm would come sooner after a
// long quiet than after a short one and mean something different each time.
void test_changepoint_a_quiet_spell_builds_up_no_credit(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    // A long stretch of a reading exactly where it should be. Every sample
    // takes half the smallest change off both sums, which without the hold
    // would carry them far below nothing.
    for(uint32_t index = 0; index < 5000u; index++)
    {
        TEST_ASSERT_EQUAL(CHANGEPOINT_NONE,
                          changepoint_process_sample(&watcher, REAL_C(0.0)));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            changepoint_running_high(&watcher));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            changepoint_running_low(&watcher));

    // A clean step of two spreads. Each sample gives the sum two less the half
    // that is taken off, thus the threshold of five is reached on the fourth.
    uint32_t took = 0u;
    changepoint_way_t said = CHANGEPOINT_NONE;

    while(said == CHANGEPOINT_NONE)
    {
        took++;
        said = changepoint_process_sample(&watcher, REAL_C(2.0));
    }

    TEST_ASSERT_EQUAL(CHANGEPOINT_ROSE, said);
    TEST_ASSERT_EQUAL(4u, took);
    TEST_ASSERT_EQUAL(4u, changepoint_began_ago(&watcher));
}

// The answer is given once and the sums start again, so that a change still
// running gives the next alarm after the same delay rather than at every sample
// from then on.
void test_changepoint_reports_a_change_once_and_starts_again(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    uint32_t alarms = 0u;

    for(uint32_t index = 0; index < 40u; index++)
    {
        if(changepoint_process_sample(&watcher, REAL_C(2.0))
           != CHANGEPOINT_NONE)
        {
            alarms++;
        }
    }

    // Forty samples at four samples for each alarm.
    TEST_ASSERT_EQUAL(10u, alarms);
}

// The number to choose a threshold by.
void test_changepoint_delay_for_says_how_long_it_will_take(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    // A change of two gains 2 - 0.5 at every sample, thus 5 divided by 1.5.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(5.0) / REAL_C(1.5),
                            changepoint_delay_for(&watcher, REAL_C(2.0)));

    // And what it says is what happens.
    uint32_t took = 0u;

    while(changepoint_process_sample(&watcher, REAL_C(2.0))
          == CHANGEPOINT_NONE)
    {
        took++;
    }

    took++;

    TEST_ASSERT_EQUAL(4u, took);

    // A change smaller than half the smallest change looked for never arrives.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            changepoint_delay_for(&watcher, REAL_C(0.4)));
    // The direction does not matter to how long it takes.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                            changepoint_delay_for(&watcher, REAL_C(2.0)),
                            changepoint_delay_for(&watcher, -REAL_C(2.0)));
}

// A higher threshold takes longer and is wrong less often. That is the whole
// trade, and it must hold in the numbers.
void test_changepoint_a_higher_threshold_takes_longer(void)
{
    changepoint_t low = changepoint_make();
    changepoint_t high = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&low, REAL_C(0.0), REAL_C(1.0),
                                               REAL_C(1.0), REAL_C(4.0)));
    TEST_ASSERT_EQUAL(true, changepoint_design(&high, REAL_C(0.0), REAL_C(1.0),
                                               REAL_C(1.0), REAL_C(12.0)));

    TEST_ASSERT_TRUE(changepoint_delay_for(&high, REAL_C(1.0))
                     > changepoint_delay_for(&low, REAL_C(1.0)));

    seed = 31u;

    uint32_t low_alarms = 0u;
    uint32_t high_alarms = 0u;

    for(uint32_t index = 0; index < 20000u; index++)
    {
        real_t noise = rough();

        if(changepoint_process_sample(&low, noise) != CHANGEPOINT_NONE)
        {
            low_alarms++;
        }

        if(changepoint_process_sample(&high, noise) != CHANGEPOINT_NONE)
        {
            high_alarms++;
        }
    }

    TEST_ASSERT_TRUE(high_alarms < low_alarms);
}

void test_changepoint_reset_puts_the_sums_back(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(20.0)));

    for(uint32_t index = 0; index < 5u; index++)
    {
        changepoint_process_sample(&watcher, REAL_C(2.0));
    }

    TEST_ASSERT_TRUE(changepoint_running_high(&watcher) > REAL_C(1.0));

    changepoint_reset(&watcher);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            changepoint_running_high(&watcher));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            changepoint_running_low(&watcher));
    TEST_ASSERT_EQUAL(0u, changepoint_began_ago(&watcher));

    // And what it was told about the reading is kept.
    TEST_ASSERT_EQUAL(true, watcher.designed);
}

// A block reports the first change in it, and reads the whole block whether it
// found one or not.
void test_changepoint_a_block_reports_the_first_change_in_it(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    // Twenty samples where nothing happens and then a clean step of two, which
    // the watcher finds four samples later.
    real_t block[40];

    for(uint32_t index = 0; index < 40u; index++)
    {
        block[index] = (index < 20u) ? REAL_C(0.0) : REAL_C(2.0);
    }

    changepoint_way_t way = CHANGEPOINT_NONE;
    uint32_t at = 0u;

    TEST_ASSERT_EQUAL(true, changepoint_process_block(&watcher, block, 40u,
                                                      &way, &at));
    TEST_ASSERT_EQUAL(CHANGEPOINT_ROSE, way);
    TEST_ASSERT_EQUAL(23u, at);

    // And how long before the alarm the reading started walking away.
    TEST_ASSERT_EQUAL(4u, changepoint_began_ago(&watcher));
}

// A block holding nothing gives false and leaves both answers untouched.
void test_changepoint_a_block_with_no_change_says_so(void)
{
    changepoint_t watcher = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&watcher, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    real_t block[200];

    for(uint32_t index = 0; index < 200u; index++)
    {
        block[index] = REAL_C(0.0);
    }

    changepoint_way_t way = CHANGEPOINT_FELL;
    uint32_t at = 77u;

    TEST_ASSERT_EQUAL(false, changepoint_process_block(&watcher, block, 200u,
                                                       &way, &at));
    TEST_ASSERT_EQUAL(CHANGEPOINT_FELL, way);
    TEST_ASSERT_EQUAL(77u, at);

    // Either answer may be left out.
    TEST_ASSERT_EQUAL(false, changepoint_process_block(&watcher, block, 200u,
                                                       NULL, NULL));
}

// THE WHOLE BLOCK IS READ EVEN AFTER A CHANGE IS FOUND. Stopping at the first
// would leave the watcher part way through, and the next block would then be
// read as though the samples between had never happened.
void test_changepoint_a_block_is_the_samples_one_at_a_time(void)
{
    changepoint_t together = changepoint_make();
    changepoint_t apart = changepoint_make();

    TEST_ASSERT_EQUAL(true, changepoint_design(&together, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));
    TEST_ASSERT_EQUAL(true, changepoint_design(&apart, REAL_C(0.0),
                                               REAL_C(1.0), REAL_C(1.0),
                                               REAL_C(5.0)));

    // A block holding several changes, so that the two must agree past the
    // first of them.
    real_t block[120];

    for(uint32_t index = 0; index < 120u; index++)
    {
        block[index] = ((index / 20u) % 2u) ? REAL_C(2.0) : -REAL_C(2.0);
    }

    changepoint_process_block(&together, block, 120u, NULL, NULL);

    uint32_t alarms = 0u;

    for(uint32_t index = 0; index < 120u; index++)
    {
        if(changepoint_process_sample(&apart, block[index])
           != CHANGEPOINT_NONE)
        {
            alarms++;
        }
    }

    // Both watchers ended in the same place, which they could not have done
    // had the block stopped at its first alarm.
    TEST_ASSERT_TRUE(alarms > 1u);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                            changepoint_running_high(&apart),
                            changepoint_running_high(&together));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                            changepoint_running_low(&apart),
                            changepoint_running_low(&together));
}

void test_a_watcher_that_was_never_designed_promises_no_delay(void)
{
    // The delay follows from the threshold and the size of the change, and a
    // watcher that was never designed has neither. It must answer with nothing
    // rather than a number worked out from whatever the memory held.
    changepoint_t changepoint = changepoint_make();

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           changepoint_delay_for(&changepoint, REAL_C(2.0)));
}
