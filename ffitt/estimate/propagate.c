#ifndef TEST
#include <ffitt/estimate/propagate.h>
#include <ffitt/core/defs.h>
#else
#include "propagate.h"
#include "defs.h"
#endif

#include <math.h>

bool propagate_is_valid_method(propagate_method_t method)
{
    return (method >= PROPAGATE_EULER) && (method <= PROPAGATE_RUNGE);
}

bool propagate_is_valid_count(uint32_t count)
{
    return (count > 0u) && (count <= PROPAGATE_LARGEST_STATE);
}

uint32_t propagate_asks_for_each_step(propagate_method_t method)
{
    switch(method)
    {
        case PROPAGATE_EULER:
            return 1u;

        case PROPAGATE_MIDPOINT:
            return 2u;

        case PROPAGATE_RUNGE:
            return 4u;

        default:
            return 0u;
    }
}

bool propagate_state(propagate_method_t method, propagate_rate_t rate,
                     real_t time, real_t step, real_t* state,
                     const real_t* input, uint32_t count)
{
    ASSERT(rate != NULL);
    ASSERT(state != NULL);

    if(!propagate_is_valid_method(method) || !propagate_is_valid_count(count)
       || (step <= REAL_C(0.0)))
    {
        return false;
    }

    // The copies live here rather than in memory the caller gives, because a
    // model of a handful of states is what this is for and a handful fits on
    // the stack. PROPAGATE_LARGEST_STATE is what bounds it.
    real_t first[PROPAGATE_LARGEST_STATE];
    real_t second[PROPAGATE_LARGEST_STATE];
    real_t third[PROPAGATE_LARGEST_STATE];
    real_t fourth[PROPAGATE_LARGEST_STATE];
    real_t along[PROPAGATE_LARGEST_STATE];

    rate(time, state, input, first, count);

    if(method == PROPAGATE_EULER)
    {
        // One ask, and the whole step taken at that one rate. Everything that
        // happens to the rate within the step is missed, which is why the
        // error follows the step itself and not its square.
        for(uint32_t index = 0; index < count; index++)
        {
            state[index] += step * first[index];
        }

        return true;
    }

    // Half a step along the first rate, to see what the rate is in the middle.
    real_t half = step / REAL_C(2.0);

    for(uint32_t index = 0; index < count; index++)
    {
        along[index] = state[index] + (half * first[index]);
    }

    rate(time + half, along, input, second, count);

    if(method == PROPAGATE_MIDPOINT)
    {
        // The whole step taken at the rate in the middle. That catches how the
        // rate is changing across the step, thus the error follows the square
        // of the step.
        for(uint32_t index = 0; index < count; index++)
        {
            state[index] += step * second[index];
        }

        return true;
    }

    // RUNGE. Half a step again, but along the rate found in the middle rather
    // than the one found at the start.
    for(uint32_t index = 0; index < count; index++)
    {
        along[index] = state[index] + (half * second[index]);
    }

    rate(time + half, along, input, third, count);

    // And a whole step, along that third rate, to see the rate at the end.
    for(uint32_t index = 0; index < count; index++)
    {
        along[index] = state[index] + (step * third[index]);
    }

    rate(time + step, along, input, fourth, count);

    // The four rates weighed together. The two found in the middle count for
    // twice as much as the two found at the ends, and that weighing is what
    // makes the error follow the fourth power of the step.
    for(uint32_t index = 0; index < count; index++)
    {
        real_t weighed = first[index]
                         + (REAL_C(2.0) * second[index])
                         + (REAL_C(2.0) * third[index])
                         + fourth[index];

        state[index] += (step * weighed) / REAL_C(6.0);
    }

    return true;
}

bool propagate_state_over(propagate_method_t method, propagate_rate_t rate,
                          real_t time, real_t across, uint32_t steps,
                          real_t* state, const real_t* input, uint32_t count)
{
    ASSERT(rate != NULL);
    ASSERT(state != NULL);

    if((steps == 0u) || (across <= REAL_C(0.0)))
    {
        return false;
    }

    real_t step = across / (real_t)steps;

    for(uint32_t taken = 0; taken < steps; taken++)
    {
        // The time of each step is worked out from where the stretch began
        // rather than added up, so that the rounding of one step cannot gather
        // across the whole of them.
        real_t at = time + ((across * (real_t)taken) / (real_t)steps);

        if(!propagate_state(method, rate, at, step, state, input, count))
        {
            return false;
        }
    }

    return true;
}
