#include "unity.h"
#include "real_assert.h"
#include "real.h"
#include <float.h>
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_real_holds_the_width_that_the_build_asked_for(void)
{
#if defined(SPTK_REAL_64)
    TEST_ASSERT_EQUAL(8, (int)sizeof(real_t));
    TEST_ASSERT_EQUAL(DBL_DIG, REAL_DIGITS);
#else
    TEST_ASSERT_EQUAL(4, (int)sizeof(real_t));
    TEST_ASSERT_EQUAL(FLT_DIG, REAL_DIGITS);
#endif
}

void test_real_holds_more_digits_at_64_bits_than_at_32(void)
{
    // The whole reason a caller would choose 64 bits.
#if defined(SPTK_REAL_64)
    TEST_ASSERT_TRUE(REAL_DIGITS >= 15);
    TEST_ASSERT_TRUE(REAL_EPSILON < 1.0e-15);
#else
    TEST_ASSERT_TRUE(REAL_DIGITS >= 6);
    TEST_ASSERT_TRUE(REAL_EPSILON < 1.0e-6);
#endif
}

void test_a_number_written_with_real_c_keeps_every_digit_of_the_build(void)
{
    // This is what REAL_C is for. A number written as 0.1f is rounded to seven
    // digits before it is ever used, thus a 64 bit build that spelled it that
    // way would hold seven digits and say it held sixteen.
    real_t tenth = REAL_C(0.1);

#if defined(SPTK_REAL_64)
    // The nearest double to a tenth is nearer than the nearest float is.
    TEST_ASSERT_TRUE(fabs((double)tenth - 0.1) < 1.0e-16);
#else
    TEST_ASSERT_TRUE(fabs((double)tenth - 0.1) < 1.0e-7);
#endif
}

void test_real_pi_is_at_the_width_of_the_build(void)
{
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.14159265), REAL_PI);
    // The sine of pi is nothing, which holds however wide the number is.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), REAL_SIN(REAL_PI));
}

void test_the_macros_of_mathematics_give_the_right_answers(void)
{
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), REAL_SQRT(REAL_C(9.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), REAL_COS(REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), REAL_SIN(REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.5), REAL_ABS(REAL_C(-2.5)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(8.0),
                            REAL_POW(REAL_C(2.0), REAL_C(3.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), REAL_EXP(REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), REAL_LOG(REAL_C(1.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0), REAL_LOG10(REAL_C(100.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), REAL_FLOOR(REAL_C(3.7)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.0), REAL_CEIL(REAL_C(3.2)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), REAL_FMOD(REAL_C(7.0),
                                                                  REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_PI / REAL_C(4.0),
                            REAL_ATAN2(REAL_C(1.0), REAL_C(1.0)));
}

void test_the_functions_of_mathematics_agree_with_the_macros(void)
{
    // The functions exist so that something can be GIVEN a function, which a
    // macro has no address for. They must give the same answer as the macros.
    real_t x = REAL_C(0.7);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_SIN(x), real_sin(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_COS(x), real_cos(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_TAN(x), real_tan(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_SQRT(x), real_sqrt(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_EXP(x), real_exp(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_LOG(x), real_log(x));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0000001), REAL_ABS(-x), real_abs(-x));
}

void test_the_functions_of_mathematics_can_be_given_away(void)
{
    // The reason they exist at all: a pointer to one must agree with real_t,
    // which a pointer to sinf does not in a 64 bit build.
    real_t (*chosen)(real_t) = real_sin;

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), chosen(REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), chosen(REAL_PI / REAL_C(2.0)));

    chosen = real_cos;
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), chosen(REAL_C(0.0)));
}

void test_a_sum_shows_the_width_of_the_build(void)
{
    // The measurement that decides which width a piece of work needs. Adding a
    // small number to a large one keeps it at 64 bits and loses it at 32.
    real_t large = REAL_C(8000000.0);
    real_t total = large + REAL_C(0.5);

#if defined(SPTK_REAL_64)
    TEST_ASSERT_TRUE((total - large) > REAL_C(0.4));
#else
    // At 32 bits one step beside eight million is 0.5, thus a half is at the
    // very edge of what can be held and anything smaller is lost completely.
    real_t smaller = large + REAL_C(0.01);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), smaller - large);
#endif
}
