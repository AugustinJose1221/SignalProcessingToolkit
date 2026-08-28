#ifndef TEST
#include <sptk/filter/farrow.h>
#include <sptk/core/defs.h>
#else
#include "farrow.h"
#include "defs.h"
#endif

#include <stdlib.h>

bool farrow_is_valid_order(uint32_t order)
{
    return (order >= 1u) && (order <= FARROW_LARGEST_ORDER);
}

real_t farrow_smallest_delay(uint32_t order)
{
    // The curve is laid through the samples either side of the place wanted,
    // thus the place must sit in the middle of them and the filter must wait
    // for the later half to arrive.
    return (real_t)order / REAL_C(2.0);
}

real_t farrow_largest_delay(uint32_t order)
{
    return farrow_smallest_delay(order) + REAL_C(1.0);
}

bool farrow_is_valid_delay(const farrow_t* farrow, real_t delay)
{
    ASSERT(farrow != NULL);

    return (delay >= farrow_smallest_delay(farrow->order))
           && (delay <= farrow_largest_delay(farrow->order));
}

// Work out the weights, once, when the filter is built.
//
// THE WEIGHT OF EACH SAMPLE IS A POLYNOMIAL IN THE DELAY, and that is the whole
// idea. The curve laid through the samples is the one Lagrange wrote down: the
// weight of sample k is the product, across every OTHER sample j, of how far the
// place wanted stands from j divided by how far k stands from j.
//
//     h_k(D) = product over j not k of (D - j) / (k - j)
//
// Read as arithmetic that is a division for every sample at every step. Read as
// a POLYNOMIAL IN D it is a fixed set of numbers that never change, and moving
// the delay is then reading those polynomials at another number. That is what
// lets the delay move at every sample for almost nothing, and it is what the
// structure is for.
static void farrow_build_weights(real_t* weight, uint32_t order)
{
    uint32_t taps = FARROW_TAP_COUNT(order);

    for(uint32_t k = 0; k < taps; k++)
    {
        real_t* into = &weight[k * taps];

        for(uint32_t index = 0; index < taps; index++)
        {
            into[index] = REAL_C(0.0);
        }

        // The product starts at one, which is a polynomial of no order at all.
        into[0] = REAL_C(1.0);

        uint32_t degree = 0u;
        real_t divisor = REAL_C(1.0);

        for(uint32_t j = 0; j < taps; j++)
        {
            if(j == k)
            {
                continue;
            }

            // Multiply what is there by (D - j), from the highest power
            // downwards so that nothing is written over before it is read.
            for(uint32_t power = degree + 1u; power >= 1u; power--)
            {
                into[power] = into[power - 1u] - ((real_t)j * into[power]);
            }

            into[0] = -((real_t)j) * into[0];

            degree++;
            divisor *= ((real_t)k - (real_t)j);
        }

        for(uint32_t index = 0; index < taps; index++)
        {
            into[index] /= divisor;
        }
    }
}

static farrow_t farrow_make(uint32_t order, real_t* weight, real_t* working)
{
    farrow_t farrow;

    farrow.weight = weight;
    farrow.working = working;
    farrow.order = order;
    farrow.delay = farrow_smallest_delay(order);
    farrow.dynamic_alloc = false;

    return farrow;
}

farrow_t farrow_alloc(uint32_t order)
{
    ASSERT(farrow_is_valid_order(order));

    uint32_t taps = FARROW_TAP_COUNT(order);

    real_t* weight = (real_t*)calloc(FARROW_WEIGHT_COUNT(order),
                                     sizeof(real_t));
    real_t* working = (real_t*)calloc(taps, sizeof(real_t));

    farrow_t farrow = farrow_make(order, weight, working);

    farrow.history = ringbuf_alloc(taps);
    farrow.dynamic_alloc = true;

    if((weight == NULL) || (working == NULL)
       || (farrow.history.data == NULL))
    {
        farrow_free(&farrow);

        return farrow;
    }

    farrow_build_weights(weight, order);

    return farrow;
}

farrow_t farrow_static_alloc(uint32_t order, real_t* history, real_t* weight,
                             real_t* working)
{
    ASSERT(farrow_is_valid_order(order));
    ASSERT(history != NULL);
    ASSERT(weight != NULL);
    ASSERT(working != NULL);

    farrow_t farrow = farrow_make(order, weight, working);

    farrow.history = ringbuf_static_alloc(FARROW_TAP_COUNT(order), history);

    farrow_build_weights(weight, order);

    return farrow;
}

bool farrow_set_delay(farrow_t* farrow, real_t delay)
{
    ASSERT(farrow != NULL);

    if(!farrow_is_valid_delay(farrow, delay))
    {
        return false;
    }

    farrow->delay = delay;

    return true;
}

real_t farrow_get_delay(const farrow_t* farrow)
{
    ASSERT(farrow != NULL);

    return farrow->delay;
}

real_t farrow_process_sample(farrow_t* farrow, real_t sample)
{
    ASSERT(farrow != NULL);

    if((farrow->weight == NULL) || (farrow->working == NULL))
    {
        return REAL_C(0.0);
    }

    uint32_t taps = FARROW_TAP_COUNT(farrow->order);

    ringbuf_put(&farrow->history, sample);

    for(uint32_t power = 0; power < taps; power++)
    {
        farrow->working[power] = REAL_C(0.0);
    }

    // One running total for each power of the delay. Each of them is a plain
    // filter whose weights never move, thus this half of the work does not
    // depend on the delay at all.
    for(uint32_t tap = 0; tap < taps; tap++)
    {
        real_t held = ringbuf_get(&farrow->history, tap);
        const real_t* weight = &farrow->weight[tap * taps];

        for(uint32_t power = 0; power < taps; power++)
        {
            farrow->working[power] += weight[power] * held;
        }
    }

    // And the delay comes in once, here, from the highest power inwards.
    real_t answer = farrow->working[taps - 1u];

    for(uint32_t power = taps - 1u; power >= 1u; power--)
    {
        answer = (answer * farrow->delay) + farrow->working[power - 1u];
    }

    return answer;
}

bool farrow_process_block(farrow_t* farrow, const real_t* input,
                          real_t* output, uint32_t count)
{
    ASSERT(farrow != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if((farrow->weight == NULL) || (farrow->working == NULL))
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        output[index] = farrow_process_sample(farrow, input[index]);
    }

    return true;
}

void farrow_reset(farrow_t* farrow)
{
    ASSERT(farrow != NULL);

    ringbuf_reset(&farrow->history);

    if(farrow->working != NULL)
    {
        for(uint32_t power = 0; power < FARROW_TAP_COUNT(farrow->order);
            power++)
        {
            farrow->working[power] = REAL_C(0.0);
        }
    }
}

void farrow_free(farrow_t* farrow)
{
    ASSERT(farrow != NULL);

    if(!farrow->dynamic_alloc)
    {
        return;
    }

    ringbuf_free(&farrow->history);

    free(farrow->weight);
    free(farrow->working);

    farrow->weight = NULL;
    farrow->working = NULL;
}
