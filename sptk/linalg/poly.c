#ifndef TEST
#include <sptk/linalg/poly.h>
#include <sptk/core/defs.h>
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

    for(uint32_t step = 0; step < 200u; step++)
    {
        cnum_t value = poly_evaluate_complex(coefficient, order, at);

        if(cnum_magnitude(value) <= (REAL_C(100.0) * REAL_EPSILON))
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
    // the value there really is near nothing.
    return cnum_magnitude(poly_evaluate_complex(coefficient, order, at))
           <= REAL_C(0.001);
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
    // follows. A few steps of the same walk against the original polynomial
    // takes that error back off again, and it is what makes the answer worth
    // having above the lowest orders.
    for(uint32_t index = 0; index < order; index++)
    {
        cnum_t at = roots[index];

        for(uint32_t step = 0; step < 20u; step++)
        {
            cnum_t value = poly_evaluate_complex(coefficient, order, at);
            cnum_t changing = poly_evaluate_complex(slope, order - 1u, at);

            if(cnum_magnitude(changing) <= REAL_SMALLEST)
            {
                break;
            }

            cnum_t moved = cnum_subtract(at, cnum_divide(value, changing));

            if(cnum_magnitude(cnum_subtract(moved, at))
               <= (REAL_EPSILON * (cnum_magnitude(at) + REAL_C(1.0))))
            {
                at = moved;
                break;
            }

            at = moved;
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
