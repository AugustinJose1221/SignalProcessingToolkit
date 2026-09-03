// WHAT THIS FILE HOLDS, AND WHAT IT DELIBERATELY DOES NOT.
//
// nolibm.h carries a table of how far each of its functions stands from the
// system's own. A table in a comment is a claim; these tests make it a tested
// number, so that a change which quietly loses a digit stops the build.
//
// The system's functions ARE included here, and only here. That is the point:
// this file is the one place in the repository where the two are set beside
// each other. Every other file reaches the substitutes through the REAL_
// macros and never knows the difference.

#include "unity.h"
#include "nolibm.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

// How far two answers stand apart, measured against how large the answer is.
static double how_far(double got, double want)
{
    double apart = fabs(got - want);

    return (fabs(want) > 1e-300) ? (apart / fabs(want)) : apart;
}

static double worst_over(double from, double until, uint32_t steps,
                         double (*ours)(double), double (*theirs)(double))
{
    double worst = 0.0;

    for(uint32_t step = 0; step <= steps; step++)
    {
        double at = from + ((until - from) * (double)step / (double)steps);
        double apart = how_far(ours(at), theirs(at));

        if(apart > worst)
        {
            worst = apart;
        }
    }

    return worst;
}

#define STEPS   20000u

void test_nolibm_the_square_root_is_right_to_the_last_bit(void)
{
    TEST_ASSERT_TRUE(worst_over(1e-6, 1e6, STEPS, nolibm_sqrt, sqrt) < 1e-15);
    TEST_ASSERT_TRUE(nolibm_sqrt(0.0) == 0.0);
    TEST_ASSERT_TRUE(nolibm_sqrt(-1.0) != nolibm_sqrt(-1.0));
}

void test_nolibm_the_logarithm_and_the_exponential_hold_their_table(void)
{
    TEST_ASSERT_TRUE(worst_over(1e-6, 1e6, STEPS, nolibm_log, log) < 1e-14);
    TEST_ASSERT_TRUE(worst_over(1e-6, 1e6, STEPS, nolibm_log10, log10) < 1e-14);
    TEST_ASSERT_TRUE(worst_over(-20.0, 20.0, STEPS, nolibm_exp, exp) < 1e-10);

    // The edges the table does not cover.
    TEST_ASSERT_TRUE(nolibm_exp(-1000.0) == 0.0);
    TEST_ASSERT_TRUE(nolibm_log(-1.0) != nolibm_log(-1.0));
    TEST_ASSERT_TRUE(nolibm_log(0.0) < -1e300);
}

void test_nolibm_the_trigonometry_holds_its_table(void)
{
    TEST_ASSERT_TRUE(worst_over(-100.0, 100.0, STEPS, nolibm_sin, sin) < 1e-8);
    TEST_ASSERT_TRUE(worst_over(-100.0, 100.0, STEPS, nolibm_cos, cos) < 1e-9);
    TEST_ASSERT_TRUE(worst_over(-1.5, 1.5, STEPS, nolibm_tan, tan) < 1e-14);
    TEST_ASSERT_TRUE(worst_over(-100.0, 100.0, STEPS, nolibm_atan, atan) < 1e-11);
    TEST_ASSERT_TRUE(worst_over(-0.999, 0.999, STEPS, nolibm_asin, asin) < 1e-11);
}

void test_nolibm_the_hyperbolic_functions_hold_their_table(void)
{
    TEST_ASSERT_TRUE(worst_over(-10.0, 10.0, STEPS, nolibm_sinh, sinh) < 1e-10);
    TEST_ASSERT_TRUE(worst_over(-10.0, 10.0, STEPS, nolibm_cosh, cosh) < 1e-10);
    TEST_ASSERT_TRUE(worst_over(-100.0, 100.0, STEPS, nolibm_asinh, asinh) < 1e-12);
    TEST_ASSERT_TRUE(worst_over(1.0, 100.0, STEPS, nolibm_acosh, acosh) < 1e-13);
    TEST_ASSERT_TRUE(nolibm_acosh(0.5) != nolibm_acosh(0.5));
}

void test_nolibm_the_error_function_holds_its_table(void)
{
    TEST_ASSERT_TRUE(worst_over(-4.0, 4.0, STEPS, nolibm_erf, erf) < 2e-7);

    // Near nothing the answer is small, and a share of a small answer must
    // still be right. That is why a series is used there and not the
    // approximation, and this is the case that showed it: measured at 3 parts
    // in a thousand before the series went in.
    TEST_ASSERT_TRUE(worst_over(-0.2, 0.2, STEPS, nolibm_erf, erf) < 1e-9);
}

// THE REMAINDER IS EXACT, AND THIS IS THE CASE THAT PROVED IT WAS NOT.
//
// Worked out as x minus the whole part of x over y, times y, the division
// rounds, and where it rounds across a whole number the answer is out by a
// whole divisor. fmod(-87.6, 7.3) gave 0 where the truth is -7.3.
void test_nolibm_the_remainder_is_exact_even_where_the_division_rounds(void)
{
    TEST_ASSERT_TRUE(fabs(nolibm_fmod(-87.6, 7.3) - fmod(-87.6, 7.3)) < 1e-12);

    double worst = 0.0;

    for(uint32_t step = 0; step <= STEPS; step++)
    {
        double at = -100.0 + (200.0 * (double)step / (double)STEPS);
        double apart = fabs(nolibm_fmod(at, 7.3) - fmod(at, 7.3));

        if(apart > worst)
        {
            worst = apart;
        }
    }

    TEST_ASSERT_TRUE(worst == 0.0);
    TEST_ASSERT_TRUE(nolibm_fmod(1.0, 0.0) != nolibm_fmod(1.0, 0.0));
}

void test_nolibm_the_whole_number_functions_are_exact(void)
{
    for(uint32_t step = 0; step <= 4000u; step++)
    {
        double at = -100.0 + (200.0 * (double)step / 4000.0);

        TEST_ASSERT_TRUE(nolibm_floor(at) == floor(at));
        TEST_ASSERT_TRUE(nolibm_ceil(at) == ceil(at));
    }

    TEST_ASSERT_TRUE(nolibm_fabs(-3.5) == 3.5);
    TEST_ASSERT_TRUE(nolibm_fabs(3.5) == 3.5);
}

void test_nolibm_the_two_argument_functions_hold_their_table(void)
{
    double worst = 0.0;

    for(uint32_t step = 0; step <= STEPS; step++)
    {
        double at = 0.001 + (999.999 * (double)step / (double)STEPS);
        double apart = how_far(nolibm_hypot(at, at * 0.3),
                               hypot(at, at * 0.3));

        if(apart > worst) { worst = apart; }
    }
    TEST_ASSERT_TRUE(worst < 1e-15);

    worst = 0.0;
    for(uint32_t step = 0; step <= STEPS; step++)
    {
        double at = 0.01 + (99.99 * (double)step / (double)STEPS);
        double apart = how_far(nolibm_pow(at, 2.7), pow(at, 2.7));

        if(apart > worst) { worst = apart; }
    }
    TEST_ASSERT_TRUE(worst < 1e-10);

    worst = 0.0;
    for(uint32_t step = 0; step <= STEPS; step++)
    {
        double at = -10.0 + (20.0 * (double)step / (double)STEPS);
        double apart = how_far(nolibm_atan2(at, 3.0), atan2(at, 3.0));

        if(apart > worst) { worst = apart; }
    }
    TEST_ASSERT_TRUE(worst < 1e-11);

    // A power of nothing, and a negative base raised to a whole power, which
    // are the two cases the plain road cannot take.
    TEST_ASSERT_TRUE(nolibm_pow(5.0, 0.0) == 1.0);
    TEST_ASSERT_TRUE(fabs(nolibm_pow(-2.0, 3.0) + 8.0) < 1e-12);
    TEST_ASSERT_TRUE(fabs(nolibm_pow(-2.0, 2.0) - 4.0) < 1e-12);
    TEST_ASSERT_TRUE(nolibm_pow(-2.0, 0.5) != nolibm_pow(-2.0, 0.5));

    // Every quarter of the turn, so that no sign is the wrong way round.
    //
    // HELD AT WHAT THE TABLE PROMISES AND NOT TIGHTER. These were asked for
    // within a thousandth of a millionth of a millionth first, which is finer
    // than nolibm.h claims for the arc tangent, and the turn at a quarter of
    // pi missed it by half again. A test that asks for more than the header
    // sold is a test that will fail for being right.
    TEST_ASSERT_TRUE(fabs(nolibm_atan2(0.0, 1.0) - atan2(0.0, 1.0)) < 1e-11);
    TEST_ASSERT_TRUE(fabs(nolibm_atan2(1.0, 0.0) - atan2(1.0, 0.0)) < 1e-11);
    TEST_ASSERT_TRUE(fabs(nolibm_atan2(-1.0, 0.0) - atan2(-1.0, 0.0)) < 1e-11);
    TEST_ASSERT_TRUE(fabs(nolibm_atan2(1.0, -1.0) - atan2(1.0, -1.0)) < 1e-11);
    TEST_ASSERT_TRUE(fabs(nolibm_atan2(-1.0, -1.0) - atan2(-1.0, -1.0)) < 1e-11);
    TEST_ASSERT_TRUE(nolibm_atan2(0.0, 0.0) == 0.0);
}

// THE EDGES, WHICH ARE THE WHOLE REASON A GUARD IS THERE.
//
// A caller reaches these when something has already gone wrong somewhere else:
// a division that gave an endless number, a root of a negative, a reading that
// came back as nothing at all. What matters is that each answers the way the
// system's own function answers, so that a program which handled the one
// handles the other.
void test_nolibm_the_edges_answer_the_way_the_system_answers(void)
{
    double nothing = 0.0;
    double endless = 1.0 / nothing;
    double not_a_number = nothing / nothing;

    // A number that is not a number goes in and comes out.
    TEST_ASSERT_TRUE(nolibm_sqrt(not_a_number) != nolibm_sqrt(not_a_number));
    TEST_ASSERT_TRUE(nolibm_exp(not_a_number) != nolibm_exp(not_a_number));
    TEST_ASSERT_TRUE(nolibm_log(not_a_number) != nolibm_log(not_a_number));
    TEST_ASSERT_TRUE(nolibm_sin(not_a_number) != nolibm_sin(not_a_number));
    TEST_ASSERT_TRUE(nolibm_cos(not_a_number) != nolibm_cos(not_a_number));
    TEST_ASSERT_TRUE(nolibm_atan(not_a_number) != nolibm_atan(not_a_number));
    TEST_ASSERT_TRUE(nolibm_erf(not_a_number) != nolibm_erf(not_a_number));
    TEST_ASSERT_TRUE(nolibm_fmod(not_a_number, 2.0)
                     != nolibm_fmod(not_a_number, 2.0));
    TEST_ASSERT_TRUE(nolibm_fmod(2.0, not_a_number)
                     != nolibm_fmod(2.0, not_a_number));

    // An endless number, where the answer is endless too.
    TEST_ASSERT_TRUE(nolibm_sqrt(endless) > 1e300);
    TEST_ASSERT_TRUE(nolibm_log(endless) > 1e300);
    TEST_ASSERT_TRUE(nolibm_floor(endless) > 1e300);
    TEST_ASSERT_TRUE(nolibm_ceil(endless) > 1e300);
    TEST_ASSERT_TRUE(nolibm_exp(800.0) > 1e300);

    // An angle so large that no reduction of it means anything. The system
    // gives a number that is not a number and so does this.
    TEST_ASSERT_TRUE(nolibm_sin(endless) != nolibm_sin(endless));
    TEST_ASSERT_TRUE(nolibm_cos(endless) != nolibm_cos(endless));
    TEST_ASSERT_TRUE(nolibm_fmod(endless, 3.0) != nolibm_fmod(endless, 3.0));

    // A power of nothing, from both sides.
    TEST_ASSERT_TRUE(nolibm_pow(0.0, 2.0) == 0.0);
    TEST_ASSERT_TRUE(nolibm_pow(0.0, -2.0) > 1e300);

    // A tangent where the cosine is nothing.
    TEST_ASSERT_TRUE(nolibm_tan(nolibm_asin(1.0)) > 1e15);

    // The arc sine at its two ends and past them.
    TEST_ASSERT_TRUE(fabs(nolibm_asin(1.0) - asin(1.0)) < 1e-12);
    TEST_ASSERT_TRUE(fabs(nolibm_asin(-1.0) - asin(-1.0)) < 1e-12);
    TEST_ASSERT_TRUE(nolibm_asin(1.5) != nolibm_asin(1.5));

    // The error function far out, where it is one either way.
    TEST_ASSERT_TRUE(nolibm_erf(8.0) == 1.0);
    TEST_ASSERT_TRUE(nolibm_erf(-8.0) == -1.0);

    // A remainder smaller than its divisor comes back as it stands.
    TEST_ASSERT_TRUE(nolibm_fmod(2.0, 7.0) == 2.0);

    // And a whole number too large to hold a fraction is already whole.
    TEST_ASSERT_TRUE(nolibm_floor(1e17) == 1e17);
    TEST_ASSERT_TRUE(nolibm_ceil(1e17) == 1e17);
}
