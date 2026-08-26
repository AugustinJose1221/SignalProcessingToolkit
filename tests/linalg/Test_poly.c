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
