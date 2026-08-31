#ifndef POLY_H
#define POLY_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/linalg/matrix.h>
#else
#include "real.h"
#include "cnum.h"
#include "matrix.h"
#endif

// Polynomials, and where they cross nothing.
//
// A polynomial is a list of numbers, lowest power first: the list 2, 3, -1
// means 2 plus 3x less x squared. That is the same order the lstsq module
// gives its answers in, and the same order this module reads and writes.
//
// WHAT THE ROOTS ARE FOR
//
// THE POLES AND ZEROS OF A FILTER. An iir filter is two polynomials divided by
// each other. Where the one below crosses nothing, the filter has a pole; and
// A POLE OUTSIDE THE CIRCLE IS A FILTER THAT RUNS AWAY. That is not a slow
// drift: the answer doubles every few samples until it is nothing but
// infinities. poly_is_inside_circle is how to find out before it happens.
//
// A filter designed by the iir module is stable by construction. One whose
// coefficients were worked out elsewhere, read from a file, or made by
// changing a design by hand, is stable only if somebody checked.
//
// HOW THE ROOTS ARE FOUND
//
// By the eigen module. The roots of a polynomial are exactly the eigenvalues
// of one particular matrix built from its coefficients, and that turns a hard
// problem into one the library already solves.
//
// THE ONE PLACE THAT COSTS SOMETHING. The eigen module takes symmetric
// matrices only, and the matrix built from a polynomial is not symmetric. Its
// roots may be complex, and they come in pairs.
//
// This module therefore does the work itself rather than through the eigen
// module.
//
// WHY THE ORDER IS CAPPED, AND IT IS NOT THE METHOD THAT CAPS IT
//
// Measured, on polynomials built by multiplying known roots together. Two
// numbers are taken: how far each root came back from the one it was built
// from, and how near nothing the polynomial really is at the root that came
// back.
//
//     order              2         3         4         5         6
//     32 bits
//       from intended  3.5e-05   6.0e-06   1.4e-05   4.3e-02   2.9e-01
//       p(root)        6.0e-08   7.5e-08   1.8e-07   1.8e-07   3.7e-07
//     64 bits
//       from intended  0.0       0.0       0.0       0.0       0.0
//       p(root)        5.6e-17   3.6e-16   3.6e-16   5.5e-16   7.0e-16
//
// READ THE TWO 32 BIT ROWS AGAINST EACH OTHER. At order 5 the roots come back
// a twentieth away from the ones they were built from, AND THE POLYNOMIAL IS
// STILL NEARLY NOTHING THERE. Both are true at once, and what it means is
// that the module found the right roots OF THE WRONG POLYNOMIAL.
//
// THE ROOTS OF A POLYNOMIAL ARE EXTREMELY SENSITIVE TO ITS COEFFICIENTS. By
// order 5 the coefficients themselves, held at 32 bits, no longer describe the
// polynomial that was meant: the rounding of the last digit of each moves the
// roots by a twentieth. No method finds roots that the coefficients no longer
// hold.
//
// That is why the cap follows the width, and why it is low at 32 bits. It is
// not a shortcoming of the walking below; it is a limit of what a list of 32
// bit numbers can say about where a curve crosses nothing.
//
// FOR A FILTER THIS IS RARELY A LIMIT. An iir filter is a chain of biquads and
// each is order 2, which is reached by a closed form and is exact. Ask about
// one section at a time: it is both accurate and what the caller usually wants
// to know anyway.
//
// HOW THE ROOTS ARE WALKED TO
//
//   ORDER 1 AND 2 have a closed form and it is used, thus every pole of every
//   filter in this library is reached exactly and with no walking at all.
//   ABOVE THAT one root is found at a time by the step of Newton and divided
//   out, and then EVERY ROOT IS POLISHED against the original polynomial. The
//   polishing is what takes back the error that each division carried into
//   what followed, and without it the answer at order 4 is out by a sixth
//   rather than by a part in fifty thousand.
//
// The highest order whose roots this module will find.
//
// Above this the dividing out has spent too many digits for the answer to be
// worth having, and the module says so rather than giving back roots that look
// like roots.
#ifndef POLY_LARGEST_ROOT_ORDER
#if defined(FFITT_REAL_64)
#define POLY_LARGEST_ROOT_ORDER     12u
#else
#define POLY_LARGEST_ROOT_ORDER     4u
#endif
#endif

// How many numbers a polynomial of the given order holds, which is one more
// than the order.
#define POLY_COEFFICIENT_COUNT(order)   ((order) + 1u)

// True if the order is one whose roots this module will find.
bool poly_is_valid_order(uint32_t order);

// Give the value of a polynomial at one place.
//
// The work is done from the highest power inwards, which needs one
// multiplication and one addition for each order and never forms a power on
// its own: forming x to the ninth directly loses digits that this way keeps.
real_t poly_evaluate(const real_t* coefficient, uint32_t order, real_t x);

// Give the value of a polynomial at one complex place.
//
// A root of a polynomial with real coefficients may be complex, thus checking
// one needs this rather than the plain form above.
cnum_t poly_evaluate_complex(const real_t* coefficient, uint32_t order,
                             cnum_t x);

// Multiply two polynomials together.
//
// The answer holds POLY_COEFFICIENT_COUNT(first order plus second order)
// numbers and room says how many it can hold.
//
// THIS IS HOW A FILTER IS BUILT FROM ITS SECTIONS. Multiplying the denominators
// of two biquads gives the denominator of the two in a chain, and its roots are
// the poles of the pair.
//
// Give false if the room is too small.
bool poly_multiply(const real_t* first, uint32_t first_order,
                   const real_t* second, uint32_t second_order,
                   real_t* answer, uint32_t room);

// Give the polynomial whose value is how fast the given one is changing.
//
// The answer is of one order less and holds POLY_COEFFICIENT_COUNT(order - 1)
// numbers. Give false where the order is nothing, since a constant does not
// change.
bool poly_derivative(const real_t* coefficient, uint32_t order,
                     real_t* answer);

// Find where the polynomial crosses nothing.
//
// The roots hold one complex number for each order and come back in no
// particular order. A root that is real has an imaginary part of nothing; the
// complex ones come in pairs, one the mirror of the other.
//
// Give false if the order is not one poly_is_valid_order accepts, if the
// highest coefficient is nothing, which means the polynomial is really of a
// lower order, or if the roots could not be found.
bool poly_roots(const real_t* coefficient, uint32_t order, cnum_t* roots);

// True if every root of the polynomial lies inside the unit circle.
//
// THIS IS THE STABILITY OF A FILTER, and it is the reason this module exists.
// Give the DENOMINATOR of the filter, which for one biquad section of the iir
// module is 1, a1, a2.
//
// A pole outside the circle is a filter that runs away: the answer doubles
// every few samples until it is nothing but infinities. A pole exactly on the
// circle never settles either, thus the room is measured inwards and a root
// within POLY_CIRCLE_ROOM of the edge is not counted as inside.
//
// Give false where the roots cannot be found, which is the safe answer: a
// filter that cannot be shown to be stable should not be trusted to be.
bool poly_is_inside_circle(const real_t* coefficient, uint32_t order);

// How far inside the circle a root must be to count as inside.
//
// A pole on the circle is a filter that rings for ever, and the arithmetic of
// a device will push it one way or the other soon enough. The room is small
// enough that it refuses nothing a designer meant to build.
#ifndef POLY_CIRCLE_ROOM
#define POLY_CIRCLE_ROOM        REAL_C(0.000001)
#endif

#endif//POLY_H
