#ifndef TEST
#include <sptk/detect/changepoint.h>
#include <sptk/core/defs.h>
#else
#include "changepoint.h"
#include "defs.h"
#endif

#include <math.h>

// Put both sums back to nothing after an alarm, WITHOUT touching how long ago
// the change began. That number is the answer the caller is about to read, thus
// changepoint_reset cannot be used here: it clears that too.
static void changepoint_start_again(changepoint_t* changepoint);

bool changepoint_is_valid_deviation(real_t deviation)
{
    return deviation > REAL_SMALLEST;
}

bool changepoint_is_valid_change(real_t smallest_change)
{
    return smallest_change > REAL_C(0.0);
}

bool changepoint_is_valid_threshold(real_t threshold)
{
    return threshold > REAL_C(0.0);
}

changepoint_t changepoint_make(void)
{
    changepoint_t changepoint;

    changepoint.expected = REAL_C(0.0);
    changepoint.deviation = REAL_C(1.0);
    changepoint.smallest_change = CHANGEPOINT_DEFAULT_CHANGE;
    changepoint.threshold = CHANGEPOINT_DEFAULT_THRESHOLD;
    changepoint.designed = false;

    changepoint_reset(&changepoint);

    return changepoint;
}

bool changepoint_design(changepoint_t* changepoint, real_t expected,
                        real_t deviation, real_t smallest_change,
                        real_t threshold)
{
    ASSERT(changepoint != NULL);

    if(!changepoint_is_valid_deviation(deviation)
       || !changepoint_is_valid_change(smallest_change)
       || !changepoint_is_valid_threshold(threshold))
    {
        return false;
    }

    changepoint->expected = expected;
    changepoint->deviation = deviation;
    changepoint->smallest_change = smallest_change;
    changepoint->threshold = threshold;
    changepoint->designed = true;

    changepoint_reset(changepoint);

    return true;
}

void changepoint_reset(changepoint_t* changepoint)
{
    ASSERT(changepoint != NULL);

    changepoint->high = REAL_C(0.0);
    changepoint->low = REAL_C(0.0);
    changepoint->since_high = 0u;
    changepoint->since_low = 0u;
    changepoint->counted = 0u;
}

changepoint_way_t changepoint_process_sample(changepoint_t* changepoint,
                                             real_t sample)
{
    ASSERT(changepoint != NULL);

    if(!changepoint->designed)
    {
        return CHANGEPOINT_NONE;
    }

    // How far this sample stands from where it should be, measured in how far
    // the reading wanders. Working in these units is what lets one threshold
    // serve a reading of millivolts and a reading of degrees.
    real_t standing = (sample - changepoint->expected) / changepoint->deviation;

    // HALF THE SMALLEST CHANGE IS TAKEN OFF EACH STEP, and that half is the
    // whole of why the sums do not run away on their own. Without it every
    // sample above the expected value would add to the upward sum and the
    // noise alone would carry it to the threshold in time.
    //
    // Taking off half the change worth finding puts the noise on the losing
    // side: it drifts down towards nothing and is held there. A real change of
    // the size looked for gives half of itself to the sum at every sample, thus
    // it climbs steadily. A change of less than half is not found at all, which
    // is what the smallest change MEANS.
    real_t slack = changepoint->smallest_change / REAL_C(2.0);

    real_t high = changepoint->high + standing - slack;
    real_t low = changepoint->low - standing - slack;

    // Held at nothing from below. A long quiet spell must not build up credit
    // for a change that comes later, thus a sum that falls below nothing starts
    // again from there, and where it started again is when the reading last
    // looked ordinary.
    if(high <= REAL_C(0.0))
    {
        changepoint->high = REAL_C(0.0);
        changepoint->since_high = 0u;
    }
    else
    {
        changepoint->high = high;
        changepoint->since_high++;
    }

    if(low <= REAL_C(0.0))
    {
        changepoint->low = REAL_C(0.0);
        changepoint->since_low = 0u;
    }
    else
    {
        changepoint->low = low;
        changepoint->since_low++;
    }

    changepoint->counted++;

    // A rise and a fall cannot both be true of the same sample, thus whichever
    // sum arrived first is the answer. Where both stand above the threshold on
    // the same sample, which no ordinary reading does, the larger one is taken.
    bool rose = changepoint->high >= changepoint->threshold;
    bool fell = changepoint->low >= changepoint->threshold;

    if(rose && (!fell || (changepoint->high >= changepoint->low)))
    {
        changepoint->counted = changepoint->since_high;
        changepoint_start_again(changepoint);

        return CHANGEPOINT_ROSE;
    }

    if(fell)
    {
        changepoint->counted = changepoint->since_low;
        changepoint_start_again(changepoint);

        return CHANGEPOINT_FELL;
    }

    return CHANGEPOINT_NONE;
}

uint32_t changepoint_began_ago(const changepoint_t* changepoint)
{
    ASSERT(changepoint != NULL);

    return changepoint->counted;
}

real_t changepoint_running_high(const changepoint_t* changepoint)
{
    ASSERT(changepoint != NULL);

    return changepoint->high;
}

real_t changepoint_running_low(const changepoint_t* changepoint)
{
    ASSERT(changepoint != NULL);

    return changepoint->low;
}

real_t changepoint_delay_for(const changepoint_t* changepoint, real_t change)
{
    ASSERT(changepoint != NULL);

    if(!changepoint->designed)
    {
        return REAL_C(0.0);
    }

    // What the sum gains at every sample once the change is there: the change
    // itself less the half that is taken off each step.
    real_t gain = REAL_ABS(change) - (changepoint->smallest_change
                                      / REAL_C(2.0));

    // A change that gains nothing never arrives, however long it is left.
    if(gain <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    return changepoint->threshold / gain;
}

static void changepoint_start_again(changepoint_t* changepoint)
{
    changepoint->high = REAL_C(0.0);
    changepoint->low = REAL_C(0.0);
    changepoint->since_high = 0u;
    changepoint->since_low = 0u;
}
