#ifndef REAL_H
#define REAL_H

#include <float.h>

// The one type that every number of this library is held in.
//
// The library holds every sample, every coefficient and every result in
// real_t. Nothing anywhere spells float or double directly. That is the whole
// point of this header: the width of a number is decided ONE time, for the
// whole build, and never module by module.
//
// HOW TO CHOOSE THE WIDTH
//
// Build with SPTK_REAL_64 defined for 64 bits, and with nothing defined, or
// with SPTK_REAL_32, for 32 bits. 32 bits is the default.
//
//     cc -DSPTK_REAL_64 ...        a double, about 16 digits
//     cc ...                       a float, about 7 digits
//
// WHICH TO USE
//
// Take 32 bits when the work runs on a small processor. A float is half the
// memory, and a processor with a unit for 32 bit arithmetic and none for 64
// bit will run the 64 bit build tens of times more slowly, because every
// operation becomes a call to a library that does it in software.
//
// Take 64 bits when the numbers are large, when the filters are slow, or when
// the answer matters more than the time. A float holds about seven digits, and
// three kinds of work run out of them:
//
//   A LARGE OFFSET. A reading that sits at 8 000 000 counts with a signal of a
//   few thousand on top spends six of the seven digits on the part that
//   carries nothing.
//
//   A LONG SUM. Adding a thousand samples that each sit near eight million
//   gives a total near eight thousand million, where one step of a float is
//   512. The low digits of every later sample fall away.
//
//   A SLOW FILTER. A section holds its poles near the circle when the cutoff
//   is low, and it lifts whatever error reaches it by a large factor.
//   IIR_MIN_CUTOFF holds the lowest cutoff that 32 bits can carry.
//
// The guides of each area give measured numbers for all three.
//
// WHAT THIS HEADER GIVES
//
//   real_t        the type
//   REAL_C(x)     a number written in the source, for example REAL_C(0.5)
//   REAL_SQRT     and the other functions of mathematics
//   REAL_EPSILON  the smallest step that the type can hold beside 1
//   REAL_DIGITS   how many digits of ten the type holds
//
// WHY A NUMBER IN THE SOURCE NEEDS REAL_C
//
// A number written as 0.5 is a double, and a number written as 0.5f is a
// float. Writing 0.5 in a 32 bit build does not fail; it quietly makes the
// arithmetic around it run in 64 bits and then throws the extra away. Measured
// on one line, that turned three instructions into six and made the work run
// in double where it should have run in float.
//
// Writing 0.5f in a 64 bit build is worse: the number is rounded to 7 digits
// before the 64 bit arithmetic ever sees it, thus the build says it holds 16
// digits and does not.
//
// REAL_C writes the right one for the build. Use it for EVERY number in the
// source that is not a whole number used as a count.

#if defined(SPTK_REAL_64)

// A build in 64 bits must have a double that is really wider than a float.
// On some small targets the two are the same type, and there the 64 bit build
// would cost the memory and give none of the accuracy. Better to stop than to
// promise something the target cannot give.
#if DBL_MANT_DIG <= FLT_MANT_DIG
#error "SPTK_REAL_64 was asked for, but on this target a double is no wider than a float."
#endif

typedef double real_t;

#define REAL_C(x)       (x)

#define REAL_EPSILON    DBL_EPSILON
#define REAL_DIGITS     DBL_DIG
#define REAL_LARGEST    DBL_MAX
#define REAL_SMALLEST   DBL_MIN

#define REAL_SQRT(x)        sqrt(x)
#define REAL_SIN(x)         sin(x)
#define REAL_COS(x)         cos(x)
#define REAL_TAN(x)         tan(x)
#define REAL_ABS(x)         fabs(x)
#define REAL_POW(x, y)      pow((x), (y))
#define REAL_EXP(x)         exp(x)
#define REAL_LOG(x)         log(x)
#define REAL_LOG10(x)       log10(x)
#define REAL_ATAN2(y, x)    atan2((y), (x))
#define REAL_SINH(x)        sinh(x)
#define REAL_COSH(x)        cosh(x)
#define REAL_ASIN(x)        asin(x)
#define REAL_ASINH(x)       asinh(x)
#define REAL_ACOSH(x)       acosh(x)
#define REAL_FLOOR(x)       floor(x)
#define REAL_CEIL(x)        ceil(x)
#define REAL_FMOD(x, y)     fmod((x), (y))

#else

typedef float real_t;

#define REAL_C(x)       (x##f)

#define REAL_EPSILON    FLT_EPSILON
#define REAL_DIGITS     FLT_DIG
#define REAL_LARGEST    FLT_MAX
#define REAL_SMALLEST   FLT_MIN

#define REAL_SQRT(x)        sqrtf(x)
#define REAL_SIN(x)         sinf(x)
#define REAL_COS(x)         cosf(x)
#define REAL_TAN(x)         tanf(x)
#define REAL_ABS(x)         fabsf(x)
#define REAL_POW(x, y)      powf((x), (y))
#define REAL_EXP(x)         expf(x)
#define REAL_LOG(x)         logf(x)
#define REAL_LOG10(x)       log10f(x)
#define REAL_ATAN2(y, x)    atan2f((y), (x))
#define REAL_SINH(x)        sinhf(x)
#define REAL_COSH(x)        coshf(x)
#define REAL_ASIN(x)        asinf(x)
#define REAL_ASINH(x)       asinhf(x)
#define REAL_ACOSH(x)       acoshf(x)
#define REAL_FLOOR(x)       floorf(x)
#define REAL_CEIL(x)        ceilf(x)
#define REAL_FMOD(x, y)     fmodf((x), (y))

#endif//SPTK_REAL_64

// The number pi, at the width of the build.
#define REAL_PI         REAL_C(3.14159265358979323846)

// The functions of mathematics again, as functions and not as macros.
//
// The macros above cost nothing, because the compiler puts the right call in
// where they stand. But a macro has no address, thus none of them can be
// GIVEN to something that takes a function.
//
// The pmatrix module holds a function for each of its elements, and before
// real_t existed a caller could give it sinf directly. That no longer works
// and, worse, it does not fail to build: sinf takes a float, the module calls
// it through a pointer that takes a real_t, and in a 64 bit build the two do
// not agree. The answer is then not wrong by a little but nonsense.
//
// These give a name with an address that always agrees with real_t. Use the
// macros in ordinary code and these only where a function must be handed over.
// The sine of x, where x is an angle in radians.
real_t real_sin(real_t x);

// The cosine of x, where x is an angle in radians.
real_t real_cos(real_t x);

// The tangent of x, where x is an angle in radians.
real_t real_tan(real_t x);

// The square root of x.
real_t real_sqrt(real_t x);

// The number e raised to the power x.
real_t real_exp(real_t x);

// The logarithm of x to the base e.
real_t real_log(real_t x);

// The size of x, without its sign.
real_t real_abs(real_t x);

#endif//REAL_H
