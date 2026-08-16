#include "unity.h"
#include "cnum.h"
#include <stdlib.h>

#define TOLERANCE   0.0001f

void setUp(void)
{

}

void tearDown(void)
{

}

void test_cnum_make(void)
{
    cnum_t number = cnum_make(1.5f, -2.5f);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, number.re);
    TEST_ASSERT_EQUAL_FLOAT(-2.5f, number.im);
}

void test_cnum_from_real(void)
{
    cnum_t number = cnum_from_real(4.0f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, number.re);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, number.im);
}

void test_cnum_zero_and_one(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_zero(cnum_zero()));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, cnum_one().re);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, cnum_one().im);
}

void test_cnum_add(void)
{
    cnum_t sum = cnum_add(cnum_make(1.0f, 2.0f), cnum_make(3.0f, -5.0f));
    TEST_ASSERT_EQUAL_FLOAT(4.0f, sum.re);
    TEST_ASSERT_EQUAL_FLOAT(-3.0f, sum.im);
}

void test_cnum_subtract(void)
{
    cnum_t difference = cnum_subtract(cnum_make(1.0f, 2.0f), cnum_make(3.0f, -5.0f));
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, difference.re);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, difference.im);
}

void test_cnum_multiply(void)
{
    // (1 + 2i)(3 + 4i) = -5 + 10i
    cnum_t product = cnum_multiply(cnum_make(1.0f, 2.0f), cnum_make(3.0f, 4.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -5.0f, product.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 10.0f, product.im);
}

void test_cnum_multiply_by_the_imaginary_unit_turns_the_number(void)
{
    // i times i is -1.
    cnum_t unit = cnum_make(0.0f, 1.0f);
    cnum_t product = cnum_multiply(unit, unit);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, product.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, product.im);
}

void test_cnum_divide(void)
{
    // (1 + 2i)/(3 + 4i) = 0.44 + 0.08i
    cnum_t quotient = cnum_divide(cnum_make(1.0f, 2.0f), cnum_make(3.0f, 4.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.44f, quotient.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.08f, quotient.im);
}

void test_cnum_divide_by_zero_gives_zero(void)
{
    cnum_t quotient = cnum_divide(cnum_make(1.0f, 2.0f), cnum_zero());
    TEST_ASSERT_EQUAL(true, cnum_is_zero(quotient));
}

void test_cnum_multiply_and_divide_are_opposite_operations(void)
{
    cnum_t a = cnum_make(2.5f, -1.5f);
    cnum_t b = cnum_make(-0.5f, 3.0f);
    cnum_t back = cnum_divide(cnum_multiply(a, b), b);
    TEST_ASSERT_EQUAL(true, cnum_is_near(a, back, 0.001f));
}

void test_cnum_scale(void)
{
    cnum_t scaled = cnum_scale(cnum_make(1.0f, -2.0f), 3.0f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, scaled.re);
    TEST_ASSERT_EQUAL_FLOAT(-6.0f, scaled.im);
}

void test_cnum_conjugate(void)
{
    cnum_t conjugate = cnum_conjugate(cnum_make(1.0f, -2.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, conjugate.re);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, conjugate.im);
}

void test_cnum_a_number_times_its_conjugate_is_real(void)
{
    cnum_t a = cnum_make(3.0f, 4.0f);
    cnum_t product = cnum_multiply(a, cnum_conjugate(a));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 25.0f, product.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, product.im);
}

void test_cnum_negate(void)
{
    cnum_t negative = cnum_negate(cnum_make(1.0f, -2.0f));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, negative.re);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, negative.im);
}

void test_cnum_real_and_imaginary(void)
{
    cnum_t number = cnum_make(7.0f, -8.0f);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, cnum_real(number));
    TEST_ASSERT_EQUAL_FLOAT(-8.0f, cnum_imaginary(number));
}

void test_cnum_magnitude(void)
{
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, cnum_magnitude(cnum_make(3.0f, 4.0f)));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, cnum_magnitude(cnum_zero()));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, cnum_magnitude(cnum_make(0.0f, -2.0f)));
}

void test_cnum_magnitude_squared(void)
{
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 25.0f,
                             cnum_magnitude_squared(cnum_make(3.0f, 4.0f)));
}

void test_cnum_is_zero(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_zero(cnum_make(0.0f, 0.0f)));
    TEST_ASSERT_EQUAL(false, cnum_is_zero(cnum_make(0.0f, 1.0f)));
    TEST_ASSERT_EQUAL(false, cnum_is_zero(cnum_make(1.0f, 0.0f)));
}

void test_cnum_is_equal(void)
{
    TEST_ASSERT_EQUAL(true, cnum_is_equal(cnum_make(1.0f, 2.0f), cnum_make(1.0f, 2.0f)));
    TEST_ASSERT_EQUAL(false, cnum_is_equal(cnum_make(1.0f, 2.0f), cnum_make(1.0f, 3.0f)));
}

void test_cnum_is_near(void)
{
    cnum_t a = cnum_make(1.0f, 2.0f);
    cnum_t b = cnum_make(1.001f, 2.001f);

    TEST_ASSERT_EQUAL(true, cnum_is_near(a, b, 0.01f));
    TEST_ASSERT_EQUAL(false, cnum_is_near(a, b, 0.0001f));
    TEST_ASSERT_EQUAL(true, cnum_is_near(a, a, 0.0f));
}
