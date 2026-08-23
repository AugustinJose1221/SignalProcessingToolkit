#ifndef TEST
#include <sptk/filter/adaptive.h>
#include <sptk/core/defs.h>
#else
#include "adaptive.h"
#include "defs.h"
#endif

#include <math.h>

bool adaptive_is_valid_rule(adaptive_rule_t rule)
{
    return (rule >= ADAPTIVE_PLAIN) && (rule <= ADAPTIVE_SIGN);
}

adaptive_t adaptive_alloc(uint32_t length)
{
    ASSERT(length > 0);

    adaptive_t adaptive;

    adaptive.history = ringbuf_alloc(length);
    adaptive.coefficient = (real_t*)malloc(sizeof(real_t)*length);
    adaptive.length = length;
    adaptive.rule = ADAPTIVE_NORMALISED;
    adaptive.rate = REAL_C(0.2);
    adaptive.leak = REAL_C(0.0);
    adaptive.energy = REAL_C(0.0);
    adaptive.dynamic_alloc = true;

    adaptive_reset(&adaptive);

    return adaptive;
}

adaptive_t adaptive_static_alloc(uint32_t length, real_t* coefficient,
                                 real_t* history)
{
    ASSERT(length > 0);
    ASSERT(coefficient != NULL);
    ASSERT(history != NULL);

    adaptive_t adaptive;

    adaptive.history = ringbuf_static_alloc(length, history);
    adaptive.coefficient = coefficient;
    adaptive.length = length;
    adaptive.rule = ADAPTIVE_NORMALISED;
    adaptive.rate = REAL_C(0.2);
    adaptive.leak = REAL_C(0.0);
    adaptive.energy = REAL_C(0.0);
    adaptive.dynamic_alloc = false;

    adaptive_reset(&adaptive);

    return adaptive;
}

bool adaptive_design(adaptive_t* adaptive, adaptive_rule_t rule, real_t rate)
{
    ASSERT(adaptive != NULL);

    if(!adaptive_is_valid_rule(rule) || (rate <= REAL_C(0.0)))
    {
        return false;
    }

    // The normalised rule is stable for any signal while the rate lies between
    // 0 and 2, and outside that it runs away whatever the signal is. That is a
    // rule of the arithmetic and not of the work, thus it is held here.
    if((rule == ADAPTIVE_NORMALISED) && (rate >= REAL_C(2.0)))
    {
        return false;
    }

    adaptive->rule = rule;
    adaptive->rate = rate;

    return true;
}

bool adaptive_set_leak(adaptive_t* adaptive, real_t leak)
{
    ASSERT(adaptive != NULL);

    if((leak < REAL_C(0.0)) || (leak > REAL_C(1.0)))
    {
        return false;
    }

    adaptive->leak = leak;

    return true;
}

void adaptive_reset(adaptive_t* adaptive)
{
    ASSERT(adaptive != NULL);

    ringbuf_reset(&adaptive->history);
    adaptive->energy = REAL_C(0.0);

    for(uint32_t index = 0; index < adaptive->length; index++)
    {
        adaptive->coefficient[index] = REAL_C(0.0);
    }
}

real_t adaptive_error(adaptive_t* adaptive, real_t reference, real_t wanted)
{
    ASSERT(adaptive != NULL);

    // The energy of what is in the filter, kept as a running total: the sample
    // that arrives is added and the one that falls off the end is taken away.
    // Working it out afresh would cost one pass over the filter for every
    // sample, which is as much again as the filtering itself.
    if(ringbuf_is_full(&adaptive->history))
    {
        real_t leaving = ringbuf_get(&adaptive->history, adaptive->length - 1u);
        adaptive->energy -= leaving * leaving;
    }
    adaptive->energy += reference * reference;

    if(adaptive->energy < REAL_C(0.0))
    {
        // A running total of squares can only be positive. Rounding can still
        // carry it a little below, and a step divided by a negative energy
        // would move the wrong way.
        adaptive->energy = REAL_C(0.0);
    }

    ringbuf_put(&adaptive->history, reference);

    // What the filter makes of the reference as it stands.
    real_t guess = REAL_C(0.0);
    for(uint32_t index = 0; index < adaptive->length; index++)
    {
        guess += adaptive->coefficient[index] * ringbuf_get(&adaptive->history,
                                                            index);
    }

    real_t error = wanted - guess;

    // How far each coefficient moves. The three rules differ here and nowhere
    // else.
    real_t step;
    switch(adaptive->rule)
    {
        case ADAPTIVE_NORMALISED:
            // Divided by the energy in the filter, thus the step does not
            // follow how loud the reference is. The floor stops a silent
            // reference from making the step run away.
            step = (adaptive->rate * error)
                   / (adaptive->energy + ADAPTIVE_FLOOR);
            break;

        case ADAPTIVE_SIGN:
            // Only the sign of the error is used, thus the step never depends
            // on how large the error is. This costs no multiplication at all
            // and suits a processor with none, at the price of settling more
            // slowly and never settling quite as close.
            step = (error > REAL_C(0.0)) ? adaptive->rate
                   : ((error < REAL_C(0.0)) ? -adaptive->rate : REAL_C(0.0));
            break;

        case ADAPTIVE_PLAIN:
        default:
            step = adaptive->rate * error;
            break;
    }

    real_t keep = REAL_C(1.0) - adaptive->leak;

    for(uint32_t index = 0; index < adaptive->length; index++)
    {
        adaptive->coefficient[index] =
            (keep * adaptive->coefficient[index])
            + (step * ringbuf_get(&adaptive->history, index));
    }

    return error;
}

real_t adaptive_process_sample(adaptive_t* adaptive, real_t reference,
                               real_t wanted)
{
    // What the filter makes of the reference is what it was aiming at less
    // what is left over.
    return wanted - adaptive_error(adaptive, reference, wanted);
}

real_t adaptive_get_coefficient(const adaptive_t* adaptive, uint32_t index)
{
    ASSERT(adaptive != NULL);

    if(index >= adaptive->length)
    {
        return REAL_C(0.0);
    }

    return adaptive->coefficient[index];
}

real_t adaptive_get_energy(const adaptive_t* adaptive)
{
    ASSERT(adaptive != NULL);

    return adaptive->energy;
}

void adaptive_free(adaptive_t* adaptive)
{
    ASSERT(adaptive != NULL);

    ringbuf_free(&adaptive->history);

    if(adaptive->dynamic_alloc)
    {
        free(adaptive->coefficient);
        adaptive->coefficient = NULL;
        adaptive->dynamic_alloc = false;
    }
}
