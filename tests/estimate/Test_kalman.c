#include "unity.h"
#include "real_assert.h"
#include "kalman.h"
#include "matrix.h"
#include <stdlib.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

static matrix_t make_matrix(uint32_t m, uint32_t n, real_t* values)
{
    matrix_t matrix = matrix_alloc(m, n);
    for(uint32_t i = 0; i < m; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            matrix_add_element(&matrix, i, j, values[(i*n)+j]);
        }
    }
    return matrix;
}

// A filter for a constant value. The state holds one element, and the
// observation matrix reads that element directly.
static kalman_t make_scalar_filter(real_t initial_state, real_t initial_covariance,
                                   real_t process_noise, real_t measurement_noise)
{
    kalman_t kalman = kalman_alloc(1, 1, 1);

    real_t state[1] = {initial_state};
    real_t transition[1] = {REAL_C(1.0)};
    real_t control[1] = {REAL_C(0.0)};
    real_t covariance[1] = {initial_covariance};
    real_t process[1] = {process_noise};
    real_t measurement[1] = {measurement_noise};
    real_t observation[1] = {REAL_C(1.0)};
    real_t input[1] = {REAL_C(0.0)};

    matrix_t x = make_matrix(1, 1, state);
    matrix_t a = make_matrix(1, 1, transition);
    matrix_t b = make_matrix(1, 1, control);
    matrix_t p = make_matrix(1, 1, covariance);
    matrix_t q = make_matrix(1, 1, process);
    matrix_t r = make_matrix(1, 1, measurement);
    matrix_t c = make_matrix(1, 1, observation);
    matrix_t u = make_matrix(1, 1, input);

    kalman_set_state_matrix(&kalman, &x);
    kalman_set_state_transition_matrix(&kalman, &a);
    kalman_set_control_matrix(&kalman, &b);
    kalman_set_covariance_matrix(&kalman, &p);
    kalman_set_process_noise_covariance_matrix(&kalman, &q);
    kalman_set_measurement_covariance_matrix(&kalman, &r);
    kalman_set_observation_matrix(&kalman, &c);
    kalman_set_input_matrix(&kalman, &u);

    matrix_free(&x);
    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&c);
    matrix_free(&u);

    return kalman;
}

void test_kalman_alloc(void)
{
    kalman_t kalman = kalman_alloc(1, 2, 3);

    TEST_ASSERT_EQUAL(1, kalman.ni);
    TEST_ASSERT_EQUAL(2, kalman.nx);
    TEST_ASSERT_EQUAL(3, kalman.ny);
    TEST_ASSERT_EQUAL(true, kalman.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(kalman.mempool);

    kalman_free(&kalman);
}

void test_kalman_alloc_gives_matrices_of_the_correct_order(void)
{
    kalman_t kalman = kalman_alloc(2, 4, 3);

    TEST_ASSERT_EQUAL(4, kalman._x.m);
    TEST_ASSERT_EQUAL(1, kalman._x.n);
    TEST_ASSERT_EQUAL(4, kalman.x.m);
    TEST_ASSERT_EQUAL(1, kalman.x.n);
    TEST_ASSERT_EQUAL(3, kalman.y.m);
    TEST_ASSERT_EQUAL(1, kalman.y.n);
    TEST_ASSERT_EQUAL(2, kalman.u.m);
    TEST_ASSERT_EQUAL(1, kalman.u.n);
    TEST_ASSERT_EQUAL(4, kalman.a.m);
    TEST_ASSERT_EQUAL(4, kalman.a.n);
    TEST_ASSERT_EQUAL(4, kalman.b.m);
    TEST_ASSERT_EQUAL(2, kalman.b.n);
    TEST_ASSERT_EQUAL(4, kalman.p.m);
    TEST_ASSERT_EQUAL(4, kalman.p.n);
    TEST_ASSERT_EQUAL(4, kalman.q.m);
    TEST_ASSERT_EQUAL(4, kalman.q.n);
    TEST_ASSERT_EQUAL(3, kalman.r.m);
    TEST_ASSERT_EQUAL(3, kalman.r.n);
    TEST_ASSERT_EQUAL(3, kalman.c.m);
    TEST_ASSERT_EQUAL(4, kalman.c.n);
    TEST_ASSERT_EQUAL(4, kalman.k.m);
    TEST_ASSERT_EQUAL(3, kalman.k.n);

    kalman_free(&kalman);
}

void test_kalman_alloc_clears_all_matrices(void)
{
    kalman_t kalman = kalman_alloc(1, 2, 1);

    TEST_ASSERT_EQUAL(true, matrix_is_zero(&kalman.x));
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&kalman.p));
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&kalman.a));
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&kalman.k));

    kalman_free(&kalman);
}

void test_kalman_static_alloc(void)
{
    real_t mempool[KALMAN_MEMPOOL_SIZE(1, 2, 1)];
    kalman_t kalman = kalman_static_alloc(1, 2, 1, mempool);

    TEST_ASSERT_EQUAL(1, kalman.ni);
    TEST_ASSERT_EQUAL(2, kalman.nx);
    TEST_ASSERT_EQUAL(1, kalman.ny);
    TEST_ASSERT_EQUAL(false, kalman.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(mempool, kalman.mempool);
    TEST_ASSERT_EQUAL_PTR(mempool, kalman._x.elem);
}

void test_kalman_static_alloc_stays_inside_the_memory_pool(void)
{
    // The last scratch matrix must end at the last element of the pool.
    real_t mempool[KALMAN_MEMPOOL_SIZE(2, 3, 2)];
    kalman_t kalman = kalman_static_alloc(2, 3, 2, mempool);

    real_t* end = kalman.scratch.ny1_b.elem + (kalman.scratch.ny1_b.m * kalman.scratch.ny1_b.n);

    TEST_ASSERT_EQUAL_PTR(mempool + KALMAN_MEMPOOL_SIZE(2, 3, 2), end);
}

void test_kalman_free_keeps_a_static_filter(void)
{
    real_t mempool[KALMAN_MEMPOOL_SIZE(1, 1, 1)];
    kalman_t kalman = kalman_static_alloc(1, 1, 1, mempool);

    kalman_free(&kalman);

    TEST_ASSERT_EQUAL_PTR(mempool, kalman.mempool);
    TEST_ASSERT_EQUAL(false, kalman.dynamic_alloc);
}

void test_kalman_free_releases_a_dynamic_filter(void)
{
    kalman_t kalman = kalman_alloc(1, 1, 1);

    kalman_free(&kalman);

    TEST_ASSERT_NULL(kalman.mempool);
    TEST_ASSERT_EQUAL(false, kalman.dynamic_alloc);

    // A second call must do nothing.
    kalman_free(&kalman);
    TEST_ASSERT_NULL(kalman.mempool);
}

void test_kalman_setters(void)
{
    kalman_t kalman = kalman_alloc(1, 2, 1);

    real_t transition[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(0.0), REAL_C(1.0)};
    real_t observation[2] = {REAL_C(1.0), REAL_C(0.0)};
    real_t state[2] = {REAL_C(5.0), -REAL_C(2.0)};

    matrix_t a = make_matrix(2, 2, transition);
    matrix_t c = make_matrix(1, 2, observation);
    matrix_t x = make_matrix(2, 1, state);

    kalman_set_state_transition_matrix(&kalman, &a);
    kalman_set_observation_matrix(&kalman, &c);
    kalman_set_state_matrix(&kalman, &x);

    TEST_ASSERT_EQUAL(true, matrix_is_equal(&a, &kalman.a));
    TEST_ASSERT_EQUAL(true, matrix_is_equal(&c, &kalman.c));
    TEST_ASSERT_EQUAL(true, matrix_is_equal(&x, &kalman.x));
    // The setter fills the previous state as well.
    TEST_ASSERT_EQUAL(true, matrix_is_equal(&x, &kalman._x));

    matrix_free(&a);
    matrix_free(&c);
    matrix_free(&x);
    kalman_free(&kalman);
}

void test_kalman_predict_holds_a_constant_state(void)
{
    // A = 1, B = 0, Q = 0. Thus the state does not change, and the covariance
    // does not grow.
    kalman_t kalman = make_scalar_filter(REAL_C(3.0), REAL_C(1.0), REAL_C(0.0), REAL_C(1.0));

    kalman_predict(&kalman);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), matrix_get_element(&kalman.x, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.p, 0, 0));

    kalman_free(&kalman);
}

void test_kalman_predict_adds_the_process_noise(void)
{
    // Q = 0.5. Each predict step adds 0.5 to the covariance.
    kalman_t kalman = make_scalar_filter(REAL_C(0.0), REAL_C(1.0), REAL_C(0.5), REAL_C(1.0));

    kalman_predict(&kalman);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.5), matrix_get_element(&kalman.p, 0, 0));

    kalman_predict(&kalman);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), matrix_get_element(&kalman.p, 0, 0));

    kalman_free(&kalman);
}

void test_kalman_predict_uses_the_control_input(void)
{
    kalman_t kalman = kalman_alloc(1, 1, 1);

    real_t state[1] = {REAL_C(0.0)};
    real_t transition[1] = {REAL_C(1.0)};
    real_t control[1] = {REAL_C(2.0)};
    real_t input[1] = {REAL_C(3.0)};
    real_t covariance[1] = {REAL_C(1.0)};

    matrix_t x = make_matrix(1, 1, state);
    matrix_t a = make_matrix(1, 1, transition);
    matrix_t b = make_matrix(1, 1, control);
    matrix_t u = make_matrix(1, 1, input);
    matrix_t p = make_matrix(1, 1, covariance);

    kalman_set_state_matrix(&kalman, &x);
    kalman_set_state_transition_matrix(&kalman, &a);
    kalman_set_control_matrix(&kalman, &b);
    kalman_set_input_matrix(&kalman, &u);
    kalman_set_covariance_matrix(&kalman, &p);

    kalman_predict(&kalman);

    // x = 1*0 + 2*3 = 6
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(6.0), matrix_get_element(&kalman.x, 0, 0));

    matrix_free(&x);
    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&u);
    matrix_free(&p);
    kalman_free(&kalman);
}

void test_kalman_update_gives_the_known_gain_and_state(void)
{
    // P = 1, R = 1, C = 1. Thus S = 2, K = 0.5.
    // The measurement is 2, and the state starts at 0. Thus x becomes 1 and
    // P becomes 0.5.
    kalman_t kalman = make_scalar_filter(REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(1.0));

    real_t measurement[1] = {REAL_C(2.0)};
    matrix_t y = make_matrix(1, 1, measurement);
    kalman_set_measurement_matrix(&kalman, &y);

    TEST_ASSERT_EQUAL(true, kalman_update(&kalman));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.5), matrix_get_element(&kalman.k, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.x, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.5), matrix_get_element(&kalman.p, 0, 0));
    TEST_ASSERT_EQUAL(false, kalman.singular);

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_update_trusts_an_exact_measurement(void)
{
    // R = 0. The measurement has no noise. Thus the gain is 1, and the state
    // takes the value of the measurement.
    kalman_t kalman = make_scalar_filter(REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0));

    real_t measurement[1] = {REAL_C(7.0)};
    matrix_t y = make_matrix(1, 1, measurement);
    kalman_set_measurement_matrix(&kalman, &y);

    TEST_ASSERT_EQUAL(true, kalman_update(&kalman));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.k, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), matrix_get_element(&kalman.x, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&kalman.p, 0, 0));

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_update_keeps_the_state_for_a_certain_estimate(void)
{
    // P = 0. The estimate has no doubt. Thus the gain is 0, and the
    // measurement does not change the state.
    kalman_t kalman = make_scalar_filter(REAL_C(4.0), REAL_C(0.0), REAL_C(0.0), REAL_C(1.0));

    real_t measurement[1] = {REAL_C(100.0)};
    matrix_t y = make_matrix(1, 1, measurement);
    kalman_set_measurement_matrix(&kalman, &y);

    TEST_ASSERT_EQUAL(true, kalman_update(&kalman));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&kalman.k, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0), matrix_get_element(&kalman.x, 0, 0));

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_update_reports_a_singular_innovation_covariance(void)
{
    // P = 0 and R = 0. Thus S = 0, and the filter cannot invert S.
    kalman_t kalman = make_scalar_filter(REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0));

    real_t measurement[1] = {REAL_C(5.0)};
    matrix_t y = make_matrix(1, 1, measurement);
    kalman_set_measurement_matrix(&kalman, &y);

    TEST_ASSERT_EQUAL(false, kalman_update(&kalman));
    TEST_ASSERT_EQUAL(true, kalman.singular);
    // The state does not change.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.x, 0, 0));

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_two_dimensional_predict_and_update(void)
{
    // A constant velocity model. The state holds the position and the speed.
    // The filter reads the position only.
    kalman_t kalman = kalman_alloc(1, 2, 1);

    real_t state[2] = {REAL_C(0.0), REAL_C(0.0)};
    real_t transition[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(0.0), REAL_C(1.0)};
    real_t control[2] = {REAL_C(0.0), REAL_C(0.0)};
    real_t input[1] = {REAL_C(0.0)};
    real_t covariance[4] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(1.0)};
    real_t process[4] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    real_t measurement_noise[1] = {REAL_C(1.0)};
    real_t observation[2] = {REAL_C(1.0), REAL_C(0.0)};

    matrix_t x = make_matrix(2, 1, state);
    matrix_t a = make_matrix(2, 2, transition);
    matrix_t b = make_matrix(2, 1, control);
    matrix_t u = make_matrix(1, 1, input);
    matrix_t p = make_matrix(2, 2, covariance);
    matrix_t q = make_matrix(2, 2, process);
    matrix_t r = make_matrix(1, 1, measurement_noise);
    matrix_t c = make_matrix(1, 2, observation);

    kalman_set_state_matrix(&kalman, &x);
    kalman_set_state_transition_matrix(&kalman, &a);
    kalman_set_control_matrix(&kalman, &b);
    kalman_set_input_matrix(&kalman, &u);
    kalman_set_covariance_matrix(&kalman, &p);
    kalman_set_process_noise_covariance_matrix(&kalman, &q);
    kalman_set_measurement_covariance_matrix(&kalman, &r);
    kalman_set_observation_matrix(&kalman, &c);

    kalman_predict(&kalman);

    // P becomes A*P*A' = [[2,1],[1,1]]
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), matrix_get_element(&kalman.p, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.p, 0, 1));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.p, 1, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&kalman.p, 1, 1));

    real_t measurement[1] = {REAL_C(1.0)};
    matrix_t y = make_matrix(1, 1, measurement);
    kalman_set_measurement_matrix(&kalman, &y);

    TEST_ASSERT_EQUAL(true, kalman_update(&kalman));

    // S = 2 + 1 = 3. K = [2/3, 1/3]'.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0)/REAL_C(3.0), matrix_get_element(&kalman.k, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0)/REAL_C(3.0), matrix_get_element(&kalman.k, 1, 0));
    // x = [0,0] + K*1
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0)/REAL_C(3.0), matrix_get_element(&kalman.x, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0)/REAL_C(3.0), matrix_get_element(&kalman.x, 1, 0));
    // P = (I - K*C)*P
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0)/REAL_C(3.0), matrix_get_element(&kalman.p, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0)/REAL_C(3.0), matrix_get_element(&kalman.p, 0, 1));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0)/REAL_C(3.0), matrix_get_element(&kalman.p, 1, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0)/REAL_C(3.0), matrix_get_element(&kalman.p, 1, 1));

    matrix_free(&x);
    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&u);
    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&c);
    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_step_moves_the_state_to_the_measurement(void)
{
    // A = 1, Q = 0, P = 1, R = 1. For these values the filter is the mean of
    // the measurements. After n steps the theory gives:
    //     P = 1/(n+1)
    //     x = y*n/(n+1)
    // The state must come near the measurement, and the covariance must fall
    // at each step.
    kalman_t kalman = make_scalar_filter(REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(1.0));

    real_t measurement[1] = {REAL_C(10.0)};
    matrix_t y = make_matrix(1, 1, measurement);

    real_t previous_covariance = matrix_get_element(&kalman.p, 0, 0);

    for(int n = 1; n <= 20; n++)
    {
        TEST_ASSERT_EQUAL(true, kalman_step(&kalman, NULL, &y));

        real_t covariance = matrix_get_element(&kalman.p, 0, 0);
        TEST_ASSERT_TRUE(covariance < previous_covariance);
        previous_covariance = covariance;

        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0)/(n+REAL_C(1.0)), covariance);
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(10.0)*n/(n+REAL_C(1.0)),
                                 matrix_get_element(&kalman.x, 0, 0));
    }

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_step_removes_the_noise_of_a_constant_signal(void)
{
    // The measurements move around 5.0. The filter must give a value that is
    // nearer to 5.0 than the single measurements are.
    kalman_t kalman = make_scalar_filter(REAL_C(0.0), REAL_C(10.0), REAL_C(0.01), REAL_C(1.0));

    real_t samples[10] = {REAL_C(5.9), REAL_C(4.2), REAL_C(5.5), REAL_C(4.6), REAL_C(5.3), REAL_C(4.8), REAL_C(5.4), REAL_C(4.5), REAL_C(5.2), REAL_C(4.7)};
    matrix_t y = matrix_alloc(1, 1);

    for(int i = 0; i < 10; i++)
    {
        matrix_add_element(&y, 0, 0, samples[i]);
        TEST_ASSERT_EQUAL(true, kalman_step(&kalman, NULL, &y));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.3), REAL_C(5.0), matrix_get_element(&kalman.x, 0, 0));

    matrix_free(&y);
    kalman_free(&kalman);
}

void test_kalman_get_functions(void)
{
    kalman_t kalman = kalman_alloc(1, 2, 1);

    TEST_ASSERT_EQUAL_PTR(&kalman.x, kalman_get_state_matrix(&kalman));
    TEST_ASSERT_EQUAL_PTR(&kalman.p, kalman_get_covariance_matrix(&kalman));
    TEST_ASSERT_EQUAL_PTR(&kalman.k, kalman_get_gain_matrix(&kalman));

    kalman_free(&kalman);
}

void test_kalman_static_filter_gives_the_same_result_as_a_dynamic_filter(void)
{
    real_t mempool[KALMAN_MEMPOOL_SIZE(1, 1, 1)];

    kalman_t dynamic_filter = make_scalar_filter(REAL_C(0.0), REAL_C(1.0), REAL_C(0.1), REAL_C(1.0));
    kalman_t static_filter = kalman_static_alloc(1, 1, 1, mempool);

    real_t transition[1] = {REAL_C(1.0)};
    real_t control[1] = {REAL_C(0.0)};
    real_t covariance[1] = {REAL_C(1.0)};
    real_t process[1] = {REAL_C(0.1)};
    real_t noise[1] = {REAL_C(1.0)};
    real_t observation[1] = {REAL_C(1.0)};
    real_t input[1] = {REAL_C(0.0)};
    real_t state[1] = {REAL_C(0.0)};

    matrix_t a = make_matrix(1, 1, transition);
    matrix_t b = make_matrix(1, 1, control);
    matrix_t p = make_matrix(1, 1, covariance);
    matrix_t q = make_matrix(1, 1, process);
    matrix_t r = make_matrix(1, 1, noise);
    matrix_t c = make_matrix(1, 1, observation);
    matrix_t u = make_matrix(1, 1, input);
    matrix_t x = make_matrix(1, 1, state);

    kalman_set_state_matrix(&static_filter, &x);
    kalman_set_state_transition_matrix(&static_filter, &a);
    kalman_set_control_matrix(&static_filter, &b);
    kalman_set_covariance_matrix(&static_filter, &p);
    kalman_set_process_noise_covariance_matrix(&static_filter, &q);
    kalman_set_measurement_covariance_matrix(&static_filter, &r);
    kalman_set_observation_matrix(&static_filter, &c);
    kalman_set_input_matrix(&static_filter, &u);

    matrix_t y = matrix_alloc(1, 1);
    real_t samples[5] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(2.5), REAL_C(1.5)};

    for(int i = 0; i < 5; i++)
    {
        matrix_add_element(&y, 0, 0, samples[i]);
        TEST_ASSERT_EQUAL(true, kalman_step(&dynamic_filter, NULL, &y));
        TEST_ASSERT_EQUAL(true, kalman_step(&static_filter, NULL, &y));
        TEST_ASSERT_REAL_WITHIN(TOLERANCE,
                                 matrix_get_element(&dynamic_filter.x, 0, 0),
                                 matrix_get_element(&static_filter.x, 0, 0));
    }

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&c);
    matrix_free(&u);
    matrix_free(&x);
    matrix_free(&y);
    kalman_free(&dynamic_filter);
    kalman_free(&static_filter);
}
