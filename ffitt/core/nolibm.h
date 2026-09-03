#ifndef NOLIBM_H
#define NOLIBM_H

#include <stdint.h>
#include <stdbool.h>

// THE ARITHMETIC THAT real.h NEEDS, WORKED OUT HERE INSTEAD OF BY THE SYSTEM.
//
// Every call this library makes into the mathematics of the system goes
// through one seam: the REAL_ macros of real.h. This file stands behind that
// seam when FFITT_NO_LIBM is defined, and then the library links with no
// mathematics library at all.
//
// WHO WANTS THIS. A target whose toolchain ships no libm; one whose libm is
// large enough to matter beside a flash of tens of kilobytes; one whose
// licence for it is awkward; and anyone who must be able to point at every
// line that went into the image.
//
// WHAT IT COSTS IN ACCURACY, AND THIS IS THE THING TO READ FIRST.
//
// These are not the system's functions and they do not pretend to be. Each is
// a reduction of the argument and then a series. MEASURED against the system's
// own functions over the ranges named, 200000 points each, as the worst
// RELATIVE error seen:
//
//   floor  ceil  fmod            exact
//   sqrt   hypot                 2.2e-16
//   log    log10  tan            2.3e-15
//   acosh                        3.5e-14
//   asinh                        1.7e-13
//   atan   atan2  asin           3.8e-12
//   exp    pow    cosh  sinh     1.2e-11
//   cos                          4.2e-10
//   sin                          4.2e-09
//   erf                          1.5e-07
//
// AT 32 BITS EVERY ONE OF THOSE IS BELOW WHAT A FLOAT HOLDS, which is about 7
// digits. The profile costs nothing that can be seen at that width.
//
// AT 64 BITS MOST OF THEM ARE STILL FAR BELOW WHAT A DOUBLE HOLDS, and two are
// not: sin and cos lose a little to the reduction of a large angle, and erf is
// held at what the approximation of Abramowitz and Stegun can do. A caller at
// 64 bits who leans on the last digits of those two should keep the system's
// library; everything else in the table is honest to eleven digits or better.
//
// WHY sin IS THE WORST OF THE TRIGONOMETRY. The angle is brought within a
// quarter turn by taking away a multiple of half of pi, and that multiple is
// worked out at the width in hand. For an angle of a hundred radians the
// taking away has already lost a few digits before the series begins. An angle
// that large is unusual in signal processing, where a phase is kept wrapped,
// and the measurement above runs to a hundred on purpose so that the number
// shown is the bad case and not the easy one.
//
// WHAT IS NOT HERE. Nothing rounds the way the system's functions round, no
// function raises a flag, and nothing here is written for speed. The library's
// own tests are run against these as well as against the system's, thus the
// table above is a tested number and not a hope.

// The square root, by the method of Newton from a guess made by halving the
// exponent. Gives a number that is not a number for a negative argument.
double nolibm_sqrt(double x);

// The length of the two sides taken together, the larger taken out first so
// that squaring cannot run past what the width holds.
double nolibm_hypot(double x, double y);

// The error function. A series near nothing, where the answer is small and a
// share of it must still be right, and the approximation of Abramowitz and
// Stegun beyond.
double nolibm_erf(double x);

// The sine. The angle is brought within a quarter turn of nothing first, thus
// a large angle costs a little accuracy in that reduction.
double nolibm_sin(double x);

// The cosine, by the same reduction as the sine.
double nolibm_cos(double x);

// The tangent, as the sine over the cosine. Gives an endless number where the
// cosine is nothing.
double nolibm_tan(double x);

// The size of a number, with any sign taken off.
double nolibm_fabs(double x);

// One number raised to another, as the exponential of the power times the
// logarithm. A negative base is answered only for a whole power, which is the
// only case with a real answer.
double nolibm_pow(double x, double y);

// The exponential. The argument is split into a whole number of twos and what
// is left, and only the small leftover reaches the series.
double nolibm_exp(double x);

// The natural logarithm. Powers of two are taken out first, and the series in
// (m-1)/(m+1) covers the little that is left.
double nolibm_log(double x);

// The logarithm to the base ten, as the natural one divided by the logarithm
// of ten.
double nolibm_log10(double x);

// The arc tangent. Anything above one is turned into its reciprocal and the
// range halved again, so that the series stays short and honest.
double nolibm_atan(double x);

// The angle of a point from the origin, with the quarter it lies in decided by
// the signs of both sides. Gives nothing for a point at the origin.
double nolibm_atan2(double y, double x);

// The hyperbolic sine. A series near nothing, where taking one nearly equal
// number from another would lose every digit, and the exponentials beyond.
double nolibm_sinh(double x);

// The hyperbolic cosine, from the exponential.
double nolibm_cosh(double x);

// The arc sine, by way of the arc tangent. Gives a number that is not a number
// outside minus one to one.
double nolibm_asin(double x);

// The inverse hyperbolic sine, by way of the logarithm.
double nolibm_asinh(double x);

// The inverse hyperbolic cosine. Gives a number that is not a number below
// one.
double nolibm_acosh(double x);

// The largest whole number at or below the argument.
double nolibm_floor(double x);

// The smallest whole number at or above the argument.
double nolibm_ceil(double x);

// What is left when the divisor is taken away as often as it fits. Worked out
// by doubling and halving and never by dividing, thus it is exact.
double nolibm_fmod(double x, double y);


#endif//NOLIBM_H
