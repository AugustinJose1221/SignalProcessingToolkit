#ifndef TEST
#include <sptk/linalg/lstsq.h>
#include <sptk/core/defs.h>
#else
#include "lstsq.h"
#include "defs.h"
#endif

#include <math.h>

bool lstsq_is_valid_fit(uint32_t size, uint32_t order)
{
    uint32_t wanted = LSTSQ_COEFFICIENT_COUNT(order);

    // As many readings as numbers to find, at least. Fewer leaves the answer
    // undecided, and exactly as many is not a fit at all but a solve.
    return (order <= LSTSQ_HIGHEST_ORDER) && (size >= wanted);
}

// Solve a triangle by walking down it and then back up it.
//
// The factor of Cholesky turns one square problem into two triangular ones,
// and a triangle is solved by substitution: the first row holds one unknown,
// the second holds that one and one more, and so on down. Then the same again
// upwards through the transpose.
static bool lstsq_solve_triangles(matrix_t* factor, matrix_t* right,
                                  matrix_t* answer)
{
    uint32_t order = factor->m;

    // Down: solve for the working column.
    for(uint32_t i = 0; i < order; i++)
    {
        real_t total = matrix_get_element(right, i, 0);

        for(uint32_t k = 0; k < i; k++)
        {
            total -= matrix_get_element(factor, i, k)
                     * matrix_get_element(answer, k, 0);
        }

        real_t diagonal = matrix_get_element(factor, i, i);

        if(REAL_ABS(diagonal) <= REAL_SMALLEST)
        {
            return false;
        }

        matrix_add_element(answer, i, 0, total / diagonal);
    }

    // Up: solve through the transpose, which needs no transpose to be formed
    // because the element across the diagonal is read instead.
    for(uint32_t step = order; step >= 1u; step--)
    {
        uint32_t i = step - 1u;
        real_t total = matrix_get_element(answer, i, 0);

        for(uint32_t k = i + 1u; k < order; k++)
        {
            total -= matrix_get_element(factor, k, i)
                     * matrix_get_element(answer, k, 0);
        }

        real_t diagonal = matrix_get_element(factor, i, i);

        if(REAL_ABS(diagonal) <= REAL_SMALLEST)
        {
            return false;
        }

        matrix_add_element(answer, i, 0, total / diagonal);
    }

    return true;
}

bool lstsq_solve(matrix_t* model, matrix_t* readings, matrix_t* answer,
                 matrix_t* square, matrix_t* factor, matrix_t* column)
{
    ASSERT(model != NULL);
    ASSERT(readings != NULL);
    ASSERT(answer != NULL);
    ASSERT(square != NULL);
    ASSERT(factor != NULL);
    ASSERT(column != NULL);

    uint32_t rows = model->m;
    uint32_t columns = model->n;

    if((readings->m != rows) || (readings->n != 1u)
       || (answer->m != columns) || (answer->n != 1u)
       || (square->m != columns) || (square->n != columns)
       || (factor->m != columns) || (factor->n != columns)
       || (column->m != columns) || (column->n != 1u))
    {
        return false;
    }
    if(rows < columns)
    {
        return false;
    }

    // The model turned round and multiplied by itself. This is always square,
    // always symmetric, and as wide as the number of things to find, however
    // many readings there are.
    for(uint32_t i = 0; i < columns; i++)
    {
        for(uint32_t j = 0; j < columns; j++)
        {
            real_t total = REAL_C(0.0);

            for(uint32_t k = 0; k < rows; k++)
            {
                total += matrix_get_element(model, k, i)
                         * matrix_get_element(model, k, j);
            }

            matrix_add_element(square, i, j, total);
        }

        real_t total = REAL_C(0.0);
        for(uint32_t k = 0; k < rows; k++)
        {
            total += matrix_get_element(model, k, i)
                     * matrix_get_element(readings, k, 0);
        }
        matrix_add_element(column, i, 0, total);
    }

    // A square problem that is symmetric and, when the model is sound,
    // positive in every direction. Where it is not, two columns of the model
    // say the same thing and there is no single answer to give.
    if(!matrix_cholesky_into(square, factor))
    {
        return false;
    }

    // WHERE THE ANSWER IS REFUSED FOR BEING NOT WORTH HAVING.
    //
    // A factor exists long after the answer has stopped meaning anything. Two
    // columns that say ALMOST the same thing leave a diagonal of the factor
    // that is tiny beside the others, and dividing by it in the substitution
    // multiplies whatever error the readings carry by the ratio of the two.
    //
    // The square of that ratio is how badly conditioned the small problem is,
    // and the width of the build can hold about 1 divided by the smallest step
    // it can tell. Beyond that the answer is made of rounding, and this is
    // where the module says so instead of giving it back.
    real_t largest_pivot = REAL_C(0.0);
    real_t smallest_pivot = REAL_LARGEST;

    for(uint32_t i = 0; i < columns; i++)
    {
        real_t pivot = REAL_ABS(matrix_get_element(factor, i, i));

        if(pivot > largest_pivot) { largest_pivot = pivot; }
        if(pivot < smallest_pivot) { smallest_pivot = pivot; }
    }

    if(smallest_pivot <= (largest_pivot * LSTSQ_SMALLEST_PIVOT_PART))
    {
        return false;
    }

    return lstsq_solve_triangles(factor, column, answer);
}

bool lstsq_polyfit(const real_t* x, const real_t* y, uint32_t size,
                   uint32_t order, real_t* coefficients)
{
    ASSERT(x != NULL);
    ASSERT(y != NULL);
    ASSERT(coefficients != NULL);

    if(!lstsq_is_valid_fit(size, order))
    {
        return false;
    }

    uint32_t wanted = LSTSQ_COEFFICIENT_COUNT(order);

    matrix_t model = matrix_alloc(size, wanted);
    matrix_t readings = matrix_alloc(size, 1);
    matrix_t answer = matrix_alloc(wanted, 1);
    matrix_t square = matrix_alloc(wanted, wanted);
    matrix_t factor = matrix_alloc(wanted, wanted);
    matrix_t column = matrix_alloc(wanted, 1);

    // One row for each reading: 1, x, x squared, and so on. Each power is the
    // one before it multiplied by x, thus no power is worked out on its own.
    for(uint32_t row = 0; row < size; row++)
    {
        real_t power = REAL_C(1.0);

        for(uint32_t which = 0; which < wanted; which++)
        {
            matrix_add_element(&model, row, which, power);
            power *= x[row];
        }

        matrix_add_element(&readings, row, 0, y[row]);
    }

    bool solved = lstsq_solve(&model, &readings, &answer, &square, &factor,
                              &column);

    if(solved)
    {
        for(uint32_t which = 0; which < wanted; which++)
        {
            coefficients[which] = matrix_get_element(&answer, which, 0);
        }
    }

    matrix_free(&model);
    matrix_free(&readings);
    matrix_free(&answer);
    matrix_free(&square);
    matrix_free(&factor);
    matrix_free(&column);

    return solved;
}

void lstsq_scaling(const real_t* x, uint32_t size, real_t* centre,
                   real_t* width)
{
    ASSERT(x != NULL);
    ASSERT(centre != NULL);
    ASSERT(width != NULL);

    if(size == 0u)
    {
        *centre = REAL_C(0.0);
        *width = REAL_C(1.0);
        return;
    }

    real_t lowest = x[0];
    real_t highest = x[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(x[index] < lowest)  { lowest = x[index]; }
        if(x[index] > highest) { highest = x[index]; }
    }

    *centre = (lowest + highest) / REAL_C(2.0);

    real_t half = (highest - lowest) / REAL_C(2.0);

    // Readings that all share one x have no width. Giving 1 changes nothing
    // and lets the caller carry on to a fit that refuses for the real reason,
    // which is that one x cannot fix a curve.
    *width = (half > REAL_C(0.0)) ? half : REAL_C(1.0);
}

bool lstsq_polyfit_scaled(const real_t* x, const real_t* y, uint32_t size,
                          uint32_t order, real_t* coefficients,
                          real_t* centre, real_t* width)
{
    ASSERT(x != NULL);
    ASSERT(centre != NULL);
    ASSERT(width != NULL);

    lstsq_scaling(x, size, centre, width);

    if(!lstsq_is_valid_fit(size, order))
    {
        return false;
    }

    // The scaled places, worked out once into memory of their own so that the
    // readings the caller gave are left alone.
    real_t* scaled = (real_t*)malloc(sizeof(real_t)*size);

    if(scaled == NULL)
    {
        return false;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        scaled[index] = (x[index] - *centre) / *width;
    }

    bool fitted = lstsq_polyfit(scaled, y, size, order, coefficients);

    free(scaled);

    return fitted;
}

real_t lstsq_evaluate_scaled(const real_t* coefficients, uint32_t order,
                             real_t centre, real_t width, real_t x)
{
    ASSERT(coefficients != NULL);

    real_t place = (width > REAL_C(0.0)) ? ((x - centre) / width)
                                         : REAL_C(0.0);

    return lstsq_evaluate(coefficients, order, place);
}

real_t lstsq_evaluate(const real_t* coefficients, uint32_t order, real_t x)
{
    ASSERT(coefficients != NULL);

    // From the highest power inwards. This needs one multiplication and one
    // addition for each order and never forms a power on its own: working out
    // x to the ninth directly loses digits that this way keeps.
    real_t total = coefficients[order];

    for(uint32_t step = order; step >= 1u; step--)
    {
        total = (total * x) + coefficients[step - 1u];
    }

    return total;
}

real_t lstsq_fit_quality_scaled(const real_t* x, const real_t* y, uint32_t size,
                                const real_t* coefficients, uint32_t order,
                                real_t centre, real_t width)
{
    ASSERT(x != NULL);
    ASSERT(y != NULL);
    ASSERT(coefficients != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t mean = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        mean += y[index];
    }
    mean /= (real_t)size;

    real_t left_over = REAL_C(0.0);
    real_t altogether = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t missed = y[index] - lstsq_evaluate_scaled(coefficients, order,
                                                         centre, width,
                                                         x[index]);
        real_t moved = y[index] - mean;

        left_over += missed * missed;
        altogether += moved * moved;
    }

    if(altogether <= REAL_C(0.0))
    {
        return REAL_C(1.0);
    }

    real_t quality = REAL_C(1.0) - (left_over / altogether);

    return (quality < REAL_C(0.0)) ? REAL_C(0.0) : quality;
}

real_t lstsq_fit_quality(const real_t* x, const real_t* y, uint32_t size,
                         const real_t* coefficients, uint32_t order)
{
    ASSERT(x != NULL);
    ASSERT(y != NULL);
    ASSERT(coefficients != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t mean = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        mean += y[index];
    }
    mean /= (real_t)size;

    real_t left_over = REAL_C(0.0);
    real_t altogether = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t missed = y[index] - lstsq_evaluate(coefficients, order, x[index]);
        real_t moved = y[index] - mean;

        left_over += missed * missed;
        altogether += moved * moved;
    }

    // Readings that never move are followed perfectly by any constant, thus
    // there is no movement to account for and 1 is the honest answer.
    if(altogether <= REAL_C(0.0))
    {
        return REAL_C(1.0);
    }

    real_t quality = REAL_C(1.0) - (left_over / altogether);

    // A fit worse than a flat line through the mean gives a negative number,
    // which is a real answer and not a fault. It is held at 0 because the
    // header promises 0 to 1, and anything below 0 says the same thing: the
    // curve does not follow the readings.
    return (quality < REAL_C(0.0)) ? REAL_C(0.0) : quality;
}
