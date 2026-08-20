#ifndef TEST
#include <sptk/linalg/cnum.h>
#include <sptk/core/defs.h>
#else
#include "cnum.h"
#include "defs.h"
#endif

#include <math.h>

cnum_t cnum_make(real_t re, real_t im)
{
    cnum_t number;

    number.re = re;
    number.im = im;

    return number;
}

cnum_t cnum_from_real(real_t re)
{
    return cnum_make(re, REAL_C(0.0));
}

cnum_t cnum_zero(void)
{
    return cnum_make(REAL_C(0.0), REAL_C(0.0));
}

cnum_t cnum_one(void)
{
    return cnum_make(REAL_C(1.0), REAL_C(0.0));
}

cnum_t cnum_add(cnum_t a, cnum_t b)
{
    return cnum_make(a.re + b.re, a.im + b.im);
}

cnum_t cnum_subtract(cnum_t a, cnum_t b)
{
    return cnum_make(a.re - b.re, a.im - b.im);
}

cnum_t cnum_multiply(cnum_t a, cnum_t b)
{
    return cnum_make((a.re * b.re) - (a.im * b.im),
                     (a.re * b.im) + (a.im * b.re));
}

cnum_t cnum_divide(cnum_t a, cnum_t b)
{
    real_t divisor = cnum_magnitude_squared(b);

    if(divisor == REAL_C(0.0))
    {
        return cnum_zero();
    }

    return cnum_make((((a.re * b.re) + (a.im * b.im)) / divisor),
                     (((a.im * b.re) - (a.re * b.im)) / divisor));
}

cnum_t cnum_scale(cnum_t a, real_t factor)
{
    return cnum_make(a.re * factor, a.im * factor);
}

cnum_t cnum_conjugate(cnum_t a)
{
    return cnum_make(a.re, -a.im);
}

cnum_t cnum_negate(cnum_t a)
{
    return cnum_make(-a.re, -a.im);
}

real_t cnum_real(cnum_t a)
{
    return a.re;
}

real_t cnum_imaginary(cnum_t a)
{
    return a.im;
}

real_t cnum_magnitude(cnum_t a)
{
    return (real_t)REAL_SQRT(cnum_magnitude_squared(a));
}

real_t cnum_magnitude_squared(cnum_t a)
{
    return (a.re * a.re) + (a.im * a.im);
}

bool cnum_is_zero(cnum_t a)
{
    return (a.re == REAL_C(0.0)) && (a.im == REAL_C(0.0));
}

bool cnum_is_equal(cnum_t a, cnum_t b)
{
    return (a.re == b.re) && (a.im == b.im);
}

bool cnum_is_near(cnum_t a, cnum_t b, real_t tolerance)
{
    ASSERT(tolerance >= REAL_C(0.0));

    real_t real_difference = a.re - b.re;
    real_t imaginary_difference = a.im - b.im;

    if(real_difference < REAL_C(0.0))
    {
        real_difference = -real_difference;
    }
    if(imaginary_difference < REAL_C(0.0))
    {
        imaginary_difference = -imaginary_difference;
    }

    return (real_difference <= tolerance) && (imaginary_difference <= tolerance);
}
