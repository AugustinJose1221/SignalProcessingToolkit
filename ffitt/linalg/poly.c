// This file is left out of the build when FFITT_NO_LINALG is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_LINALG

#ifndef TEST
#include <ffitt/linalg/poly.h>
#include <ffitt/core/defs.h>
#else
#include "poly.h"
#include "defs.h"
#endif

#include <math.h>

bool poly_is_valid_order(uint32_t order)
{
    return (order >= 1u) && (order <= POLY_LARGEST_ROOT_ORDER);
}

real_t poly_evaluate(const real_t* coefficient, uint32_t order, real_t x)
{
    ASSERT(coefficient != NULL);

    // From the highest power inwards. No power is ever formed on its own, thus
    // nothing is lost to the size of x raised to a large number.
    real_t total = coefficient[order];

    for(uint32_t step = order; step >= 1u; step--)
    {
        total = (total * x) + coefficient[step - 1u];
    }

    return total;
}

cnum_t poly_evaluate_complex(const real_t* coefficient, uint32_t order,
                             cnum_t x)
{
    ASSERT(coefficient != NULL);

    cnum_t total = cnum_from_real(coefficient[order]);

    for(uint32_t step = order; step >= 1u; step--)
    {
        total = cnum_add(cnum_multiply(total, x),
                         cnum_from_real(coefficient[step - 1u]));
    }

    return total;
}

bool poly_multiply(const real_t* first, uint32_t first_order,
                   const real_t* second, uint32_t second_order,
                   real_t* answer, uint32_t room)
{
    ASSERT(first != NULL);
    ASSERT(second != NULL);
    ASSERT(answer != NULL);

    uint32_t order = first_order + second_order;
    uint32_t wanted = POLY_COEFFICIENT_COUNT(order);

    if(room < wanted)
    {
        return false;
    }

    for(uint32_t index = 0; index < wanted; index++)
    {
        answer[index] = REAL_C(0.0);
    }

    // Every power of the one multiplied by every power of the other, and the
    // products gathered where their powers add up to.
    for(uint32_t left = 0; left <= first_order; left++)
    {
        for(uint32_t right = 0; right <= second_order; right++)
        {
            answer[left + right] += first[left] * second[right];
        }
    }

    return true;
}

bool poly_derivative(const real_t* coefficient, uint32_t order,
                     real_t* answer)
{
    ASSERT(coefficient != NULL);
    ASSERT(answer != NULL);

    if(order == 0u)
    {
        return false;
    }

    for(uint32_t power = 1; power <= order; power++)
    {
        answer[power - 1u] = coefficient[power] * (real_t)power;
    }

    return true;
}

// The two roots of a quadratic, which may be complex.
//
// WRITTEN SO THAT NO TWO NEARLY EQUAL NUMBERS ARE SUBTRACTED. The plain form
// of the answer takes b away from the root of b squared less four ac, and
// where those two are nearly equal the answer is made of rounding. Taking the
// root that ADDS and reaching the other through the product of the two costs
// nothing and holds every digit.
static void poly_quadratic(real_t a, real_t b, real_t c, cnum_t* roots)
{
    real_t under = (b * b) - (REAL_C(4.0) * a * c);

    if(under >= REAL_C(0.0))
    {
        real_t root = REAL_SQRT(under);

        // The one whose subtraction cannot cancel.
        real_t away = (b >= REAL_C(0.0)) ? (-b - root) : (-b + root);
        real_t first = away / (REAL_C(2.0) * a);

        // And the other from the product of the two, which is c over a.
        real_t second = (REAL_ABS(away) > REAL_SMALLEST)
                        ? ((REAL_C(2.0) * c) / away)
                        : first;

        roots[0] = cnum_from_real(first);
        roots[1] = cnum_from_real(second);

        return;
    }

    // A pair that mirror each other.
    real_t real_part = -b / (REAL_C(2.0) * a);
    real_t imaginary_part = REAL_SQRT(-under) / (REAL_C(2.0) * a);

    roots[0] = cnum_make(real_part, imaginary_part);
    roots[1] = cnum_make(real_part, -imaginary_part);
}

// Find one root by walking downhill from a starting place, and give false
// where the walk does not arrive.
//
// The step is the one of Newton, which is the value divided by how fast the
// value is changing. It is taken in complex numbers so that a root off the
// real line can be reached.
// How large the polynomial itself is at a place this far from nothing.
//
// THIS IS THE NUMBER THAT SAYS WHAT "NEARLY NOTHING" MEANS. Adding up the terms
// of a polynomial loses digits to the largest of them, thus a value of the
// polynomial can only be trusted down to about the size of that largest term
// multiplied by the smallest step the width can tell. A fixed number in its
// place asks the same of a polynomial whose terms are millions and of one whose
// terms are millionths, and it is wrong for one of them whichever number is
// chosen.
static real_t poly_size_at(const real_t* coefficient, uint32_t order,
                           real_t how_far)
{
    real_t total = REAL_C(0.0);
    real_t power = REAL_C(1.0);

    for(uint32_t index = 0; index <= order; index++)
    {
        total += REAL_ABS(coefficient[index]) * power;
        power *= how_far;
    }

    return total;
}

// How many steps the walk is given for a polynomial of this order.
//
// A WALK TOWARDS A ROOT THAT STANDS ALONE ARRIVES IN A HANDFUL OF STEPS. It
// doubles the digits it has right at every step, thus a dozen steps is plenty.
// A walk towards a place where SEVERAL roots stand on top of each other does
// not: it creeps in, keeping the same share of its distance each time, and the
// share it keeps is one less one over how many roots are there. For a place
// where m roots stand together it therefore takes about m times as many steps
// as there are digits to get right, which at 64 bits is about 36m.
//
// The budget was a flat 200, which is enough for m up to 6 and not for 7. It
// showed as x to the seventh: every one of its seven roots stands at nothing,
// the walk was still creeping towards them when the steps ran out, and
// poly_roots refused a polynomial whose roots could not be more plainly inside
// the circle. x to the sixth, needing about 187 steps, passed.
//
// The order is the largest m can be, thus this is what covers the worst case.
// The cost of the steps that are not needed is nothing: a walk that arrives
// stops.
#define POLY_WALK_STEPS(order)      (((order) * 50u) + 100u)

static bool poly_one_root(const real_t* coefficient, uint32_t order,
                          cnum_t* found)
{
    // The start is off the real line on purpose. A polynomial with real
    // coefficients that is started on the real line can only walk along it,
    // and would never reach a root that is not there.
    cnum_t at = cnum_make(REAL_C(0.4), REAL_C(0.9));

    real_t slope[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];

    if(!poly_derivative(coefficient, order, slope))
    {
        return false;
    }

    for(uint32_t step = 0; step < POLY_WALK_STEPS(order); step++)
    {
        cnum_t value = poly_evaluate_complex(coefficient, order, at);

        // Nearly nothing FOR THIS POLYNOMIAL AT THIS PLACE, and not nearly
        // nothing on its own.
        //
        // This was a fixed 100 times the smallest step the width can tell, and
        // that stopped the walk early wherever the polynomial is small
        // everywhere. On 0.000061 - x^4 at 32 bits the walk stopped with the
        // value still at 0.000012, which is a fifth of the whole polynomial.
        // The test below that asks whether a root is real then compared that
        // number against the value at the real part alone, found them within a
        // factor of ten of each other, and called a root that stands straight
        // up the imaginary line a real one. A line was divided out where a
        // quadratic belonged and every root after it was wrong: the first came
        // back as -1.61 where the four true roots all stand at 0.088.
        real_t size = poly_size_at(coefficient, order, cnum_magnitude(at));

        // The size cannot be nothing, because the highest coefficient is not
        // nothing and poly_roots refuses a polynomial where it is.
        if(cnum_magnitude(value) <= (REAL_C(100.0) * REAL_EPSILON * size))
        {
            *found = at;
            return true;
        }

        cnum_t changing = poly_evaluate_complex(slope, order - 1u, at);

        if(cnum_magnitude(changing) <= REAL_SMALLEST)
        {
            // Flat here, thus there is no downhill to walk. Move aside and try
            // again from somewhere else.
            at = cnum_add(at, cnum_make(REAL_C(0.1), REAL_C(0.1)));
        }
        else
        {
            cnum_t moved = cnum_subtract(at, cnum_divide(value, changing));
            bool settled = (cnum_magnitude(cnum_subtract(moved, at))
                            <= (REAL_C(10.0) * REAL_EPSILON
                                * (cnum_magnitude(at) + REAL_C(1.0))));

            at = moved;

            if(settled)
            {
                *found = at;
                return true;
            }
        }
    }

    *found = at;

    // It went somewhere and stayed there, thus it is taken as a root only if
    // the value there really is near nothing. Near nothing FOR THIS POLYNOMIAL,
    // for the reason poly_size_at gives: a fixed number here would take any
    // place at all as a root of a polynomial whose terms are all smaller than
    // that number.
    return cnum_magnitude(poly_evaluate_complex(coefficient, order, at))
           <= (REAL_C(0.001) * poly_size_at(coefficient, order,
                                            cnum_magnitude(at)));
}

bool poly_roots(const real_t* coefficient, uint32_t order, cnum_t* roots)
{
    ASSERT(coefficient != NULL);
    ASSERT(roots != NULL);

    if(!poly_is_valid_order(order))
    {
        return false;
    }

    // A highest coefficient of nothing means the polynomial is really of a
    // lower order, and answering as though it were not would give a root at
    // infinity.
    if(REAL_ABS(coefficient[order]) <= REAL_SMALLEST)
    {
        return false;
    }

    // The closed forms, which are exact and need no walking at all.
    if(order == 1u)
    {
        roots[0] = cnum_from_real(-coefficient[0] / coefficient[1]);
        return true;
    }

    if(order == 2u)
    {
        poly_quadratic(coefficient[2], coefficient[1], coefficient[0], roots);
        return true;
    }

    // ONE ROOT AT A TIME, DIVIDING EACH OUT AS IT IS FOUND. Every division
    // carries its own error into what is left, thus the last roots found are
    // the worst, and that is what caps the order this module takes.
    real_t left[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];
    real_t slope[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];
    uint32_t standing = order;

    if(!poly_derivative(coefficient, order, slope))
    {
        return false;
    }

    for(uint32_t index = 0; index <= order; index++)
    {
        left[index] = coefficient[index];
    }

    uint32_t found = 0u;

    while(standing > 2u)
    {
        cnum_t root;

        if(!poly_one_root(left, standing, &root))
        {
            return false;
        }

        // IS THIS ROOT REALLY COMPLEX, OR IS IT A REAL ONE WITH A LITTLE
        // ROUNDING ON IT?
        //
        // The question cannot be answered by looking at how small the
        // imaginary part is, and trying to was the first thing that went
        // wrong here: a root at 0.2 came back with an imaginary part of
        // 0.001, which no fixed threshold tells from a real pair standing
        // that close together. The polynomial was then divided by a quadratic
        // where a line belonged, and every root after it was wrong.
        //
        // The question that CAN be answered is whether the real part ALONE is
        // a root. If dropping the imaginary part leaves the value near
        // nothing, the root was real and the imaginary part was rounding.
        real_t answer[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];

        real_t as_complex = cnum_magnitude(poly_evaluate_complex(left,
                                                                 standing,
                                                                 root));
        real_t as_real = REAL_ABS(poly_evaluate(left, standing,
                                                cnum_real(root)));

        if(as_real <= ((REAL_C(10.0) * as_complex) + REAL_SMALLEST))
        {
            real_t place = cnum_real(root);

            roots[found] = cnum_from_real(place);
            found++;

            // DIVIDING ONE ROOT OUT, from the highest power downwards. What is
            // left is a polynomial of one order less whose roots are all the
            // others.
            answer[standing - 1u] = left[standing];

            for(uint32_t step = standing - 1u; step >= 1u; step--)
            {
                answer[step - 1u] = left[step] + (place * answer[step]);
            }

            standing--;

            for(uint32_t index = 0; index <= standing; index++)
            {
                left[index] = answer[index];
            }
        }
        else
        {
        // A complex root brings its mirror with it, thus both are taken and a
        // quadratic is divided out rather than a single root. That keeps every
        // coefficient real and the pair exactly mirrored.
        roots[found] = root;
        roots[found + 1u] = cnum_conjugate(root);
        found += 2u;

        // The quadratic they are the roots of, as x squared plus this much x
        // plus this much.
        real_t middle = -REAL_C(2.0) * cnum_real(root);
        real_t lowest = cnum_magnitude_squared(root);

        answer[standing - 2u] = left[standing];

        if(standing >= 3u)
        {
            answer[standing - 3u] = left[standing - 1u]
                                    - (middle * answer[standing - 2u]);
        }

        for(uint32_t step = standing - 2u; step >= 2u; step--)
        {
            answer[step - 2u] = left[step]
                                - (middle * answer[step - 1u])
                                - (lowest * answer[step]);
        }

            standing -= 2u;

            for(uint32_t index = 0; index <= standing; index++)
            {
                left[index] = answer[index];
            }
        }
    }

    // What is left is a quadratic or a line, and both have a closed form.
    if(standing == 2u)
    {
        poly_quadratic(left[2], left[1], left[0], &roots[found]);
        found += 2u;
    }
    else if(standing == 1u)
    {
        roots[found] = cnum_from_real(-left[0] / left[1]);
        found++;
    }

    if(found != order)
    {
        return false;
    }

    // EVERY ROOT IS NOW POLISHED AGAINST THE POLYNOMIAL IT REALLY CAME FROM.
    //
    // The roots above were found from what was LEFT after the earlier ones
    // were divided out, and every division carries its own error into what
    // follows. The same walk against the original polynomial takes that error
    // back off again, and it is what makes the answer worth having above the
    // lowest orders.
    //
    // AND IT IS GIVEN AS MANY STEPS AS THE FIRST WALK, not a handful. It was a
    // handful, and a handful is enough only where the last division left the
    // root somewhere near where it belongs. It does not always: a polynomial
    // with the same root four times over leaves a quadratic whose coefficients
    // are mostly the error of three divisions, and at 32 bits the roots of that
    // quadratic came back at 1536 for four true roots that all stand at 0.571.
    // Walking in from 1536 costs about thirty five steps before the walk even
    // reaches the roots, and a root of four is walked towards slowly after
    // that. With the budget raised the four come back between 0.561 and 0.571,
    // which is as close as a root of four can be held at this width, and all of
    // them inside the circle where they belong.
    for(uint32_t index = 0; index < order; index++)
    {
        cnum_t at = roots[index];
        real_t here = cnum_magnitude(poly_evaluate_complex(coefficient, order,
                                                           at));

        for(uint32_t step = 0; step < POLY_WALK_STEPS(order); step++)
        {
            cnum_t value = poly_evaluate_complex(coefficient, order, at);
            cnum_t changing = poly_evaluate_complex(slope, order - 1u, at);

            if(cnum_magnitude(changing) <= REAL_SMALLEST)
            {
                break;
            }

            cnum_t moved = cnum_subtract(at, cnum_divide(value, changing));
            real_t there = cnum_magnitude(poly_evaluate_complex(coefficient,
                                                                order, moved));

            // A STEP THAT DOES NOT MAKE THE VALUE SMALLER IS NOT TAKEN, and
            // this guard is what makes polishing safe rather than merely
            // usually helpful.
            //
            // The walk divides the value by the slope. Where several roots
            // stand on top of each other BOTH of those are nearly nothing, and
            // the rounding on the slope is then a large part of it. One step
            // can be enormous, and it goes wherever that rounding pointed.
            // Measured at 32 bits on a root of four at -0.8125, the divisions
            // had already left every root within 0.07 of the truth and the
            // polishing threw two of them out to 114, where the polynomial
            // reaches 170 million. The answer went from good to worse than
            // useless, and poly_is_inside_circle then called a stable filter
            // unstable.
            //
            // Written this way round rather than as a test for "worse", so
            // that a step giving a value that is not a number is not taken
            // either.
            if(!(there < here))
            {
                break;
            }

            bool settled = (cnum_magnitude(cnum_subtract(moved, at))
                            <= (REAL_EPSILON
                                * (cnum_magnitude(at) + REAL_C(1.0))));

            at = moved;
            here = there;

            if(settled)
            {
                break;
            }
        }

        // A root that was found as real stays real. Polishing must not give it
        // an imaginary part that the mirror of it would then not match.
        if(cnum_imaginary(roots[index]) == REAL_C(0.0))
        {
            at = cnum_from_real(cnum_real(at));
        }

        roots[index] = at;
    }

    return true;
}

bool poly_is_inside_circle(const real_t* coefficient, uint32_t order)
{
    ASSERT(coefficient != NULL);

    cnum_t roots[POLY_LARGEST_ROOT_ORDER];

    if(!poly_roots(coefficient, order, roots))
    {
        // A filter that cannot be shown to be stable should not be trusted to
        // be, thus the safe answer is no.
        return false;
    }

    for(uint32_t index = 0; index < order; index++)
    {
        if(cnum_magnitude(roots[index]) >= (REAL_C(1.0) - POLY_CIRCLE_ROOM))
        {
            return false;
        }
    }

    return true;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int poly_is_not_in_this_build_t;

#endif//FFITT_NO_LINALG
