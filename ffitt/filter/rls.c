#ifndef TEST
#include <ffitt/filter/rls.h>
#include <ffitt/core/defs.h>
#else
#include "rls.h"
#include "defs.h"
#endif

#include <math.h>
#include <stdlib.h>

bool rls_is_valid_forgetting(real_t forgetting)
{
    return (forgetting >= RLS_SMALLEST_FORGETTING)
           && (forgetting <= RLS_LARGEST_FORGETTING);
}

rls_t rls_alloc(uint32_t length)
{
    ASSERT(length > 0u);

    rls_t rls;

    rls.length = length;
    rls.history = ringbuf_alloc(length);
    rls.coefficient = (real_t*)calloc(length, sizeof(real_t));
    rls.inverse = (real_t*)calloc(RLS_MATRIX_SIZE(length), sizeof(real_t));
    rls.gain = (real_t*)calloc(length, sizeof(real_t));
    rls.carried = (real_t*)calloc(length, sizeof(real_t));
    rls.forgetting = RLS_LARGEST_FORGETTING;
    rls.healthy = true;
    rls.dynamic_alloc = true;

    if((rls.coefficient == NULL) || (rls.inverse == NULL)
       || (rls.gain == NULL) || (rls.carried == NULL))
    {
        rls_free(&rls);
        return rls;
    }

    rls_design(&rls, RLS_LARGEST_FORGETTING, RLS_DEFAULT_DOUBT);

    return rls;
}

rls_t rls_static_alloc(uint32_t length, real_t* coefficient, real_t* inverse,
                       real_t* gain, real_t* carried, real_t* history)
{
    ASSERT(length > 0u);
    ASSERT(coefficient != NULL);
    ASSERT(inverse != NULL);
    ASSERT(gain != NULL);
    ASSERT(carried != NULL);
    ASSERT(history != NULL);

    rls_t rls;

    rls.length = length;
    rls.history = ringbuf_static_alloc(length, history);
    rls.coefficient = coefficient;
    rls.inverse = inverse;
    rls.gain = gain;
    rls.carried = carried;
    rls.forgetting = RLS_LARGEST_FORGETTING;
    rls.healthy = true;
    rls.dynamic_alloc = false;

    rls_design(&rls, RLS_LARGEST_FORGETTING, RLS_DEFAULT_DOUBT);

    return rls;
}

// Reach one element of the matrix the filter carries.
static real_t rls_at(const rls_t* rls, uint32_t row, uint32_t column)
{
    return rls->inverse[(row * rls->length) + column];
}

static void rls_put(rls_t* rls, uint32_t row, uint32_t column, real_t value)
{
    rls->inverse[(row * rls->length) + column] = value;
}

void rls_reset(rls_t* rls)
{
    ASSERT(rls != NULL);

    ringbuf_reset(&rls->history);

    for(uint32_t index = 0; index < rls->length; index++)
    {
        rls->coefficient[index] = REAL_C(0.0);
        rls->gain[index] = REAL_C(0.0);
        rls->carried[index] = REAL_C(0.0);
    }

    // The matrix begins as the doubt multiplied by the unit matrix, which says
    // the filter knows nothing yet and every direction is equally uncertain.
    for(uint32_t row = 0; row < rls->length; row++)
    {
        for(uint32_t column = 0; column < rls->length; column++)
        {
            rls_put(rls, row, column,
                    (row == column) ? rls->doubt : REAL_C(0.0));
        }
    }

    rls->healthy = true;
}

bool rls_design(rls_t* rls, real_t forgetting, real_t doubt)
{
    ASSERT(rls != NULL);

    if(!rls_is_valid_forgetting(forgetting) || (doubt <= REAL_SMALLEST))
    {
        return false;
    }

    rls->forgetting = forgetting;
    rls->doubt = doubt;

    rls_reset(rls);

    return true;
}

bool rls_is_healthy(const rls_t* rls)
{
    ASSERT(rls != NULL);

    return rls->healthy;
}

real_t rls_get_coefficient(const rls_t* rls, uint32_t index)
{
    ASSERT(rls != NULL);
    ASSERT(index < rls->length);

    return rls->coefficient[index];
}

real_t rls_process_sample(rls_t* rls, real_t reference, real_t wanted)
{
    ASSERT(rls != NULL);

    uint32_t length = rls->length;

    ringbuf_put(&rls->history, reference);

    // What the filter says, before it learns anything from this sample.
    real_t guess = REAL_C(0.0);

    for(uint32_t index = 0; index < length; index++)
    {
        guess += rls->coefficient[index] * ringbuf_get(&rls->history, index);
    }

    if(!rls->healthy)
    {
        // Once the matrix has stopped describing a real spread, going on with
        // it only makes the coefficients worse. The filter holds still and
        // says what it last knew.
        return guess;
    }

    // The matrix multiplied by what is in the filter now.
    for(uint32_t row = 0; row < length; row++)
    {
        real_t total = REAL_C(0.0);

        for(uint32_t column = 0; column < length; column++)
        {
            total += rls_at(rls, row, column)
                     * ringbuf_get(&rls->history, column);
        }

        rls->carried[row] = total;
    }

    // How much of the doubt this sample takes away. Where the sample points
    // somewhere the filter has already heard a great deal about, this is small
    // and the step is small.
    real_t along = REAL_C(0.0);

    for(uint32_t index = 0; index < length; index++)
    {
        along += ringbuf_get(&rls->history, index) * rls->carried[index];
    }

    real_t below = rls->forgetting + along;

    // A divisor at or below nothing means the matrix has stopped describing a
    // real spread: no direction can hold a negative amount of doubt.
    if(below <= REAL_SMALLEST)
    {
        rls->healthy = false;
        return guess;
    }

    for(uint32_t index = 0; index < length; index++)
    {
        rls->gain[index] = rls->carried[index] / below;
    }

    // The coefficients move by the error multiplied by the gain, which is the
    // whole of the learning.
    real_t error = wanted - guess;

    for(uint32_t index = 0; index < length; index++)
    {
        rls->coefficient[index] += rls->gain[index] * error;
    }

    // And the matrix loses the doubt that this sample took away.
    //
    // THE TWO HALVES ARE WRITTEN TOGETHER AND NOT WORKED OUT APART. What comes
    // out of this should be symmetric, and nothing in the arithmetic holds it
    // to that: the rounding of each element pulls the two halves a little
    // further from each other, and after enough samples the matrix describes a
    // spread that is negative in some direction and the filter runs away.
    //
    // Working out one half and writing it to both costs nothing and removes
    // the whole of that drift. The ukf module holds its covariance together
    // the same way, for the same reason.
    real_t scale = REAL_C(1.0) / rls->forgetting;

    for(uint32_t row = 0; row < length; row++)
    {
        for(uint32_t column = row; column < length; column++)
        {
            real_t value = (rls_at(rls, row, column)
                            - (rls->gain[row] * rls->carried[column])) * scale;

            rls_put(rls, row, column, value);
            rls_put(rls, column, row, value);
        }
    }

    // The diagonal is how much doubt is left in each direction, and no
    // direction can hold less than none of it.
    for(uint32_t index = 0; index < length; index++)
    {
        if(rls_at(rls, index, index) <= REAL_C(0.0))
        {
            rls->healthy = false;
            break;
        }
    }

    return guess;
}

real_t rls_error(rls_t* rls, real_t reference, real_t wanted)
{
    return wanted - rls_process_sample(rls, reference, wanted);
}

void rls_free(rls_t* rls)
{
    ASSERT(rls != NULL);

    ringbuf_free(&rls->history);

    if(rls->dynamic_alloc)
    {
        free(rls->coefficient);
        free(rls->inverse);
        free(rls->gain);
        free(rls->carried);
    }

    rls->coefficient = NULL;
    rls->inverse = NULL;
    rls->gain = NULL;
    rls->carried = NULL;
    rls->length = 0u;
    rls->healthy = false;
}

bool rls_process_block(rls_t* rls, const real_t* reference,
                       const real_t* wanted, real_t* output, real_t* error,
                       uint32_t count)
{
    ASSERT(rls != NULL);
    ASSERT(reference != NULL);
    ASSERT(wanted != NULL);

    if(rls->coefficient == NULL)
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        real_t made = rls_process_sample(rls, reference[index],
                                         wanted[index]);

        if(output != NULL)
        {
            output[index] = made;
        }

        if(error != NULL)
        {
            error[index] = wanted[index] - made;
        }
    }

    return rls_is_healthy(rls);
}
