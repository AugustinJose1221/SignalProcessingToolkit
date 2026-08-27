#include "unity.h"
#include "real_assert.h"
#include "cnum.h"
#include <stdlib.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_cnum_make(void)
{
    cnum_t number = cnum_make(REAL_C(1.5), -REAL_C(2.5));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.5), number.re);
    TEST_ASSERT_EQUAL_REAL(-REAL_C(2.5), number.im);
}

void test_cnum_from_real(void)
{
    cnum_t number = cnum_from_real(REAL_C(4.0));
    TEST_ASSERT_EQUAL_REAL(REAL_C(4.0), number.re);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), number.im);
}

void test_cnum_zero_and_one(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_zero(cnum_zero()));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), cnum_one().re);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), cnum_one().im);
}

void test_cnum_add(void)
{
    cnum_t sum = cnum_add(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(3.0), -REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(4.0), sum.re);
    TEST_ASSERT_EQUAL_REAL(-REAL_C(3.0), sum.im);
}

void test_cnum_subtract(void)
{
    cnum_t difference = cnum_subtract(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(3.0), -REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(2.0), difference.re);
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0), difference.im);
}

void test_cnum_multiply(void)
{
    // (1 + 2i)(3 + 4i) = -5 + 10i
    cnum_t product = cnum_multiply(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(3.0), REAL_C(4.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(5.0), product.re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(10.0), product.im);
}

void test_cnum_multiply_by_the_imaginary_unit_turns_the_number(void)
{
    // i times i is -1.
    cnum_t unit = cnum_make(REAL_C(0.0), REAL_C(1.0));
    cnum_t product = cnum_multiply(unit, unit);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(1.0), product.re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), product.im);
}

void test_cnum_divide(void)
{
    // (1 + 2i)/(3 + 4i) = 0.44 + 0.08i
    cnum_t quotient = cnum_divide(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(3.0), REAL_C(4.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.44), quotient.re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.08), quotient.im);
}

void test_cnum_divide_by_zero_gives_zero(void)
{
    cnum_t quotient = cnum_divide(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_zero());
    TEST_ASSERT_EQUAL(true, cnum_is_zero(quotient));
}

void test_cnum_multiply_and_divide_are_opposite_operations(void)
{
    cnum_t a = cnum_make(REAL_C(2.5), -REAL_C(1.5));
    cnum_t b = cnum_make(-REAL_C(0.5), REAL_C(3.0));
    cnum_t back = cnum_divide(cnum_multiply(a, b), b);
    TEST_ASSERT_EQUAL(true, cnum_is_near(a, back, REAL_C(0.001)));
}

void test_cnum_scale(void)
{
    cnum_t scaled = cnum_scale(cnum_make(REAL_C(1.0), -REAL_C(2.0)), REAL_C(3.0));
    TEST_ASSERT_EQUAL_REAL(REAL_C(3.0), scaled.re);
    TEST_ASSERT_EQUAL_REAL(-REAL_C(6.0), scaled.im);
}

void test_cnum_conjugate(void)
{
    cnum_t conjugate = cnum_conjugate(cnum_make(REAL_C(1.0), -REAL_C(2.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), conjugate.re);
    TEST_ASSERT_EQUAL_REAL(REAL_C(2.0), conjugate.im);
}

void test_cnum_a_number_times_its_conjugate_is_real(void)
{
    cnum_t a = cnum_make(REAL_C(3.0), REAL_C(4.0));
    cnum_t product = cnum_multiply(a, cnum_conjugate(a));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(25.0), product.re);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), product.im);
}

void test_cnum_negate(void)
{
    cnum_t negative = cnum_negate(cnum_make(REAL_C(1.0), -REAL_C(2.0)));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(1.0), negative.re);
    TEST_ASSERT_EQUAL_REAL(REAL_C(2.0), negative.im);
}

void test_cnum_real_and_imaginary(void)
{
    cnum_t number = cnum_make(REAL_C(7.0), -REAL_C(8.0));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0), cnum_real(number));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(8.0), cnum_imaginary(number));
}

void test_cnum_magnitude(void)
{
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0), cnum_magnitude(cnum_make(REAL_C(3.0), REAL_C(4.0))));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), cnum_magnitude(cnum_zero()));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), cnum_magnitude(cnum_make(REAL_C(0.0), -REAL_C(2.0))));
}

void test_cnum_magnitude_squared(void)
{
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(25.0),
                             cnum_magnitude_squared(cnum_make(REAL_C(3.0), REAL_C(4.0))));
}

void test_cnum_is_zero(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_zero(cnum_make(REAL_C(0.0), REAL_C(0.0))));
    TEST_ASSERT_EQUAL(false, cnum_is_zero(cnum_make(REAL_C(0.0), REAL_C(1.0))));
    TEST_ASSERT_EQUAL(false, cnum_is_zero(cnum_make(REAL_C(1.0), REAL_C(0.0))));
}

void test_cnum_is_equal(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_equal(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(1.0), REAL_C(2.0))));
    TEST_ASSERT_EQUAL(false, cnum_is_equal(cnum_make(REAL_C(1.0), REAL_C(2.0)), cnum_make(REAL_C(1.0), REAL_C(3.0))));
}

void test_cnum_is_near(void)
{
    cnum_t a = cnum_make(REAL_C(1.0), REAL_C(2.0));
    cnum_t b = cnum_make(REAL_C(1.001), REAL_C(2.001));

    TEST_ASSERT_EQUAL(true, cnum_is_near(a, b, REAL_C(0.01)));
    TEST_ASSERT_EQUAL(false, cnum_is_near(a, b, REAL_C(0.0001)));
    TEST_ASSERT_EQUAL(true, cnum_is_near(a, a, REAL_C(0.0)));
}

// The size of a number must not depend on whether its square fits.
//
// The size was worked out as the root of the sum of the squares, which throws
// away every number whose square the width cannot hold. At 32 bits the square
// of 6.1e-30 falls below the smallest ordinary number there is, thus the size
// of 6.1e-30 came back as NOTHING, and at the other end the square of 1e30 runs
// past the largest number the width holds and the size came back as infinity.
//
// It cost a root. poly_roots walked onto a root at 6.1e-30, asked how large the
// polynomial was there, was told nothing, and took a plain real root for a
// complex pair. A pole standing outside the circle was then missed and
// poly_is_inside_circle called an unstable filter stable.
void test_cnum_magnitude_of_a_very_small_and_a_very_large_number(void)
{
    // Small enough that its square is not an ordinary number at 32 bits.
    cnum_t small = cnum_make(REAL_C(6.1e-30), REAL_C(0.0));

    TEST_ASSERT_TRUE(cnum_magnitude(small) > REAL_C(0.0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0e-32), REAL_C(6.1e-30),
                            cnum_magnitude(small));

    // And the same the other way round, where the square would run past the
    // largest number the width holds.
    cnum_t large = cnum_make(REAL_C(0.0), REAL_C(1.0e30));

    TEST_ASSERT_TRUE(cnum_magnitude(large) < REAL_LARGEST);
    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0e28), REAL_C(1.0e30),
                            cnum_magnitude(large));

    // Both parts together, where neither square fits on its own.
    cnum_t both = cnum_make(REAL_C(3.0e-30), REAL_C(4.0e-30));

    TEST_ASSERT_REAL_WITHIN(REAL_C(1.0e-32), REAL_C(5.0e-30),
                            cnum_magnitude(both));
}
