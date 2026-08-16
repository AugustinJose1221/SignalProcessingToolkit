#ifndef CNUM_H
#define CNUM_H

#include <stdbool.h>

// A complex number.
//
// The C standard gives <complex.h> and the type float _Complex. This library
// does not use them for two reasons. Many small compilers do not give them.
// And <complex.h> gives the macro `complex`, thus a module of this library
// could not carry that name. A structure with two float members works on every
// compiler, and it holds the values in the same way as the rest of the
// library.

typedef struct{
    float re;                   // The real part
    float im;                   // The imaginary part
}cnum_t;

cnum_t cnum_make(float re, float im);
cnum_t cnum_from_real(float re);
cnum_t cnum_zero(void);
cnum_t cnum_one(void);

cnum_t cnum_add(cnum_t a, cnum_t b);
cnum_t cnum_subtract(cnum_t a, cnum_t b);
cnum_t cnum_multiply(cnum_t a, cnum_t b);

// Give the quotient of the two numbers. If the second number is zero, the
// quotient has no value, and the function gives zero.
cnum_t cnum_divide(cnum_t a, cnum_t b);

cnum_t cnum_scale(cnum_t a, float factor);
cnum_t cnum_conjugate(cnum_t a);
cnum_t cnum_negate(cnum_t a);

float cnum_real(cnum_t a);
float cnum_imaginary(cnum_t a);

// Give the distance of the number from zero.
float cnum_magnitude(cnum_t a);

// Give the square of the distance from zero. This function does not take a
// square root, thus it is faster than cnum_magnitude and it keeps more of the
// accuracy. Use it when you only compare two distances.
float cnum_magnitude_squared(cnum_t a);

bool cnum_is_zero(cnum_t a);
bool cnum_is_equal(cnum_t a, cnum_t b);

// Give true if the two numbers differ by less than the tolerance. A float
// keeps about 7 digits, thus a calculation with several steps gives a result
// that is near the correct value but not equal to it.
bool cnum_is_near(cnum_t a, cnum_t b, float tolerance);

#endif//CNUM_H
