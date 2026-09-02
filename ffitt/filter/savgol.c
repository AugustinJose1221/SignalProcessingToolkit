#ifndef TEST
#include <ffitt/filter/savgol.h>
#include <ffitt/linalg/matrix.h>
#include <ffitt/core/defs.h>
#else
#include "savgol.h"
#include "matrix.h"
#include "defs.h"
#endif

static real_t savgol_factorial_ratio(uint32_t derivative);

savgol_t savgol_alloc(uint32_t window)
{
    ASSERT(window > 0);
    ASSERT((window % 2) == 1);

    savgol_t savgol;

    savgol.window = window;
    savgol.order = 0;
    savgol.derivative = 0;
    savgol.coefficient = (real_t*)malloc(sizeof(real_t)*window);
    savgol.dynamic_alloc = true;

    // The clearing below writes through the list, thus it must not be reached
    // with nothing to write to.
    if(savgol.coefficient == NULL)
    {
        savgol.window = 0;
        savgol.dynamic_alloc = false;

        return savgol;
    }

    for(uint32_t index = 0; index < window; index++)
    {
        savgol.coefficient[index] = REAL_C(0.0);
    }

    return savgol;
}

savgol_t savgol_static_alloc(uint32_t window, real_t* coefficient)
{
    ASSERT(window > 0);
    ASSERT((window % 2) == 1);
    ASSERT(coefficient != NULL);

    savgol_t savgol;

    savgol.window = window;
    savgol.order = 0;
    savgol.derivative = 0;
    savgol.coefficient = coefficient;
    savgol.dynamic_alloc = false;

    for(uint32_t index = 0; index < window; index++)
    {
        savgol.coefficient[index] = REAL_C(0.0);
    }

    return savgol;
}

bool savgol_is_valid(uint32_t window, uint32_t order, uint32_t derivative)
{
    if((window % 2) != 1)
    {
        return false;
    }
    if(window <= order)
    {
        return false;
    }
    if(derivative > order)
    {
        return false;
    }

    return true;
}

bool savgol_design(savgol_t* savgol, uint32_t order, uint32_t derivative)
{
    ASSERT(savgol != NULL);

    if(!savgol_is_valid(savgol->window, order, derivative))
    {
        return false;
    }

    uint32_t window = savgol->window;
    uint32_t terms = order + 1;
    int32_t half = (int32_t)(window / 2);

    // The matrix of the powers. The row of a sample holds 1, x, x*x and so on,
    // where x is the distance of that sample from the middle of the window.
    matrix_t powers = matrix_alloc(window, terms);
    for(uint32_t row = 0; row < window; row++)
    {
        real_t position = (real_t)((int32_t)row - half);
        real_t value = REAL_C(1.0);

        for(uint32_t column = 0; column < terms; column++)
        {
            matrix_add_element(&powers, row, column, value);
            value *= position;
        }
    }

    // The normal equations of the least squares: the coefficients of the
    // polynomial are inverse(P' * P) * P' * y. The filter needs one row of
    // that product only, which is the row of the derivative that the caller
    // asked for.
    matrix_t transpose = matrix_transpose(&powers);
    matrix_t normal = matrix_multiply(&transpose, &powers);
    matrix_t inverse = matrix_alloc(terms, terms);
    matrix_t scratch = matrix_alloc(terms, 2*terms);

    bool ok = matrix_inverse_into(&normal, &inverse, &scratch);

    if(ok)
    {
        // The value of the polynomial at the middle is its first coefficient,
        // the first derivative is its second coefficient, and so on. The
        // derivative of the power d also brings the factor d factorial.
        real_t factor = savgol_factorial_ratio(derivative);

        for(uint32_t index = 0; index < window; index++)
        {
            real_t sum = REAL_C(0.0);

            for(uint32_t term = 0; term < terms; term++)
            {
                sum += matrix_get_element(&inverse, derivative, term)
                       * matrix_get_element(&transpose, term, index);
            }

            savgol->coefficient[index] = sum * factor;
        }

        savgol->order = order;
        savgol->derivative = derivative;
    }

    matrix_free(&powers);
    matrix_free(&transpose);
    matrix_free(&normal);
    matrix_free(&inverse);
    matrix_free(&scratch);

    return ok;
}

real_t savgol_get_coefficient(savgol_t* savgol, uint32_t index)
{
    ASSERT(savgol != NULL);
    ASSERT(index < savgol->window);

    return savgol->coefficient[index];
}

real_t savgol_apply(savgol_t* savgol, const real_t* window)
{
    ASSERT(savgol != NULL);
    ASSERT(window != NULL);

    real_t result = REAL_C(0.0);

    for(uint32_t index = 0; index < savgol->window; index++)
    {
        result += savgol->coefficient[index] * window[index];
    }

    return result;
}

void savgol_process_block(savgol_t* savgol, const real_t* input, real_t* output,
                          uint32_t size)
{
    ASSERT(savgol != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);
    ASSERT(input != output);

    int32_t half = (int32_t)(savgol->window / 2);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t result = REAL_C(0.0);

        for(uint32_t tap = 0; tap < savgol->window; tap++)
        {
            int32_t position = (int32_t)index + (int32_t)tap - half;

            // Near the two ends the window reaches outside the signal. Repeat
            // the first and the last sample to fill it.
            if(position < 0)
            {
                position = 0;
            }
            if(position >= (int32_t)size)
            {
                position = (int32_t)size - 1;
            }

            result += savgol->coefficient[tap] * input[position];
        }

        output[index] = result;
    }
}

void savgol_free(savgol_t* savgol)
{
    ASSERT(savgol != NULL);

    if(savgol->dynamic_alloc)
    {
        free(savgol->coefficient);
        savgol->coefficient = NULL;
        savgol->dynamic_alloc = false;
    }
}

// Give the factor that the derivative brings. The derivative of the power d
// gives d factorial.
static real_t savgol_factorial_ratio(uint32_t derivative)
{
    real_t result = REAL_C(1.0);

    for(uint32_t index = 2; index <= derivative; index++)
    {
        result *= (real_t)index;
    }

    return result;
}
