#ifndef TEST
#include <ffitt/core/nolibm.h>
#else
#include "nolibm.h"
#endif

// EVERYTHING HERE IS WORKED OUT FROM ARITHMETIC AND NOTHING ELSE.
//
// No header of the system's mathematics is included, on purpose: including one
// would let a call slip through to it and the whole point would be lost
// quietly. The only things taken from outside are the sizes of the number
// types, which are arithmetic and not mathematics.
#include <float.h>

#define NOLIBM_PI           3.14159265358979323846
#define NOLIBM_HALF_PI      1.57079632679489661923
#define NOLIBM_TWO_PI       6.28318530717958647693
#define NOLIBM_LN2          0.69314718055994530942
#define NOLIBM_LOG2_E       1.44269504088896340736
#define NOLIBM_LN10         2.30258509299404568402

// A number that is not a number, and one that is endless, made without a
// header. Dividing nothing by nothing gives the first and one by nothing the
// second, and both are what the system's functions give in the same places.
static double nolibm_not_a_number(void)
{
    double nothing = 0.0;

    return nothing / nothing;
}

static double nolibm_endless(void)
{
    double nothing = 0.0;

    return 1.0 / nothing;
}

static bool nolibm_is_not_a_number(double x)
{
    // A number that is not a number is the only one unequal to itself.
    return x != x;
}

double nolibm_fabs(double x)
{
    return (x < 0.0) ? -x : x;
}

double nolibm_floor(double x)
{
    // A number too large to hold a fraction is already whole.
    if(nolibm_fabs(x) >= 4503599627370496.0)
    {
        return x;
    }

    double whole = (double)(int64_t)x;

    return ((x < 0.0) && (whole != x)) ? (whole - 1.0) : whole;
}

double nolibm_ceil(double x)
{
    if(nolibm_fabs(x) >= 4503599627370496.0)
    {
        return x;
    }

    double whole = (double)(int64_t)x;

    return ((x > 0.0) && (whole != x)) ? (whole + 1.0) : whole;
}

// The remainder, by taking the divisor away and never by dividing.
//
// WHY NOT x - trunc(x/y)*y, WHICH IS THE OBVIOUS WAY AND IS WRONG. The
// division rounds, and when it rounds across a whole number the answer is out
// by a whole divisor. MEASURED: fmod(-87.6, 7.3) gave 0 where the truth is
// -7.3, because -87.6/7.3 rounded to exactly -12 while the true quotient is a
// shade under it.
//
// Doubling and halving a number is exact in this arithmetic, and so is taking
// away a number that is close to it. Thus the divisor is doubled until it
// stands above what is left, and then halved back down, taking it away
// wherever it fits. Nothing rounds anywhere in that, and the answer is the one
// the system's own function gives.
double nolibm_fmod(double x, double y)
{
    if((y == 0.0) || nolibm_is_not_a_number(x) || nolibm_is_not_a_number(y))
    {
        return nolibm_not_a_number();
    }

    double left = nolibm_fabs(x);
    double divisor = nolibm_fabs(y);

    if(left < divisor)
    {
        return x;
    }

    if(left > DBL_MAX)
    {
        return nolibm_not_a_number();
    }

    // Up, until the divisor stands above what is left.
    double raised = divisor;
    uint32_t doublings = 0;

    while((raised <= left) && (doublings < 2048u))
    {
        raised *= 2.0;
        doublings++;
    }

    // And down again, taking it away wherever it fits.
    while(doublings > 0u)
    {
        raised *= 0.5;
        doublings--;

        if(raised <= left)
        {
            left -= raised;
        }
    }

    return (x < 0.0) ? -left : left;
}

// The square root, by the method of Newton.
//
// The guess is made by halving the exponent, which needs the bits of the
// number rather than its value: a number is a power of two times something
// between 1 and 2, thus halving the exponent lands within a factor of the root
// and four turns of Newton close the rest. Starting from x itself would need
// far more turns for a large x and would lose digits for a small one.
double nolibm_sqrt(double x)
{
    if(nolibm_is_not_a_number(x) || (x < 0.0))
    {
        return nolibm_not_a_number();
    }

    if((x == 0.0) || (x > DBL_MAX))
    {
        return x;
    }

    // The exponent, taken by halving or doubling until the number sits between
    // 1 and 4, which needs no bit work and no header.
    int32_t twos = 0;

    while(x >= 4.0)
    {
        x /= 4.0;
        twos++;
    }

    while(x < 1.0)
    {
        x *= 4.0;
        twos--;
    }

    // Between 1 and 4 the root runs 1 to 2, and this line is within a twentieth
    // of it everywhere in that range.
    double root = 0.5 + (0.5 * x);

    for(uint32_t turn = 0; turn < 5u; turn++)
    {
        root = 0.5 * (root + (x / root));
    }

    while(twos > 0)
    {
        root *= 2.0;
        twos--;
    }

    while(twos < 0)
    {
        root *= 0.5;
        twos++;
    }

    return root;
}

double nolibm_hypot(double x, double y)
{
    x = nolibm_fabs(x);
    y = nolibm_fabs(y);

    // The larger is taken out first, so that squaring cannot run past what the
    // width holds when both are large, nor down to nothing when both are small.
    double larger = (x > y) ? x : y;
    double smaller = (x > y) ? y : x;

    if(larger == 0.0)
    {
        return 0.0;
    }

    double part = smaller / larger;

    return larger * nolibm_sqrt(1.0 + (part * part));
}

// The exponential.
//
// The argument is split into a whole number of twos and what is left, which
// runs between minus half a log of two and plus half. The polynomial then has
// only that small range to cover, and the twos are put back by doubling.
double nolibm_exp(double x)
{
    if(nolibm_is_not_a_number(x))
    {
        return x;
    }

    if(x > 709.0)
    {
        return nolibm_endless();
    }

    if(x < -745.0)
    {
        return 0.0;
    }

    double twos = nolibm_floor((x * NOLIBM_LOG2_E) + 0.5);
    double left = x - (twos * NOLIBM_LN2);

    // The series of the exponential, to the seventh power. Over the range that
    // is left, which is about a third either way, the eighth term is already
    // below what a float holds.
    double term = 1.0;
    double total = 1.0;

    for(uint32_t power = 1u; power <= 9u; power++)
    {
        term *= left / (double)power;
        total += term;
    }

    // The twos put back, by doubling or halving that many times. A loop rather
    // than a power, because a power would call this function.
    int32_t count = (int32_t)twos;

    while(count > 0)
    {
        total *= 2.0;
        count--;
    }

    while(count < 0)
    {
        total *= 0.5;
        count++;
    }

    return total;
}

// The natural logarithm.
//
// The number is brought between the root of a half and the root of two by
// taking powers of two out of it, and the series in (m-1)/(m+1) covers what is
// left. That series converges fast over so small a range, where the plain
// series in (m-1) would not.
double nolibm_log(double x)
{
    if(nolibm_is_not_a_number(x))
    {
        return x;
    }

    if(x < 0.0)
    {
        return nolibm_not_a_number();
    }

    if(x == 0.0)
    {
        return -nolibm_endless();
    }

    if(x > DBL_MAX)
    {
        return x;
    }

    int32_t twos = 0;

    while(x > 1.4142135623730951)
    {
        x *= 0.5;
        twos++;
    }

    while(x < 0.7071067811865476)
    {
        x *= 2.0;
        twos--;
    }

    double part = (x - 1.0) / (x + 1.0);
    double squared = part * part;
    double term = part;
    double total = part;

    for(uint32_t power = 3u; power <= 15u; power += 2u)
    {
        term *= squared;
        total += term / (double)power;
    }

    return (2.0 * total) + ((double)twos * NOLIBM_LN2);
}

double nolibm_log10(double x)
{
    return nolibm_log(x) / NOLIBM_LN10;
}

double nolibm_pow(double x, double y)
{
    if(y == 0.0)
    {
        return 1.0;
    }

    if(x == 0.0)
    {
        return (y > 0.0) ? 0.0 : nolibm_endless();
    }

    // A negative base is answered only for a whole power, which is the only
    // case with a real answer, and it is the only case this library asks for.
    if(x < 0.0)
    {
        double whole = nolibm_floor(y);

        if(whole != y)
        {
            return nolibm_not_a_number();
        }

        double size = nolibm_exp(y * nolibm_log(-x));
        double half = whole * 0.5;

        return (nolibm_floor(half) == half) ? size : -size;
    }

    return nolibm_exp(y * nolibm_log(x));
}

// The sine and the cosine.
//
// The angle is brought within a quarter turn of nothing, which is what keeps
// the polynomial short: over that range seven terms of the series are already
// below what a float holds, and over a whole turn they would not be.
static double nolibm_sine_core(double x)
{
    double squared = x * x;
    double term = x;
    double total = x;

    for(uint32_t power = 3u; power <= 15u; power += 2u)
    {
        term *= -squared / (double)(power * (power - 1u));
        total += term;
    }

    return total;
}

static double nolibm_cosine_core(double x)
{
    double squared = x * x;
    double term = 1.0;
    double total = 1.0;

    for(uint32_t power = 2u; power <= 14u; power += 2u)
    {
        term *= -squared / (double)(power * (power - 1u));
        total += term;
    }

    return total;
}

// Which quarter of the turn the angle lies in, and what is left over inside
// it. The sine and the cosine of the whole are then the core of the leftover,
// with the sign and the swap that the quarter asks for.
static int32_t nolibm_quarter(double x, double* left)
{
    double turns = nolibm_floor((x / NOLIBM_HALF_PI) + 0.5);

    *left = x - (turns * NOLIBM_HALF_PI);

    int32_t quarter = (int32_t)nolibm_fmod(turns, 4.0);

    return (quarter < 0) ? (quarter + 4) : quarter;
}

double nolibm_sin(double x)
{
    if(nolibm_is_not_a_number(x) || (nolibm_fabs(x) > DBL_MAX))
    {
        return nolibm_not_a_number();
    }

    double left;
    int32_t quarter = nolibm_quarter(x, &left);

    switch(quarter)
    {
        case 0:  return nolibm_sine_core(left);
        case 1:  return nolibm_cosine_core(left);
        case 2:  return -nolibm_sine_core(left);
        default: return -nolibm_cosine_core(left);
    }
}

double nolibm_cos(double x)
{
    if(nolibm_is_not_a_number(x) || (nolibm_fabs(x) > DBL_MAX))
    {
        return nolibm_not_a_number();
    }

    double left;
    int32_t quarter = nolibm_quarter(x, &left);

    switch(quarter)
    {
        case 0:  return nolibm_cosine_core(left);
        case 1:  return -nolibm_sine_core(left);
        case 2:  return -nolibm_cosine_core(left);
        default: return nolibm_sine_core(left);
    }
}

double nolibm_tan(double x)
{
    double cosine = nolibm_cos(x);

    if(cosine == 0.0)
    {
        return nolibm_endless();
    }

    return nolibm_sin(x) / cosine;
}

// The arc tangent.
//
// Anything above one is turned into its reciprocal, which lands the argument
// between nothing and one, and the answer is taken off a quarter turn. The
// series is then short enough to be honest, which over the whole line it would
// not be.
double nolibm_atan(double x)
{
    if(nolibm_is_not_a_number(x))
    {
        return x;
    }

    bool negative = (x < 0.0);
    double size = nolibm_fabs(x);
    bool turned = false;

    if(size > 1.0)
    {
        size = 1.0 / size;
        turned = true;
    }

    // Even between nothing and one the plain series crawls near one, thus the
    // range is halved again by the identity of the half angle.
    bool halved = false;

    if(size > 0.4142135623730951)
    {
        size = (size - 0.4142135623730951) / (1.0 + (0.4142135623730951 * size));
        halved = true;
    }

    double squared = size * size;
    double term = size;
    double total = size;

    for(uint32_t power = 3u; power <= 25u; power += 2u)
    {
        term *= -squared;
        total += term / (double)power;
    }

    if(halved)
    {
        total += NOLIBM_PI / 8.0;
    }

    if(turned)
    {
        total = NOLIBM_HALF_PI - total;
    }

    return negative ? -total : total;
}

double nolibm_atan2(double y, double x)
{
    if(x > 0.0)
    {
        return nolibm_atan(y / x);
    }

    if(x < 0.0)
    {
        double angle = nolibm_atan(y / x);

        return (y >= 0.0) ? (angle + NOLIBM_PI) : (angle - NOLIBM_PI);
    }

    // Straight up, straight down, or nowhere at all.
    if(y > 0.0)
    {
        return NOLIBM_HALF_PI;
    }

    if(y < 0.0)
    {
        return -NOLIBM_HALF_PI;
    }

    return 0.0;
}

double nolibm_asin(double x)
{
    double size = nolibm_fabs(x);

    if(size > 1.0)
    {
        return nolibm_not_a_number();
    }

    if(size == 1.0)
    {
        return (x < 0.0) ? -NOLIBM_HALF_PI : NOLIBM_HALF_PI;
    }

    return nolibm_atan(x / nolibm_sqrt(1.0 - (x * x)));
}

double nolibm_sinh(double x)
{
    // For a small argument the difference of two nearly equal numbers loses
    // every digit, thus the series is used there instead.
    if(nolibm_fabs(x) < 0.5)
    {
        double squared = x * x;
        double term = x;
        double total = x;

        for(uint32_t power = 3u; power <= 13u; power += 2u)
        {
            term *= squared / (double)(power * (power - 1u));
            total += term;
        }

        return total;
    }

    double up = nolibm_exp(x);

    return 0.5 * (up - (1.0 / up));
}

double nolibm_cosh(double x)
{
    double up = nolibm_exp(nolibm_fabs(x));

    return 0.5 * (up + (1.0 / up));
}

double nolibm_asinh(double x)
{
    double size = nolibm_fabs(x);
    double answer = nolibm_log(size + nolibm_sqrt((size * size) + 1.0));

    return (x < 0.0) ? -answer : answer;
}

double nolibm_acosh(double x)
{
    if(x < 1.0)
    {
        return nolibm_not_a_number();
    }

    return nolibm_log(x + nolibm_sqrt((x * x) - 1.0));
}

// The error function, by the approximation of Abramowitz and Stegun, 7.1.26.
//
// It is right to about one part in ten million everywhere, which is what a
// float holds and is the accuracy the rest of this file aims at.
double nolibm_erf(double x)
{
    static const double A1 = 0.254829592;
    static const double A2 = -0.284496736;
    static const double A3 = 1.421413741;
    static const double A4 = -1.453152027;
    static const double A5 = 1.061405429;
    static const double P = 0.3275911;

    if(nolibm_is_not_a_number(x))
    {
        return x;
    }

    double sign = (x < 0.0) ? -1.0 : 1.0;
    double size = nolibm_fabs(x);

    if(size > 6.0)
    {
        return sign;
    }

    // NEAR NOTHING THE APPROXIMATION BELOW IS NO GOOD, and the reason is worth
    // saying. It is right to about 1.5 parts in ten million of the ANSWER'S
    // OWN SCALE, which is fine where the answer is near one. Near zero the
    // answer is about x itself, thus that same error is the whole of a small
    // answer: at x of a twentieth it was measured out by three parts in a
    // thousand. The series has no such trouble there and converges fast.
    if(size <= 1.0)
    {
        double squared = x * x;
        double term = x;
        double total = x;

        for(uint32_t power = 1u; power <= 24u; power++)
        {
            term *= -squared / (double)power;
            total += term / (double)((2u * power) + 1u);
        }

        return 1.1283791670955125739 * total;
    }

    double t = 1.0 / (1.0 + (P * size));
    double poly = t * (A1 + (t * (A2 + (t * (A3 + (t * (A4 + (t * A5))))))));

    return sign * (1.0 - (poly * nolibm_exp(-size * size)));
}
