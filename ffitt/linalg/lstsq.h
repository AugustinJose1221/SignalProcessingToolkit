#ifndef LSTSQ_H
#define LSTSQ_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif

// Fitting a line, a curve or a model through more readings than it has room
// for.
//
// A calibration takes twenty readings to fix three numbers. There is no answer
// that passes through all twenty, and looking for one is the wrong question.
// The right one is: which three numbers leave the smallest total error, and
// this module answers it.
//
// WHAT THIS IS FOR
//
//   A CALIBRATION CURVE. Twenty readings of a sensor against a reference, and
//   a polynomial of the third order to turn one into the other.
//   TAKING A TREND OUT. Fit a straight line and subtract it, which is what the
//   detrend module does with this underneath.
//   FITTING A MODEL. Anything of the form "the reading is this much of one
//   thing plus that much of another", where the amounts are wanted.
//
// HOW IT IS DONE, AND WHAT THAT COSTS
//
// The normal equations. The problem of many readings and few numbers becomes a
// small square problem: the model turned round and multiplied by itself, and
// solved with the factor of Cholesky, which is half the work of a general
// elimination because the small problem is always symmetric.
//
// THE PRICE IS PRECISION, AND IT MUST BE STATED. Turning the model round and
// multiplying it by itself SQUARES how badly conditioned it is. A fit that
// would need seven digits by a careful method needs fourteen by this one.
//
// WHAT STOPS A BAD ANSWER FROM BEING GIVEN BACK, AND WHAT DOES NOT
//
// A factor exists long after the answer has stopped meaning anything, thus the
// factor alone is not the test. The module looks at the diagonal of the factor
// instead: two columns that say almost the same thing leave one diagonal tiny
// beside the others, and above what the width can hold the fit is refused.
//
// THAT GUARD CATCHES ONE FAULT AND NOT THE OTHER, THUS THERE IS A SECOND ONE.
//
//   IT CATCHES columns of the model that say the same thing, or nearly so.
//   That is a fault of the MODEL, and the factor shows it plainly.
//
//   IT DOES NOT CATCH the digits lost in FORMING the normal equations. Turning
//   the model round and multiplying it by itself is where the loss happens,
//   and by the time the factor is taken the loss has already happened. The
//   factor of a matrix built from spent digits looks perfectly healthy,
//   because it is: it is the healthy factor of the wrong matrix.
//
// READING THE ANSWER BACK DOES NOT FIND IT EITHER, and it is worth saying so
// because it is the first thing anyone tries. A fit that is right leaves an
// error holding nothing of any column of the model. Measured over 20000 random
// sets at 32 bits, the answers that were RIGHT leaned on a column by as much
// as 8.6 parts in ten thousand, and the answers that were WRONG by as little
// as 1.2 parts in a hundred thousand. The two ranges lie across each other.
// No rule about the error left behind can part them.
//
// WHAT DOES PART THEM is the same fit done with the places brought near zero,
// which does not lose the digits. Where the two disagree it is the plain one
// that is wrong. Thus lstsq_polyfit DOES BOTH and gives nothing back where the
// plain fit leaves more error than the other. Measured, at 32 bits: not one
// wrong answer in 20000 sets, and 1.6 fits in every 100 refused. At 64 bits
// nothing is refused, because nothing is wrong.
//
// THE PRICE IS TWICE THE WORK, and it is affordable here and nowhere else: a
// fit is worked out once when a calibration is made, and not while a device
// runs. WHERE THE COST IS NOT WANTED, CALL lstsq_polyfit_scaled, which does
// the right fit once and needs no comparison at all.
//
// Measured, on 14 readings whose x runs from 2 to 6, at 32 bits. A curve of a
// higher order can always do whatever a lower one did, thus the quality must
// never fall as the order rises:
//
//     order            1       2       3       4       5
//     plain fit     0.171   0.482   0.769  refused  refused
//     scaled fit    0.171   0.482   0.769   0.930    0.983
//
// Before this check the plain fit gave back 0.527 at order 4 and said nothing
// was wrong. Now it refuses, and the scaled fit answers 0.930 on the same data
// at the same width.
//
// WHERE X SITS MATTERS AS MUCH AS THE ORDER, AND THIS IS THE TRAP
//
// Read the two rows of that table against each other. THE SAME READINGS AND
// THE SAME WIDTH REACH MORE THAN TWICE THE ORDER when x is moved to -1 to 1.
// Nothing about the readings changed; only where their x sits.
//
// It gets worse than the table shows. Move 50 points to x from 1000 to 1001
// and EVEN A CUBIC IS REFUSED, at 64 bits, on data that fits perfectly.
//
// The reason is that 1000 to the sixth is a number near 10 to the eighteenth,
// and the small problem holds sums of such numbers beside sums of numbers near
// 1. Nothing is left of the small ones.
//
// A calibration is exactly where this bites. A thermistor read in ohms runs
// from 1000 to 70000, and a plain fit through it fails whatever the order.
//
// THE ANSWER IS lstsq_polyfit_scaled, which brings x to a range about -1 to 1
// first and gives back the centre and the width it used. Use it unless the x
// of the readings already runs about -1 to 1. It costs one subtraction and one
// division for each reading and it removes the whole trouble.
//
// A HIGH ORDER IS USUALLY THE WRONG ANSWER ANYWAY. A polynomial of the ninth
// order through twelve calibration points passes through all of them and swings
// wildly between them. Where a table is what is wanted, the interp module
// reads between its points without inventing anything; where a curve is
// wanted, the third or the fourth order is nearly always enough.

// The smallest a diagonal of the factor may be, as a part of the largest one,
// before the answer is refused.
//
// The square root of the smallest step the width can tell, and a margin of
// two. The square of the ratio of the diagonals is how badly conditioned the
// small problem is, thus this holds that at about 1 divided by the smallest
// step: the last digit of the answer is rounding, and no digit beyond it is
// claimed. The margin is what catches a model whose columns are exactly alike;
// the header says why it is needed.
#ifndef LSTSQ_SMALLEST_PIVOT_PART
#define LSTSQ_SMALLEST_PIVOT_PART       (REAL_C(2.0) * REAL_SQRT(REAL_EPSILON))
#endif

// How much more error a plain fit may leave than the same fit done with the
// places brought near zero, before it is refused.
//
// The two are worked out along different roads and neither lands exactly, thus
// a little room is needed. A fit that leaves a hundredth more error than
// another fit of the same order through the same readings is not the least
// squares answer at all.
//
// Measured over 20000 random sets of readings at 32 bits: at a hundredth, not
// one wrong answer was given back, and 1.6 in every 100 fits were refused. A
// wider room lets wrong answers through and a narrower one only refuses more.
// At 64 bits nothing was refused and nothing was wrong at any setting.
#ifndef LSTSQ_LARGEST_EXCESS
#define LSTSQ_LARGEST_EXCESS    REAL_C(0.01)
#endif

// The smallest error worth comparing, as a part of the size of the readings.
//
// Where the readings lie on the curve, both fits leave nothing but rounding.
// Two such numbers cannot be compared by their ratio: one may be ten times the
// other and both be zero to every digit that matters. This is the floor below
// which a difference is not a difference.
#ifndef LSTSQ_SMALLEST_ERROR
#define LSTSQ_SMALLEST_ERROR    (REAL_C(1000.0) * REAL_EPSILON)
#endif

// The highest order of polynomial that the width of the build can carry.
//
// This is the best that was reached at each width, with x from -1 to 1, which
// is what lstsq_polyfit_scaled gives. It is a cap against a mistaken order and
// NOT a promise: a fit at this order through x that sits elsewhere is refused
// by the guard on the diagonal, and rightly.
#ifndef LSTSQ_HIGHEST_ORDER
#if defined(FFITT_REAL_64)
#define LSTSQ_HIGHEST_ORDER     23u
#else
#define LSTSQ_HIGHEST_ORDER     10u
#endif
#endif

// How many numbers a polynomial of the given order holds, which is one more
// than the order: a line is of the first order and holds two.
#define LSTSQ_COEFFICIENT_COUNT(order)      ((order) + 1u)

// True if a polynomial of this order can be fitted through this many points at
// the width of this build.
//
// There must be at least as many points as numbers to find, and the order must
// not be above LSTSQ_HIGHEST_ORDER. This says nothing about whether the
// readings can fix a polynomial of that order; only the fit itself can say
// that, and it does.
bool lstsq_is_valid_fit(uint32_t size, uint32_t order);

// Solve a set of equations that has more rows than columns, in the sense of
// the least total squared error.
//
// The model holds one row for each reading and one column for each number to
// find. The readings hold one row each. The answer holds one row for each
// number to find.
//
// The two scratch matrices must be square and as wide as the model, and one
// column matrix as tall. They lose their content.
//
// Give false if the shapes do not fit together, if the small problem has no
// factor, or if two columns of the model say so nearly the same thing that the
// answer would be made of rounding.
bool lstsq_solve(matrix_t* model, matrix_t* readings, matrix_t* answer,
                 matrix_t* square, matrix_t* factor, matrix_t* column);

// Fit a polynomial of the given order through the points, in the sense of the
// least total squared error.
//
// The coefficients hold LSTSQ_COEFFICIENT_COUNT values, lowest power first:
// the first is the constant, the second multiplies x, the third x squared.
//
// This function gets memory from the heap for the matrices it needs. It runs
// once, when a calibration is worked out, and not while a device runs.
//
// Give false if lstsq_is_valid_fit is false, or if the points cannot fix a
// polynomial of that order. That happens when too many of them share an x,
// when the order is too high for the width, and above all when the x of the
// readings sits far from zero. Read the header on that last one.
bool lstsq_polyfit(const real_t* x, const real_t* y, uint32_t size,
                   uint32_t order, real_t* coefficients);

// Give the centre and the width that bring a set of x to a range about -1 to 1.
//
// The centre is the middle of the range that the readings cover and the width
// is half of it. A set of readings that all share one x has no width; the
// width then comes back as 1, which changes nothing and lets the caller carry
// on to a fit that will refuse for the real reason.
void lstsq_scaling(const real_t* x, uint32_t size, real_t* centre,
                   real_t* width);

// Fit a polynomial through the points, bringing x to a range about -1 to 1
// first.
//
// TAKE THIS ONE unless the x of the readings already runs about -1 to 1. The
// header says why: a plain fit through readings whose x runs from 1000 to 1001
// fails at any order and at either width.
//
// The coefficients are for the SCALED place, thus they must be read with
// lstsq_evaluate_scaled and the centre and the width that come back here. They
// are not a polynomial in x and using them as one gives nonsense.
bool lstsq_polyfit_scaled(const real_t* x, const real_t* y, uint32_t size,
                          uint32_t order, real_t* coefficients,
                          real_t* centre, real_t* width);

// Give the value at one place of a fit that was scaled.
//
// The centre and the width must be the ones that lstsq_polyfit_scaled gave.
real_t lstsq_evaluate_scaled(const real_t* coefficients, uint32_t order,
                             real_t centre, real_t width, real_t x);

// Give the value of a polynomial at one place.
//
// The coefficients are lowest power first, as lstsq_polyfit writes them. The
// work is done from the highest power inwards, which needs one multiplication
// and one addition for each order and never forms a power on its own: forming
// x to the ninth directly loses digits that this way keeps.
real_t lstsq_evaluate(const real_t* coefficients, uint32_t order, real_t x);

// Give how much of the movement of the readings the fit accounts for, from 0
// to 1.
//
// This is the one number to look at after a fit. A value near 1 says the curve
// follows the readings; a value near 0 says it does not, and then the order,
// the model or the readings are wrong. A fit that is never examined is a fit
// that is believed for no reason.
real_t lstsq_fit_quality(const real_t* x, const real_t* y, uint32_t size,
                         const real_t* coefficients, uint32_t order);

// Give how much of the movement of the readings a scaled fit accounts for,
// from 0 to 1.
//
// The centre and the width must be the ones that lstsq_polyfit_scaled gave.
real_t lstsq_fit_quality_scaled(const real_t* x, const real_t* y, uint32_t size,
                                const real_t* coefficients, uint32_t order,
                                real_t centre, real_t width);

#endif//LSTSQ_H
