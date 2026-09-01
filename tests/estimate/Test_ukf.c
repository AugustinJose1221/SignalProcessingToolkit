#include "unity.h"
#include "real_assert.h"
#include "ukf.h"
#include "ekf.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

void setUp(void)
{
    seed = 1u;
}

void tearDown(void)
{

}

// A model that does not bend at all: the state simply stays as it is.
static void state_stays(const matrix_t* state, const matrix_t* input,
                        matrix_t* result)
{
    (void)input;
    matrix_copy((matrix_t*)state, result);
}

// A model that is DRIVEN FROM OUTSIDE: the state grows by whatever the input
// says at each step.
//
// Every other model in this file writes (void)input and ignores it, and every
// call of ukf_step here passes NULL for the input. Between them, the input of
// the filter was never read at all.
static void driven_state(const matrix_t* state, const matrix_t* input,
                         matrix_t* result)
{
    real_t held = matrix_get_element((matrix_t*)state, 0, 0);
    real_t drive = matrix_get_element((matrix_t*)input, 0, 0);

    matrix_add_element(result, 0, 0, held + drive);
}

// A measurement that does not bend: it reads the state straight.
static void measures_the_state(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
}

// A measurement that bends sharply: it reads the SQUARE of the state.
//
// This is where a straight line laid against the model gives a wrong answer,
// and not merely a less accurate one. Put a spread through a square and its
// middle moves outwards, because the far side of the spread is squared into
// something further away than the near side is squared into. A straight line
// through the middle cannot show that at all.
static void measures_the_square(const matrix_t* state, matrix_t* result)
{
    real_t value = matrix_get_element((matrix_t*)state, 0, 0);

    matrix_add_element(result, 0, 0, value * value);
}

void test_ukf_alloc(void)
{
    ukf_t ukf = ukf_alloc(1, 2, 1);

    TEST_ASSERT_EQUAL(1, ukf.ni);
    TEST_ASSERT_EQUAL(2, ukf.nx);
    TEST_ASSERT_EQUAL(1, ukf.ny);
    TEST_ASSERT_EQUAL(true, ukf.dynamic_alloc);
    TEST_ASSERT_EQUAL(true, matrix_is_unit(&ukf.p));

    // The three numbers that place the points start at their usual values,
    // thus a caller who does not want to choose need not.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, UKF_DEFAULT_ALPHA, ukf.alpha);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, UKF_DEFAULT_BETA, ukf.beta);

    ukf_free(&ukf);
}

void test_ukf_static_alloc_takes_no_heap(void)
{
    real_t pool[UKF_MEMPOOL_SIZE(1, 2, 1)];
    ukf_t ukf = ukf_static_alloc(1, 2, 1, pool);

    TEST_ASSERT_EQUAL(false, ukf.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(pool, ukf.mempool);

    ukf_free(&ukf);
    TEST_ASSERT_EQUAL_PTR(pool, ukf.mempool);
}

void test_ukf_point_count(void)
{
    // Two for each element of the state, and one at the middle.
    TEST_ASSERT_EQUAL(3, UKF_POINT_COUNT(1));
    TEST_ASSERT_EQUAL(5, UKF_POINT_COUNT(2));
    TEST_ASSERT_EQUAL(9, UKF_POINT_COUNT(4));
}

void test_the_weights_add_up_to_one(void)
{
    // Both sets must add to one, or the middle and the spread they work out
    // would be scaled by something other than one. The weight at the middle is
    // negative for a small alpha, which is what lets so few points carry the
    // spread exactly.
    ukf_t ukf = ukf_alloc(1, 3, 1);
    uint32_t count = UKF_POINT_COUNT(3);

    real_t mean_total = REAL_C(0.0);
    real_t spread_total = REAL_C(0.0);

    for(uint32_t index = 0; index < count; index++)
    {
        mean_total += matrix_get_element(&ukf.scratch.weight_mean, index, 0);
        spread_total += matrix_get_element(&ukf.scratch.weight_spread, index, 0);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), mean_total);
    // The set for the spread differs from the set for the mean at the middle
    // point only, and by 1 less alpha squared plus beta. Thus it adds to one
    // more than that.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                            REAL_C(2.0)
                            - (UKF_DEFAULT_ALPHA * UKF_DEFAULT_ALPHA)
                            + UKF_DEFAULT_BETA,
                            spread_total);

    TEST_ASSERT_TRUE(matrix_get_element(&ukf.scratch.weight_mean, 0, 0)
                     < REAL_C(0.0));

    ukf_free(&ukf);
}

void test_the_points_carry_the_middle_and_the_spread_exactly(void)
{
    // This is what the points are FOR. Worked back out of them, the middle and
    // the spread must be the ones they were placed from. If that fails,
    // everything the filter does afterwards is built on sand.
    ukf_t ukf = ukf_alloc(1, 2, 1);

    matrix_t state = matrix_create_zero_matrix(2, 1);
    matrix_add_element(&state, 0, 0, REAL_C(3.0));
    matrix_add_element(&state, 1, 0, REAL_C(-1.5));

    matrix_t covariance = matrix_alloc(2, 2);
    matrix_add_element(&covariance, 0, 0, REAL_C(4.0));
    matrix_add_element(&covariance, 0, 1, REAL_C(1.0));
    matrix_add_element(&covariance, 1, 0, REAL_C(1.0));
    matrix_add_element(&covariance, 1, 1, REAL_C(2.0));

    ukf_set_state_matrix(&ukf, &state);
    ukf_set_covariance_matrix(&ukf, &covariance);

    matrix_t points = matrix_alloc(2, UKF_POINT_COUNT(2));
    TEST_ASSERT_EQUAL(true, ukf_place_points_into(&ukf, &points));

    uint32_t count = UKF_POINT_COUNT(2);

    // The middle, worked back out.
    for(uint32_t row = 0; row < 2u; row++)
    {
        real_t total = REAL_C(0.0);
        for(uint32_t index = 0; index < count; index++)
        {
            total += matrix_get_element(&ukf.scratch.weight_mean, index, 0)
                     * matrix_get_element(&points, row, index);
        }
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01),
                                matrix_get_element(&state, row, 0), total);
    }

    // The spread, worked back out.
    for(uint32_t i = 0; i < 2u; i++)
    {
        for(uint32_t j = 0; j < 2u; j++)
        {
            real_t total = REAL_C(0.0);
            for(uint32_t index = 0; index < count; index++)
            {
                real_t a = matrix_get_element(&points, i, index)
                           - matrix_get_element(&state, i, 0);
                real_t b = matrix_get_element(&points, j, index)
                           - matrix_get_element(&state, j, 0);
                total += matrix_get_element(&ukf.scratch.weight_spread, index, 0)
                         * a * b;
            }
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.01),
                                    matrix_get_element(&covariance, i, j),
                                    total);
        }
    }

    matrix_free(&state);
    matrix_free(&covariance);
    matrix_free(&points);
    ukf_free(&ukf);
}

void test_ukf_set_spread_refuses_what_the_width_cannot_hold(void)
{
    ukf_t ukf = ukf_alloc(1, 2, 1);

    TEST_ASSERT_EQUAL(true, ukf_set_spread(&ukf, REAL_C(0.5), REAL_C(2.0),
                                           REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, ukf_set_spread(&ukf, REAL_C(0.0), REAL_C(2.0),
                                            REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, ukf_set_spread(&ukf, REAL_C(-1.0), REAL_C(2.0),
                                            REAL_C(0.0)));
    // A kappa that cancels the state size leaves nothing to spread by.
    TEST_ASSERT_EQUAL(false, ukf_set_spread(&ukf, REAL_C(0.5), REAL_C(2.0),
                                            REAL_C(-2.0)));

    // A filter that would not change keeps what it had.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.5), ukf.alpha);

    ukf_free(&ukf);
}

void test_how_small_alpha_may_be_follows_the_width_of_the_build(void)
{
    // The weights are about one divided by the spreading, and they must add to
    // one. A small alpha therefore makes very large weights that add to a very
    // small number, and a narrow number cannot hold that sum.
#if defined(FFITT_REAL_64)
    // Sixteen digits carry the usual choice of the literature easily.
    TEST_ASSERT_EQUAL(true, ukf_is_valid_spread(3u, REAL_C(0.001), REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.001), UKF_DEFAULT_ALPHA);
#else
    // Seven digits do not. At an alpha of 0.001 the weights come out about
    // 6 percent wrong before the filter has done anything at all, thus the
    // default at this width is a hundred times larger.
    TEST_ASSERT_EQUAL(false, ukf_is_valid_spread(3u, REAL_C(0.001), REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.1), UKF_DEFAULT_ALPHA);
#endif

    // The default of the build must itself be one that the build can hold.
    TEST_ASSERT_EQUAL(true, ukf_is_valid_spread(3u, UKF_DEFAULT_ALPHA,
                                                UKF_DEFAULT_KAPPA));
}

void test_ukf_refuses_a_covariance_that_is_no_longer_a_spread(void)
{
    // Arithmetic can take a covariance out of being a real spread. The filter
    // says so rather than placing points that mean nothing.
    ukf_t ukf = ukf_alloc(1, 2, 1);

    matrix_t broken = matrix_create_zero_matrix(2, 2);
    matrix_add_element(&broken, 0, 0, REAL_C(1.0));
    matrix_add_element(&broken, 1, 1, REAL_C(-1.0));

    ukf_set_covariance_matrix(&ukf, &broken);
    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);

    matrix_t points = matrix_alloc(2, UKF_POINT_COUNT(2));

    TEST_ASSERT_EQUAL(false, ukf_place_points_into(&ukf, &points));
    TEST_ASSERT_EQUAL(false, ukf_predict(&ukf));
    TEST_ASSERT_EQUAL(false, ukf_update(&ukf));
    TEST_ASSERT_EQUAL(true, ukf.singular);

    matrix_free(&broken);
    matrix_free(&points);
    ukf_free(&ukf);
}

void test_ukf_finds_a_steady_value_under_noise(void)
{
    // The plainest thing a filter does: a value that does not move, seen
    // through a noisy sensor.
    ukf_t ukf = ukf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.0001));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);

    const real_t truth = REAL_C(7.0);

    for(uint32_t index = 0; index < 400u; index++)
    {
        matrix_add_element(&y, 0, 0, truth + (REAL_C(0.5) * rough()));
        TEST_ASSERT_EQUAL(true, ukf_step(&ukf, NULL, &y));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), truth,
                            matrix_get_element(ukf_get_state_matrix(&ukf), 0, 0));

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ukf_free(&ukf);
}

void test_ukf_agrees_with_ekf_when_the_model_is_straight(void)
{
    // Where the model does not bend, both filters are answering the same
    // question and must give the same answer. If they did not, one of them
    // would be wrong about the easy case.
    ukf_t ukf = ukf_alloc(1, 1, 1);
    ekf_t ekf = ekf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.01));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);

    ekf_set_state_function(&ekf, state_stays);
    ekf_set_measurement_function(&ekf, measures_the_state);
    ekf_set_process_noise_covariance_matrix(&ekf, &q);
    ekf_set_measurement_covariance_matrix(&ekf, &r);

    seed = 4u;
    for(uint32_t index = 0; index < 200u; index++)
    {
        real_t reading = REAL_C(5.0) + (REAL_C(0.5) * rough());

        matrix_add_element(&y, 0, 0, reading);
        ukf_step(&ukf, NULL, &y);
        ekf_step(&ekf, NULL, &y);
    }

    real_t from_ukf = matrix_get_element(ukf_get_state_matrix(&ukf), 0, 0);
    real_t from_ekf = matrix_get_element(ekf_get_state_matrix(&ekf), 0, 0);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), from_ekf, from_ukf);

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ukf_free(&ukf);
    ekf_free(&ekf);
}

void test_a_spread_put_through_a_bend_keeps_its_middle(void)
{
    // THE WHOLE REASON THIS MODULE EXISTS, measured on its own rather than
    // through a whole filter.
    //
    // A spread put through a square comes out with its middle at the middle
    // squared PLUS the spread, because the far side of the spread is squared
    // into something further away than the near side is. A straight line laid
    // against the square at the middle gives the middle squared and misses the
    // spread entirely.
    real_t middles[3] = {REAL_C(0.0), REAL_C(1.0), REAL_C(3.0)};
    real_t spreads[3] = {REAL_C(9.0), REAL_C(4.0), REAL_C(1.0)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        ukf_t ukf = ukf_alloc(1, 1, 1);

        matrix_t state = matrix_create_zero_matrix(1, 1);
        matrix_add_element(&state, 0, 0, middles[which]);
        matrix_t covariance = matrix_create_zero_matrix(1, 1);
        matrix_add_element(&covariance, 0, 0, spreads[which]);

        ukf_set_state_matrix(&ukf, &state);
        ukf_set_covariance_matrix(&ukf, &covariance);
        ukf_set_measurement_function(&ukf, measures_the_square);

        matrix_t points = matrix_alloc(1, UKF_POINT_COUNT(1));
        TEST_ASSERT_EQUAL(true, ukf_place_points_into(&ukf, &points));

        // Put every point through the square and take the weighted middle.
        real_t came_out = REAL_C(0.0);
        for(uint32_t index = 0; index < UKF_POINT_COUNT(1); index++)
        {
            real_t value = matrix_get_element(&points, 0, index);
            came_out += matrix_get_element(&ukf.scratch.weight_mean, index, 0)
                        * value * value;
        }

        real_t truth = (middles[which] * middles[which]) + spreads[which];
        real_t straight_line = middles[which] * middles[which];

        // This filter is exact.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01) * truth, truth, came_out);

        // A straight line is not, and it is wrong by the whole of the spread.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), spreads[which],
                                truth - straight_line);

        matrix_free(&state);
        matrix_free(&covariance);
        matrix_free(&points);
        ukf_free(&ukf);
    }
}

void test_a_spread_of_nothing_gives_the_plain_answer(void)
{
    // With no spread there is nothing for a bend to move, thus this filter and
    // a straight line must agree. If they did not, the extra machinery would
    // be changing an answer that was already right.
    ukf_t ukf = ukf_alloc(1, 1, 1);

    matrix_t state = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&state, 0, 0, REAL_C(3.0));
    matrix_t covariance = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&covariance, 0, 0, REAL_C(0.000001));

    ukf_set_state_matrix(&ukf, &state);
    ukf_set_covariance_matrix(&ukf, &covariance);

    matrix_t points = matrix_alloc(1, UKF_POINT_COUNT(1));
    TEST_ASSERT_EQUAL(true, ukf_place_points_into(&ukf, &points));

    real_t came_out = REAL_C(0.0);
    for(uint32_t index = 0; index < UKF_POINT_COUNT(1); index++)
    {
        real_t value = matrix_get_element(&points, 0, index);
        came_out += matrix_get_element(&ukf.scratch.weight_mean, index, 0)
                    * value * value;
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(9.0), came_out);

    matrix_free(&state);
    matrix_free(&covariance);
    matrix_free(&points);
    ukf_free(&ukf);
}

void test_ukf_needs_no_derivative_at_all(void)
{
    // A model with a condition in it has no derivative anywhere near the
    // condition. The extended filter of this library works its Jacobians out
    // by a central difference, thus it must be given a step and the answer
    // depends on it. This filter is given nothing of the kind, and its header
    // has no function to set one.
    //
    // The test holds that: the filter follows a model that is not smooth.
    ukf_t ukf = ukf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.001));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.04));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);

    seed = 21u;
    for(uint32_t index = 0; index < 300u; index++)
    {
        matrix_add_element(&y, 0, 0, REAL_C(3.0) + (REAL_C(0.2) * rough()));
        ukf_step(&ukf, NULL, &y);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(3.0),
                            matrix_get_element(ukf_get_state_matrix(&ukf), 0, 0));

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ukf_free(&ukf);
}

void test_the_covariance_falls_as_readings_arrive(void)
{
    // A filter that is learning must become more certain. A covariance that
    // grew instead would mean the filter was losing what it knew.
    ukf_t ukf = ukf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.0001));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);

    real_t at_first = matrix_get_element(ukf_get_covariance_matrix(&ukf), 0, 0);

    for(uint32_t index = 0; index < 100u; index++)
    {
        matrix_add_element(&y, 0, 0, REAL_C(1.0) + (REAL_C(0.5) * rough()));
        ukf_step(&ukf, NULL, &y);
    }

    real_t at_last = matrix_get_element(ukf_get_covariance_matrix(&ukf), 0, 0);

    TEST_ASSERT_TRUE(at_last < at_first);
    TEST_ASSERT_TRUE(at_last > REAL_C(0.0));

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ukf_free(&ukf);
}

// A state of two carried by a measurement of one, and a state of four by a
// measurement of three.
//
// EVERY OTHER TEST HERE USES A STATE AND A MEASUREMENT OF THE SAME SIZE, AND
// THAT HID A FAULT. The gain is nx by ny, thus turned round it is ny by nx.
// Those two shapes are the same only when nx equals ny, which is the easy case
// and not the usual one. The filter wrote the turned gain into a matrix of the
// wrong shape and stopped on an assertion the first time a real model was
// given to it.
static void two_states_stay(const matrix_t* state, const matrix_t* input,
                            matrix_t* result)
{
    (void)input;
    matrix_copy((matrix_t*)state, result);
}

// One measurement of a state of two: it sees the first element only.
static void sees_the_first_of_two(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
}

// Three measurements of a state of four, none of them straight.
static void sees_three_of_four(const matrix_t* state, matrix_t* result)
{
    real_t a = matrix_get_element((matrix_t*)state, 0, 0);
    real_t b = matrix_get_element((matrix_t*)state, 1, 0);
    real_t c = matrix_get_element((matrix_t*)state, 2, 0);
    real_t d = matrix_get_element((matrix_t*)state, 3, 0);

    matrix_add_element(result, 0, 0, a + (b * b));
    matrix_add_element(result, 1, 0, c * d);
    matrix_add_element(result, 2, 0, a - d);
}

void test_ukf_works_when_the_state_and_the_measurement_differ_in_size(void)
{
    // A state of two seen through one measurement.
    ukf_t small = ukf_alloc(1, 2, 1);

    matrix_t q2 = matrix_create_unit_matrix(2);
    matrix_multiply_scalar_into(&q2, REAL_C(0.001), &q2);
    matrix_t r1 = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r1, 0, 0, REAL_C(0.1));
    matrix_t y1 = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&small, two_states_stay);
    ukf_set_measurement_function(&small, sees_the_first_of_two);
    ukf_set_process_noise_covariance_matrix(&small, &q2);
    ukf_set_measurement_covariance_matrix(&small, &r1);

    seed = 3u;
    for(uint32_t index = 0; index < 200u; index++)
    {
        matrix_add_element(&y1, 0, 0, REAL_C(4.0) + (REAL_C(0.3) * rough()));
        TEST_ASSERT_EQUAL(true, ukf_step(&small, NULL, &y1));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(4.0),
                            matrix_get_element(ukf_get_state_matrix(&small), 0, 0));

    matrix_free(&q2);
    matrix_free(&r1);
    matrix_free(&y1);
    ukf_free(&small);

    // A state of four seen through three measurements, none of them straight.
    ukf_t larger = ukf_alloc(1, 4, 3);

    matrix_t q4 = matrix_create_unit_matrix(4);
    matrix_multiply_scalar_into(&q4, REAL_C(0.001), &q4);
    matrix_t r3 = matrix_create_unit_matrix(3);
    matrix_multiply_scalar_into(&r3, REAL_C(0.05), &r3);
    matrix_t y3 = matrix_create_zero_matrix(3, 1);
    matrix_t start = matrix_create_zero_matrix(4, 1);

    for(uint32_t index = 0; index < 4u; index++)
    {
        matrix_add_element(&start, index, 0, REAL_C(0.5));
    }

    ukf_set_state_function(&larger, two_states_stay);
    ukf_set_measurement_function(&larger, sees_three_of_four);
    ukf_set_process_noise_covariance_matrix(&larger, &q4);
    ukf_set_measurement_covariance_matrix(&larger, &r3);
    ukf_set_state_matrix(&larger, &start);

    seed = 11u;
    for(uint32_t index = 0; index < 300u; index++)
    {
        // What a state of 1, 1, 2, 2 would measure.
        matrix_add_element(&y3, 0, 0, REAL_C(2.0) + (REAL_C(0.1) * rough()));
        matrix_add_element(&y3, 1, 0, REAL_C(4.0) + (REAL_C(0.1) * rough()));
        matrix_add_element(&y3, 2, 0, REAL_C(-1.0) + (REAL_C(0.1) * rough()));

        TEST_ASSERT_EQUAL(true, ukf_step(&larger, NULL, &y3));
    }

    // The filter must at least reproduce the measurements it was given, which
    // is the most that can be asked when four numbers are seen through three.
    matrix_t predicted = matrix_create_zero_matrix(3, 1);
    sees_three_of_four(ukf_get_state_matrix(&larger), &predicted);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(2.0),
                            matrix_get_element(&predicted, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(4.0),
                            matrix_get_element(&predicted, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.2), REAL_C(-1.0),
                            matrix_get_element(&predicted, 2, 0));

    matrix_free(&q4);
    matrix_free(&r3);
    matrix_free(&y3);
    matrix_free(&start);
    matrix_free(&predicted);
    ukf_free(&larger);
}

// THE INPUT IS WHAT DRIVES THE STATE FROM OUTSIDE. A throttle, a heater, a
// steering angle: something the filter is told rather than something it works
// out.
void test_ukf_the_input_drives_the_state(void)
{
    ukf_t ukf = ukf_alloc(1, 1, 1);

    ukf_set_state_function(&ukf, driven_state);
    ukf_set_measurement_function(&ukf, measures_the_state);

    matrix_t x = matrix_create_zero_matrix(1, 1);

    ukf_set_state_matrix(&ukf, &x);

    matrix_t none = matrix_create_zero_matrix(1, 1);

    ukf_set_input_matrix(&ukf, &none);
    TEST_ASSERT_EQUAL(true, ukf_predict(&ukf));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            matrix_get_element(ukf_get_state_matrix(&ukf),
                                               0, 0));

    matrix_t u = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&u, 0, 0, REAL_C(2.0));

    ukf_set_input_matrix(&ukf, &u);

    TEST_ASSERT_EQUAL(true, ukf_predict(&ukf));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0),
                            matrix_get_element(ukf_get_state_matrix(&ukf),
                                               0, 0));

    TEST_ASSERT_EQUAL(true, ukf_predict(&ukf));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.0),
                            matrix_get_element(ukf_get_state_matrix(&ukf),
                                               0, 0));

    matrix_free(&x);
    matrix_free(&none);
    matrix_free(&u);
    ukf_free(&ukf);
}

// ukf_step takes the input as well, and gives it to the same place. A caller
// that used the short form rather than predict and update apart must get the
// same answer.
void test_ukf_step_carries_the_input_too(void)
{
    ukf_t apart = ukf_alloc(1, 1, 1);
    ukf_t together = ukf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.01));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t u = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&u, 0, 0, REAL_C(1.0));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_t* both[2] = {&apart, &together};

    for(uint32_t which = 0; which < 2u; which++)
    {
        ukf_set_state_function(both[which], driven_state);
        ukf_set_measurement_function(both[which], measures_the_state);
        ukf_set_process_noise_covariance_matrix(both[which], &q);
        ukf_set_measurement_covariance_matrix(both[which], &r);
    }

    for(uint32_t index = 0; index < 20u; index++)
    {
        matrix_add_element(&y, 0, 0, (real_t)index);

        ukf_set_input_matrix(&apart, &u);
        ukf_set_measurement_matrix(&apart, &y);
        TEST_ASSERT_EQUAL(true, ukf_predict(&apart));
        TEST_ASSERT_EQUAL(true, ukf_update(&apart));

        TEST_ASSERT_EQUAL(true, ukf_step(&together, &u, &y));

        TEST_ASSERT_REAL_WITHIN(
            REAL_C(0.0001),
            matrix_get_element(ukf_get_state_matrix(&apart), 0, 0),
            matrix_get_element(ukf_get_state_matrix(&together), 0, 0));
    }

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&u);
    matrix_free(&y);
    ukf_free(&apart);
    ukf_free(&together);
}

// THE GAIN SAYS HOW FAR THE FILTER MOVED FOR EACH UNIT THE MEASUREMENT
// DIFFERED. Reading it is how a caller tells a filter that is still listening
// to its measurements from one that has stopped: a gain falling towards nothing
// is a filter that has made up its mind.
void test_ukf_the_gain_says_how_much_the_measurement_was_believed(void)
{
    ukf_t ukf = ukf_alloc(1, 1, 1);

    TEST_ASSERT_EQUAL_PTR(&ukf.k, ukf_get_gain_matrix(&ukf));

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.0001));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_set_state_function(&ukf, state_stays);
    ukf_set_measurement_function(&ukf, measures_the_state);
    ukf_set_process_noise_covariance_matrix(&ukf, &q);
    ukf_set_measurement_covariance_matrix(&ukf, &r);

    matrix_add_element(&y, 0, 0, REAL_C(7.0));
    TEST_ASSERT_EQUAL(true, ukf_step(&ukf, NULL, &y));

    real_t at_first = matrix_get_element(ukf_get_gain_matrix(&ukf), 0, 0);

    // The gain is a share of the difference, thus it stands between nothing
    // and one, and at the first step the filter knows nothing and believes the
    // measurement almost entirely.
    TEST_ASSERT_TRUE(at_first > REAL_C(0.0));
    TEST_ASSERT_TRUE(at_first <= REAL_C(1.0));

    for(uint32_t index = 0; index < 200u; index++)
    {
        matrix_add_element(&y, 0, 0, REAL_C(7.0));
        TEST_ASSERT_EQUAL(true, ukf_step(&ukf, NULL, &y));
    }

    real_t settled = matrix_get_element(ukf_get_gain_matrix(&ukf), 0, 0);

    // Once it has heard the same thing two hundred times it believes each new
    // reading far less than it believed the first.
    TEST_ASSERT_TRUE(settled < at_first);
    TEST_ASSERT_TRUE(settled > REAL_C(0.0));

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ukf_free(&ukf);
}

// THERE IS NO ukf_reset, AND THIS IS WHY THERE NEED NOT BE. The sigma points
// and their weights are worked out afresh at every predict, thus the state and
// the covariance are the whole of what the filter remembers, and the caller
// sets both.
void test_ukf_putting_the_state_and_covariance_back_is_a_reset(void)
{
    ukf_t used = ukf_alloc(1, 1, 1);
    ukf_t fresh = ukf_alloc(1, 1, 1);

    matrix_t q = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&q, 0, 0, REAL_C(0.01));
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, REAL_C(0.25));
    matrix_t x = matrix_create_zero_matrix(1, 1);
    matrix_t p = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&p, 0, 0, REAL_C(1.0));
    matrix_t y = matrix_create_zero_matrix(1, 1);

    ukf_t* both[2] = {&used, &fresh};

    for(uint32_t which = 0; which < 2u; which++)
    {
        ukf_set_state_function(both[which], state_stays);
        ukf_set_measurement_function(both[which], measures_the_state);
        ukf_set_process_noise_covariance_matrix(both[which], &q);
        ukf_set_measurement_covariance_matrix(both[which], &r);
        ukf_set_state_matrix(both[which], &x);
        ukf_set_covariance_matrix(both[which], &p);
    }

    for(uint32_t index = 0; index < 200u; index++)
    {
        matrix_add_element(&y, 0, 0, REAL_C(9.0));
        TEST_ASSERT_EQUAL(true, ukf_step(&used, NULL, &y));
    }

    TEST_ASSERT_TRUE(
        REAL_ABS(matrix_get_element(ukf_get_state_matrix(&used), 0, 0)
                 - matrix_get_element(ukf_get_state_matrix(&fresh), 0, 0))
        > REAL_C(1.0));

    ukf_set_state_matrix(&used, &x);
    ukf_set_covariance_matrix(&used, &p);

    for(uint32_t index = 0; index < 50u; index++)
    {
        real_t reading = REAL_C(3.0) + ((real_t)(index % 7u) * REAL_C(0.1));

        matrix_add_element(&y, 0, 0, reading);

        TEST_ASSERT_EQUAL(true, ukf_step(&used, NULL, &y));
        TEST_ASSERT_EQUAL(true, ukf_step(&fresh, NULL, &y));

        TEST_ASSERT_EQUAL_REAL(
            matrix_get_element(ukf_get_state_matrix(&fresh), 0, 0),
            matrix_get_element(ukf_get_state_matrix(&used), 0, 0));
        TEST_ASSERT_EQUAL_REAL(
            matrix_get_element(ukf_get_covariance_matrix(&fresh), 0, 0),
            matrix_get_element(ukf_get_covariance_matrix(&used), 0, 0));
    }

    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&x);
    matrix_free(&p);
    matrix_free(&y);
    ukf_free(&used);
    ukf_free(&fresh);
}

static void ukf_edge_model(const matrix_t* state, const matrix_t* input,
                           matrix_t* result)
{
    (void)input;
    matrix_add_element(result, 0, 0, matrix_get_element(state, 0, 0));
    matrix_add_element(result, 1, 0, matrix_get_element(state, 1, 0));
}

static void ukf_edge_measurement(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element(state, 0, 0));
}

void test_a_covariance_that_is_not_a_real_spread_stops_the_filter(void)
{
    // The points are placed along a root of the covariance, and a covariance
    // that is not a real spread has no root. A doubt below nothing means
    // nothing: no direction can hold a negative amount of it.
    //
    // The filter must say so and change NOTHING, rather than place its points
    // from a root that does not exist and carry the answer forward as though
    // it did.
    ukf_t ukf = ukf_alloc(1, 2, 1);

    ukf_set_state_function(&ukf, ukf_edge_model);
    ukf_set_measurement_function(&ukf, ukf_edge_measurement);

    matrix_t state = matrix_create_zero_matrix(2, 1);
    matrix_add_element(&state, 0, 0, REAL_C(1.0));

    matrix_t covariance = matrix_create_zero_matrix(2, 2);
    matrix_add_element(&covariance, 0, 0, REAL_C(-4.0));
    matrix_add_element(&covariance, 1, 1, REAL_C(-2.0));

    matrix_t noise = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&noise, 0, 0, REAL_C(1.0));

    matrix_t measurement = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&measurement, 0, 0, REAL_C(2.0));

    ukf_set_state_matrix(&ukf, &state);
    ukf_set_covariance_matrix(&ukf, &covariance);
    ukf_set_measurement_covariance_matrix(&ukf, &noise);

    matrix_t points = matrix_alloc(2, UKF_POINT_COUNT(2));
    TEST_ASSERT_EQUAL(false, ukf_place_points_into(&ukf, &points));
    TEST_ASSERT_EQUAL(false, ukf_predict(&ukf));
    TEST_ASSERT_EQUAL(false, ukf_step(&ukf, NULL, &measurement));

    // The state is exactly where it was put.
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0),
                           matrix_get_element(ukf_get_state_matrix(&ukf), 0,
                                              0));

    matrix_free(&points);
    matrix_free(&state);
    matrix_free(&covariance);
    matrix_free(&noise);
    matrix_free(&measurement);
    ukf_free(&ukf);
}
