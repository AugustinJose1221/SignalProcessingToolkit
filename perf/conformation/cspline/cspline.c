#include <perf/conformation/support.h>
#include <perf/conformation/cspline/cspline.h>

#include <math.h>
#include <stdlib.h>

#include <gsl/gsl_spline.h>

// The spline of this library set against the spline of the GNU Scientific
// Library.
//
// BOTH HOLD THE ENDS THE SAME WAY, and that was measured rather than assumed.
// A spline needs something said about its two ends, and there is more than one
// choice; the header of this module does not say which it made. Set against
// the natural spline of the other library, which holds the second derivative
// at nothing at both ends, the two agree to about a hundred millionth on six
// points of a wave. Were the ends held differently the two would part company
// near the ends and agree in the middle, which is the shape of that fault.
//
// The places are read BETWEEN the points, because at a point every spline
// gives the point back and nothing is being compared.

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static bool spline_check(uint32_t size)
{
    real_t* x = (real_t*)malloc(sizeof(real_t) * size);
    real_t* y = (real_t*)malloc(sizeof(real_t) * size);
    double* gx = (double*)malloc(sizeof(double) * size);
    double* gy = (double*)malloc(sizeof(double) * size);
    bool flag = true;

    // The places must rise, and no two of them may be the same, or neither
    // library has a curve to give.
    real_t place = REAL_C(0.0);

    for(uint32_t k = 0; k < size; k++)
    {
        place += random_between(REAL_C(0.5), REAL_C(2.0));

        x[k] = place;
        y[k] = random_between(REAL_C(-4.0), REAL_C(4.0));
        gx[k] = (double)x[k];
        gy[k] = (double)y[k];
    }

    cspline_t mine = cspline_alloc(size);
    cspline_mempool_t pool = cspline_alloc_mempool(size);
    cspline_init(&mine, pool, x, y);

    gsl_interp_accel* accelerator = gsl_interp_accel_alloc();
    gsl_spline* theirs = gsl_spline_alloc(gsl_interp_cspline, size);
    gsl_spline_init(theirs, gx, gy, size);

    // Nine places inside each interval, so that the curve is read where the
    // two libraries can differ and not only where they must agree.
    for(uint32_t k = 0; (k + 1u) < size; k++)
    {
        for(uint32_t step = 1; step < 10u; step++)
        {
            real_t between = x[k] + (((x[k + 1u] - x[k]) * (real_t)step)
                                     / REAL_C(10.0));

            real_t my_value = cspline_get_interpolated_point(&mine, between);
            double their_value = gsl_spline_eval(theirs, (double)between,
                                                 accelerator);

            if(!CONFORMATION_IS_NEAR(my_value, their_value))
            {
                flag = false;
                break;
            }
        }

        if(!flag)
        {
            break;
        }
    }

    gsl_spline_free(theirs);
    gsl_interp_accel_free(accelerator);
    cspline_free(mine);
    cspline_free_mempool(pool);
    free(x);
    free(y);
    free(gx);
    free(gy);

    return flag;
}

void run_cspline_conformation_tests(void)
{
    static const uint32_t sizes[] = {4u, 5u, 8u, 16u, 32u};

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(spline_check(sizes[k]), "Cubic Spline Test");
    }
}
