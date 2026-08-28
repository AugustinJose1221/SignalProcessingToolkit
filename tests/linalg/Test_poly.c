#include "unity.h"
#include "real_assert.h"
#include "poly.h"
#include "cnum.h"
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_poly_is_valid_order(void)
{
    TEST_ASSERT_EQUAL(true, poly_is_valid_order(1));
    TEST_ASSERT_EQUAL(true, poly_is_valid_order(2));
    TEST_ASSERT_EQUAL(true, poly_is_valid_order(POLY_LARGEST_ROOT_ORDER));

    TEST_ASSERT_EQUAL(false, poly_is_valid_order(0));
    TEST_ASSERT_EQUAL(false,
                      poly_is_valid_order(POLY_LARGEST_ROOT_ORDER + 1u));
}

void test_poly_evaluate(void)
{
    // 2 plus 3x less x squared. At 2 that is 2 + 6 - 4 = 4.
    real_t p[3] = {REAL_C(2.0), REAL_C(3.0), -REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.0),
                            poly_evaluate(p, 2u, REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0),
                            poly_evaluate(p, 2u, REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(2.0),
                            poly_evaluate(p, 2u, -REAL_C(1.0)));
}

void test_poly_evaluate_complex(void)
{
    // The same, at the place i: 2 + 3i - (i squared) = 3 + 3i.
    real_t p[3] = {REAL_C(2.0), REAL_C(3.0), -REAL_C(1.0)};

    cnum_t at = cnum_make(REAL_C(0.0), REAL_C(1.0));
    cnum_t answer = poly_evaluate_complex(p, 2u, at);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), cnum_real(answer));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0),
                            cnum_imaginary(answer));
}

void test_poly_multiply(void)
{
    // (1 + 2x)(3 + 4x) = 3 + 10x + 8x squared.
    real_t first[2] = {REAL_C(1.0), REAL_C(2.0)};
    real_t second[2] = {REAL_C(3.0), REAL_C(4.0)};
    real_t answer[8];

    TEST_ASSERT_EQUAL(true, poly_multiply(first, 1u, second, 1u, answer, 8u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), answer[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(10.0), answer[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(8.0), answer[2]);

    // Room for one fewer than the answer needs.
    TEST_ASSERT_EQUAL(false, poly_multiply(first, 1u, second, 1u, answer, 2u));
}

void test_multiplying_agrees_with_evaluating_each_and_multiplying(void)
{
    // The product of two polynomials, read at a place, must be the product of
    // the two read at that place.
    real_t first[3] = {REAL_C(1.0), -REAL_C(2.0), REAL_C(0.5)};
    real_t second[3] = {REAL_C(3.0), REAL_C(1.0), -REAL_C(1.5)};
    real_t product[8];

    poly_multiply(first, 2u, second, 2u, product, 8u);

    for(int32_t step = -20; step <= 20; step++)
    {
        real_t at = (real_t)step / REAL_C(10.0);

        real_t apart = poly_evaluate(first, 2u, at)
                       * poly_evaluate(second, 2u, at);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), apart,
                                poly_evaluate(product, 4u, at));
    }
}

void test_poly_derivative(void)
{
    // 2 + 3x - x squared changes at 3 - 2x.
    real_t p[3] = {REAL_C(2.0), REAL_C(3.0), -REAL_C(1.0)};
    real_t slope[3];

    TEST_ASSERT_EQUAL(true, poly_derivative(p, 2u, slope));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), slope[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(2.0), slope[1]);

    // A constant does not change, thus there is nothing to give.
    TEST_ASSERT_EQUAL(false, poly_derivative(p, 0u, slope));
}

void test_a_line_crosses_nothing_once(void)
{
    // 4 + 2x crosses nothing at -2.
    real_t p[2] = {REAL_C(4.0), REAL_C(2.0)};
    cnum_t roots[1];

    TEST_ASSERT_EQUAL(true, poly_roots(p, 1u, roots));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(2.0),
                            cnum_real(roots[0]));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            cnum_imaginary(roots[0]));
}

void test_a_quadratic_with_two_real_roots(void)
{
    // (x - 2)(x + 3) = x squared plus x less 6.
    real_t p[3] = {-REAL_C(6.0), REAL_C(1.0), REAL_C(1.0)};
    cnum_t roots[2];

    TEST_ASSERT_EQUAL(true, poly_roots(p, 2u, roots));

    real_t smaller = (cnum_real(roots[0]) < cnum_real(roots[1]))
                     ? cnum_real(roots[0]) : cnum_real(roots[1]);
    real_t larger = (cnum_real(roots[0]) < cnum_real(roots[1]))
                    ? cnum_real(roots[1]) : cnum_real(roots[0]);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(3.0), smaller);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0), larger);
}

void test_a_quadratic_with_a_pair_that_mirror_each_other(void)
{
    // x squared plus 1 crosses nothing only off the real line, at plus and
    // minus i.
    real_t p[3] = {REAL_C(1.0), REAL_C(0.0), REAL_C(1.0)};
    cnum_t roots[2];

    TEST_ASSERT_EQUAL(true, poly_roots(p, 2u, roots));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            cnum_real(roots[0]));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0),
                            REAL_ABS(cnum_imaginary(roots[0])));

    // And the two really do mirror each other.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), cnum_real(roots[0]),
                            cnum_real(roots[1]));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -cnum_imaginary(roots[0]),
                            cnum_imaginary(roots[1]));
}

void test_a_quadratic_whose_roots_stand_far_apart(void)
{
    // THE PLACE WHERE THE PLAIN FORM OF THE ANSWER FALLS APART. Where one root
    // is very much larger than the other, taking b away from the root of b
    // squared less four ac subtracts two nearly equal numbers and the smaller
    // root is made of rounding. This module reaches it through the product of
    // the two instead.
    //
    // (x - 1000000)(x - 0.000001): the two stand twelve orders apart.
    real_t p[3] = {REAL_C(1.0), -REAL_C(1000000.000001), REAL_C(1.0)};
    cnum_t roots[2];

    TEST_ASSERT_EQUAL(true, poly_roots(p, 2u, roots));

    real_t smaller = (REAL_ABS(cnum_real(roots[0]))
                      < REAL_ABS(cnum_real(roots[1])))
                     ? cnum_real(roots[0]) : cnum_real(roots[1]);

    // The small root is right to a part in a hundred, where the plain form
    // gives nothing at all.
    TEST_ASSERT_TRUE(smaller > REAL_C(0.0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.00000002), REAL_C(0.000001), smaller);
}

void test_every_root_that_comes_back_is_really_a_root(void)
{
    // THE TEST THAT SAYS THE ANSWER IS AN ANSWER, whatever the order. A root
    // is a place where the polynomial is nothing, and nothing else about it
    // matters.
    real_t first[3] = {REAL_C(0.08), -REAL_C(0.6), REAL_C(1.0)};
    real_t second[3] = {REAL_C(0.25), -REAL_C(0.6), REAL_C(1.0)};
    real_t product[8];
    cnum_t roots[POLY_LARGEST_ROOT_ORDER];

    poly_multiply(first, 2u, second, 2u, product, 8u);

    TEST_ASSERT_EQUAL(true, poly_roots(product, 4u, roots));

    real_t largest = REAL_C(0.0);

    for(uint32_t index = 0; index < 4u; index++)
    {
        real_t size_of = cnum_magnitude(poly_evaluate_complex(product, 4u,
                                                              roots[index]));

        if(size_of > largest) { largest = size_of; }
    }

    TEST_ASSERT_TRUE(largest < REAL_C(0.0001));
}

void test_poly_roots_refuses_what_it_cannot_answer(void)
{
    real_t p[3] = {REAL_C(1.0), REAL_C(0.0), REAL_C(1.0)};
    cnum_t roots[POLY_LARGEST_ROOT_ORDER + 2u];

    TEST_ASSERT_EQUAL(false, poly_roots(p, 0u, roots));
    TEST_ASSERT_EQUAL(false, poly_roots(p, POLY_LARGEST_ROOT_ORDER + 1u,
                                        roots));

    // A highest coefficient of nothing means the polynomial is really of a
    // lower order, and answering would give a root at infinity.
    real_t lower[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(0.0)};

    TEST_ASSERT_EQUAL(false, poly_roots(lower, 2u, roots));
}

void test_a_stable_filter_has_every_pole_inside_the_circle(void)
{
    // THE REASON THIS MODULE EXISTS. A biquad section is stable when the roots
    // of its denominator lie inside the unit circle, and a pole outside it is
    // a filter whose answer doubles every few samples until it is nothing but
    // infinities.
    //
    // The denominator is given lowest power first, thus 1, a1, a2 becomes
    // a2, a1, 1 read as a polynomial in the other direction. Here the poles
    // stand at plus and minus 0.9.
    real_t stable[3] = {-REAL_C(0.81), REAL_C(0.0), REAL_C(1.0)};

    TEST_ASSERT_EQUAL(true, poly_is_inside_circle(stable, 2u));

    // Poles at plus and minus 1.1, which is outside.
    real_t running_away[3] = {-REAL_C(1.21), REAL_C(0.0), REAL_C(1.0)};

    TEST_ASSERT_EQUAL(false, poly_is_inside_circle(running_away, 2u));

    // And a pole exactly on the circle never settles either.
    real_t on_the_edge[3] = {-REAL_C(1.0), REAL_C(0.0), REAL_C(1.0)};

    TEST_ASSERT_EQUAL(false, poly_is_inside_circle(on_the_edge, 2u));
}

void test_a_pair_of_poles_near_the_circle_is_judged_rightly(void)
{
    // A sharp filter has its poles very near the circle on purpose, thus the
    // judgement must be exact enough to tell 0.999 from 1.001.
    for(uint32_t which = 0; which < 2u; which++)
    {
        real_t radius = (which == 0u) ? REAL_C(0.999) : REAL_C(1.001);

        // A pair at that radius and an angle, which is x squared less twice
        // the real part x plus the radius squared.
        real_t angle = REAL_C(0.4);
        real_t p[3];

        p[2] = REAL_C(1.0);
        p[1] = -REAL_C(2.0) * radius * REAL_COS(angle);
        p[0] = radius * radius;

        TEST_ASSERT_EQUAL((which == 0u), poly_is_inside_circle(p, 2u));
    }
}

void test_a_filter_that_cannot_be_judged_is_not_called_stable(void)
{
    // A filter that cannot be shown to be stable should not be trusted to be,
    // thus the safe answer where the roots cannot be found is no.
    real_t lower[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(0.0)};

    TEST_ASSERT_EQUAL(false, poly_is_inside_circle(lower, 2u));
}

// Every root of a polynomial whose terms are all small must still be a root.
//
// The walk that finds a root used to stop when the value reached a fixed 100
// times the smallest step the width can tell. That number says nothing about a
// polynomial whose whole size is near it: on 0.000061 - x^4 at 32 bits the walk
// stopped with the value still at 0.000012, a fifth of the polynomial.
//
// The test that follows it asks whether a root is real by comparing the value
// at the root against the value at its real part alone. Given a value that had
// stopped short, the two stood within a factor of ten of each other, and a root
// standing straight up the imaginary line was called real. A line was then
// divided out where a quadratic belonged and every root after it was wrong: the
// first came back as -1.61, where all four true roots stand at 0.0884.
void test_poly_roots_of_a_polynomial_that_is_small_everywhere(void)
{
    // 0.000061 - x^4, whose four roots stand at 0.0884 turned a quarter of the
    // way round each time.
    real_t coefficient[5] = {REAL_C(0.00006103515625), REAL_C(0.0),
                             REAL_C(0.0), REAL_C(0.0), -REAL_C(1.0)};
    cnum_t roots[5];

    TEST_ASSERT_EQUAL(true, poly_roots(coefficient, 4u, roots));

    // How large the polynomial gets near its roots, which is what the value at
    // a root must be measured against.
    real_t size = REAL_C(0.00006103515625) + REAL_C(1.0);

    for(uint32_t index = 0; index < 4u; index++)
    {
        cnum_t value = poly_evaluate_complex(coefficient, 4u, roots[index]);

        TEST_ASSERT_TRUE(cnum_magnitude(value) < (REAL_C(0.001) * size));

        // And every one of them really stands where the four roots stand.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0883883),
                                cnum_magnitude(roots[index]));
    }
}

// A polynomial with the same root four times over.
//
// This is the worst a root finder meets. Dividing out one root of a set that
// stand on top of each other leaves a polynomial made mostly of the error of
// that division, thus the roots found last are found from almost nothing. At 32
// bits the leftover quadratic gave roots at 1536, for four true roots that all
// stand at 0.571, and poly_is_inside_circle then called a stable filter
// unstable.
//
// What was wrong was the polish: the roots ARE walked back against the original
// polynomial afterwards, but with too few steps to walk in from 1536.
void test_poly_roots_of_a_root_repeated_four_times(void)
{
    // (x - 0.5707710)^4, worked out and written down as its coefficients.
    real_t coefficient[5] = {REAL_C(0.10604707151651382),
                             -REAL_C(0.7433340549468994),
                             REAL_C(1.9538923501968384),
                             -REAL_C(2.282625675201416),
                             REAL_C(1.0)};
    cnum_t roots[5];

    TEST_ASSERT_EQUAL(true, poly_roots(coefficient, 4u, roots));

    for(uint32_t index = 0; index < 4u; index++)
    {
        // A root of four cannot be held closely at any width: the answer moves
        // by the fourth root of whatever the coefficients moved by. What must
        // hold is that every one of them stands near the place they all share.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.5707710),
                                cnum_magnitude(roots[index]));
    }

    // And the filter these poles describe is stable, which is the question a
    // caller actually asks.
    TEST_ASSERT_EQUAL(true, poly_is_inside_circle(coefficient, 4u));
}

// Polishing must never make a root worse.
//
// The walk divides the value of the polynomial by its slope. Where several
// roots stand on top of each other both of those are nearly nothing, thus the
// rounding on the slope is a large part of it and one step can go anywhere.
//
// Measured at 32 bits on a root of four at -0.8125: the divisions had already
// left every root within 0.07 of the truth, and polishing threw two of them out
// to 114, where the polynomial reaches 170 million. poly_is_inside_circle then
// called a stable filter unstable. A step that does not make the value smaller
// is no longer taken.
void test_poly_polishing_never_makes_a_root_worse(void)
{
    real_t places[4] = {-REAL_C(0.8125), REAL_C(0.8125), -REAL_C(0.95),
                        REAL_C(0.99)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        real_t away = -places[which];

        // (x - place)^4, written down as its coefficients.
        real_t coefficient[5] = {away * away * away * away,
                                 REAL_C(4.0) * away * away * away,
                                 REAL_C(6.0) * away * away,
                                 REAL_C(4.0) * away,
                                 REAL_C(1.0)};
        cnum_t roots[5];

        TEST_ASSERT_EQUAL(true, poly_roots(coefficient, 4u, roots));

        for(uint32_t index = 0; index < 4u; index++)
        {
            // Every root stands near the place they all share, and none of them
            // has been thrown out of the circle.
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), REAL_ABS(places[which]),
                                    cnum_magnitude(roots[index]));
            TEST_ASSERT_TRUE(cnum_magnitude(roots[index]) < REAL_C(1.0));
        }

        TEST_ASSERT_EQUAL(true, poly_is_inside_circle(coefficient, 4u));
    }
}

// Every root of x to the power n stands at nothing, which is as plainly inside
// the circle as a set of roots can be.
//
// The walk towards a place where several roots stand together creeps in rather
// than rushing, keeping a share of its distance at every step, thus it needs
// about as many steps as there are roots there multiplied by the digits to get
// right. The budget was a flat 200. That covers six roots together and not
// seven: poly_roots refused x to the seventh, and poly_is_inside_circle then
// said no about the plainest stable filter there is. The budget now follows the
// order.
void test_poly_roots_of_every_root_at_nothing(void)
{
    for(uint32_t order = 2u; order <= POLY_LARGEST_ROOT_ORDER; order++)
    {
        real_t coefficient[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];
        cnum_t roots[POLY_COEFFICIENT_COUNT(POLY_LARGEST_ROOT_ORDER)];

        for(uint32_t index = 0; index <= order; index++)
        {
            coefficient[index] = REAL_C(0.0);
        }

        coefficient[order] = REAL_C(1.0);

        TEST_ASSERT_EQUAL(true, poly_roots(coefficient, order, roots));

        for(uint32_t index = 0; index < order; index++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                                    cnum_magnitude(roots[index]));
        }

        TEST_ASSERT_EQUAL(true, poly_is_inside_circle(coefficient, order));
    }
}
