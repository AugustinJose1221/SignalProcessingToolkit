#ifndef TEST
#include <sptk/linalg/cnum.h>
#include <sptk/core/defs.h>
#else
#include "cnum.h"
#include "defs.h"
#endif

#include <math.h>

cnum_t cnum_make(float re, float im)
{
    cnum_t number;

    number.re = re;
    number.im = im;

    return number;
}

cnum_t cnum_from_real(float re)
{
    return cnum_make(re, 0.0f);
}

cnum_t cnum_zero(void)
{
    return cnum_make(0.0f, 0.0f);
}

cnum_t cnum_one(void)
{
    return cnum_make(1.0f, 0.0f);
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
    float divisor = cnum_magnitude_squared(b);

    if(divisor == 0.0f)
    {
        return cnum_zero();
    }

    return cnum_make((((a.re * b.re) + (a.im * b.im)) / divisor),
                     (((a.im * b.re) - (a.re * b.im)) / divisor));
}

cnum_t cnum_scale(cnum_t a, float factor)
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

float cnum_real(cnum_t a)
{
    return a.re;
}

float cnum_imaginary(cnum_t a)
{
    return a.im;
}

float cnum_magnitude(cnum_t a)
{
    return (float)sqrt(cnum_magnitude_squared(a));
}

float cnum_magnitude_squared(cnum_t a)
{
    return (a.re * a.re) + (a.im * a.im);
}

bool cnum_is_zero(cnum_t a)
{
    return (a.re == 0.0f) && (a.im == 0.0f);
}

bool cnum_is_equal(cnum_t a, cnum_t b)
{
    return (a.re == b.re) && (a.im == b.im);
}

bool cnum_is_near(cnum_t a, cnum_t b, float tolerance)
{
    ASSERT(tolerance >= 0.0f);

    float real_difference = a.re - b.re;
    float imaginary_difference = a.im - b.im;

    if(real_difference < 0.0f)
    {
        real_difference = -real_difference;
    }
    if(imaginary_difference < 0.0f)
    {
        imaginary_difference = -imaginary_difference;
    }

    return (real_difference <= tolerance) && (imaginary_difference <= tolerance);
}
