#include <perf/conformation/support.h>
#include <perf/conformation/poly/poly.h>

#include <ffitt/linalg/cnum.h>

#include <math.h>
#include <stdlib.h>

#include <gsl/gsl_poly.h>

// Where a polynomial crosses nothing, set against the answer of the GNU
// Scientific Library.
//
// BOTH LIBRARIES COUNT THE COEFFICIENTS THE SAME WAY, and that was measured
// rather than assumed. For the numbers 1, 2, 3 this library gives 17 at x of
// 2, which is 1 + 2x + 3x squared, thus the first number is the constant and
// the other library reads it the same way.
//
// NEITHER PROMISES AN ORDER for the roots, thus both sets are sorted before
// they are read: by the real part first, and by the imaginary part where the
// real parts are the same.
//
// The order is held to what this library will take. POLY_LARGEST_ROOT_ORDER is
// 4 when every number is a float and 12 when it is a double, because finding a
// root of a high order needs digits that a float has not got.

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static int compare_roots(const void* left, const void* right)
{
    const cnum_t* a = (const cnum_t*)left;
    const cnum_t* b = (const cnum_t*)right;

    real_t a_re = cnum_real(*a);
    real_t b_re = cnum_real(*b);

    if(!CONFORMATION_IS_NEAR(a_re, b_re))
    {
        return (a_re < b_re) ? -1 : 1;
    }

    real_t a_im = cnum_imaginary(*a);
    real_t b_im = cnum_imaginary(*b);

    if(a_im < b_im)
    {
        return -1;
    }

    return (a_im > b_im) ? 1 : 0;
}

static bool roots_check(uint32_t order)
{
    uint32_t count = POLY_COEFFICIENT_COUNT(order);
    real_t* coefficient = (real_t*)malloc(sizeof(real_t) * count);
    double* theirs = (double*)malloc(sizeof(double) * count);
    cnum_t* my_roots = (cnum_t*)malloc(sizeof(cnum_t) * order);
    cnum_t* their_roots = (cnum_t*)malloc(sizeof(cnum_t) * order);
    double* packed = (double*)malloc(sizeof(double) * 2u * order);
    gsl_poly_complex_workspace* work = gsl_poly_complex_workspace_alloc(count);
    bool flag = true;

    for(uint32_t k = 0; k < count; k++)
    {
        real_t value = random_between(REAL_C(-4.0), REAL_C(4.0));

        // The highest coefficient must not be nothing, or the polynomial is
        // really of a lower order and neither library is being asked the
        // question that was meant.
        if((k == (count - 1u)) && (REAL_ABS(value) < REAL_C(0.5)))
        {
            value = REAL_C(1.0);
        }

        coefficient[k] = value;
        theirs[k] = (double)value;
    }

    if(!poly_roots(coefficient, order, my_roots))
    {
        flag = false;
    }
    else if(gsl_poly_complex_solve(theirs, count, work, packed) != 0)
    {
        // The other library could not find them either, thus there is nothing
        // to compare and nothing to report.
        flag = true;
    }
    else
    {
        for(uint32_t k = 0; k < order; k++)
        {
            their_roots[k] = cnum_make((real_t)packed[2u * k],
                                       (real_t)packed[(2u * k) + 1u]);
        }

        qsort(my_roots, order, sizeof(cnum_t), compare_roots);
        qsort(their_roots, order, sizeof(cnum_t), compare_roots);

        for(uint32_t k = 0; k < order; k++)
        {
            if(!CONFORMATION_IS_NEAR(cnum_real(my_roots[k]),
                                     cnum_real(their_roots[k]))
               || !CONFORMATION_IS_NEAR(cnum_imaginary(my_roots[k]),
                                        cnum_imaginary(their_roots[k])))
            {
                flag = false;
                break;
            }
        }
    }

    free(coefficient);
    free(theirs);
    free(my_roots);
    free(their_roots);
    free(packed);
    gsl_poly_complex_workspace_free(work);

    return flag;
}

void run_poly_conformation_tests(void)
{
    for(uint32_t order = 2u; order <= POLY_LARGEST_ROOT_ORDER; order++)
    {
        FLAG_CHECK_TRUE_CASE(roots_check(order), "Poly Roots Test");
    }
}
