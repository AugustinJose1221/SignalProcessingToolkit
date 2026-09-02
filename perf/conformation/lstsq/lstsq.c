#include <perf/conformation/support.h>
#include <perf/conformation/lstsq/lstsq.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <gsl/gsl_multifit.h>

// The curve that this library fits set against the curve the GNU Scientific
// Library fits.
//
// BOTH COUNT THE COEFFICIENTS FROM THE CONSTANT UPWARDS. The header of this
// module says so in as many words: the first number is the constant, the
// second multiplies x, the third x squared. The other library gives back
// whatever the columns of the model were given as, thus the model here is
// built with the constant in the first column and the answer reads the same
// way round.
//
// THE PLACES ARE KEPT NEAR NOTHING ON PURPOSE. The header warns that a fit
// whose places sit far from zero cannot be trusted, because the model then
// holds columns that say nearly the same thing. Fitting such a set would be
// asking both libraries a question neither can answer well, and the two would
// part company for a reason that is not a fault in either.

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static bool polyfit_check(uint32_t size, uint32_t order)
{
    uint32_t count = LSTSQ_COEFFICIENT_COUNT(order);
    real_t* x = (real_t*)malloc(sizeof(real_t) * size);
    real_t* y = (real_t*)malloc(sizeof(real_t) * size);
    real_t* mine = (real_t*)malloc(sizeof(real_t) * count);
    bool flag = true;

    gsl_matrix* model = gsl_matrix_alloc(size, count);
    gsl_vector* readings = gsl_vector_alloc(size);
    gsl_vector* theirs = gsl_vector_alloc(count);
    gsl_matrix* covariance = gsl_matrix_alloc(count, count);
    gsl_multifit_linear_workspace* work =
        gsl_multifit_linear_alloc(size, count);
    double left_over;

    for(uint32_t k = 0; k < size; k++)
    {
        // The places run from -1 to 1, which keeps the columns of the model
        // apart from each other.
        x[k] = REAL_C(-1.0) + ((REAL_C(2.0) * (real_t)k)
                               / (real_t)(size - 1u));
        y[k] = random_between(REAL_C(-4.0), REAL_C(4.0));

        gsl_vector_set(readings, k, (double)y[k]);

        double power = 1.0;

        for(uint32_t c = 0; c < count; c++)
        {
            gsl_matrix_set(model, k, c, power);
            power *= (double)x[k];
        }
    }

    if(!lstsq_polyfit(x, y, size, order, mine))
    {
        flag = false;
    }
    else
    {
        gsl_multifit_linear(model, readings, theirs, covariance, &left_over,
                            work);

        for(uint32_t c = 0; c < count; c++)
        {
            if(!CONFORMATION_IS_NEAR(mine[c], gsl_vector_get(theirs, c)))
            {
                flag = false;
                break;
            }
        }
    }

    free(x);
    free(y);
    free(mine);
    gsl_matrix_free(model);
    gsl_matrix_free(covariance);
    gsl_vector_free(readings);
    gsl_vector_free(theirs);
    gsl_multifit_linear_free(work);

    return flag;
}

void run_lstsq_conformation_tests(void)
{
    static const uint32_t orders[] = {1u, 2u, 3u, 4u, 5u};

    for(uint32_t k = 0; k < (sizeof(orders) / sizeof(orders[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(polyfit_check(32u, orders[k]),
                             "Least Squares Fit Test");
    }
}
