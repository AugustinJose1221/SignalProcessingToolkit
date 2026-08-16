#include "unity.h"
#include "ekf.h"
#include "kalman.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   0.001f

void setUp(void)
{

}

void tearDown(void)
{

}

// A state that does not change.
static void constant_state(const matrix_t* state, const matrix_t* input,
                           matrix_t* result)
{
    (void)input;
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
}

// The measurement is the square of the state. Its slope at the point x is 2x.
static void square_measurement(const matrix_t* state, matrix_t* result)
{
    float value = matrix_get_element((matrix_t*)state, 0, 0);
    matrix_add_element(result, 0, 0, value*value);
}

// The measurement is the state itself, which is a linear model.
static void direct_measurement(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
}

// A model of a constant speed. The position grows with the speed at each step.
static void constant_speed(const matrix_t* state, const matrix_t* input,
                           matrix_t* result)
{
    (void)input;
    float position = matrix_get_element((matrix_t*)state, 0, 0);
    float speed = matrix_get_element((matrix_t*)state, 1, 0);

    matrix_add_element(result, 0, 0, position + speed);
    matrix_add_element(result, 1, 0, speed);
}

// The measurement reads the position only.
static void position_measurement(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
}

static matrix_t make_matrix(uint32_t m, uint32_t n, float* values)
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

static void set_scalar(ekf_t* ekf, void (*setter)(ekf_t*, matrix_t*), float value)
{
    matrix_t matrix = make_matrix(1, 1, &value);
    setter(ekf, &matrix);
    matrix_free(&matrix);
}

void test_ekf_alloc(void)
{
    ekf_t ekf = ekf_alloc(1, 2, 3);

    TEST_ASSERT_EQUAL(1, ekf.ni);
    TEST_ASSERT_EQUAL(2, ekf.nx);
    TEST_ASSERT_EQUAL(3, ekf.ny);
    TEST_ASSERT_EQUAL(true, ekf.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(ekf.mempool);
    TEST_ASSERT_NULL(ekf.state_function);
    TEST_ASSERT_EQUAL_FLOAT(EKF_DEFAULT_DERIVATIVE_STEP, ekf.derivative_step);

    ekf_free(&ekf);
}

void test_ekf_alloc_gives_matrices_of_the_correct_order(void)
{
    ekf_t ekf = ekf_alloc(2, 4, 3);

    TEST_ASSERT_EQUAL(4, ekf.x.m);
    TEST_ASSERT_EQUAL(1, ekf.x.n);
    TEST_ASSERT_EQUAL(3, ekf.y.m);
    TEST_ASSERT_EQUAL(2, ekf.u.m);
    TEST_ASSERT_EQUAL(4, ekf.p.m);
    TEST_ASSERT_EQUAL(4, ekf.p.n);
    TEST_ASSERT_EQUAL(4, ekf.a.m);
    TEST_ASSERT_EQUAL(4, ekf.a.n);
    TEST_ASSERT_EQUAL(3, ekf.c.m);
    TEST_ASSERT_EQUAL(4, ekf.c.n);
    TEST_ASSERT_EQUAL(4, ekf.k.m);
    TEST_ASSERT_EQUAL(3, ekf.k.n);

    ekf_free(&ekf);
}

void test_ekf_static_alloc_stays_inside_the_memory_pool(void)
{
    float mempool[EKF_MEMPOOL_SIZE(2, 3, 2)];
    ekf_t ekf = ekf_static_alloc(2, 3, 2, mempool);

    float* end = ekf.scratch.ny1_c.elem
                 + (ekf.scratch.ny1_c.m * ekf.scratch.ny1_c.n);

    TEST_ASSERT_EQUAL_PTR(mempool + EKF_MEMPOOL_SIZE(2, 3, 2), end);
    TEST_ASSERT_EQUAL(false, ekf.dynamic_alloc);
}

void test_ekf_the_state_jacobian_of_a_constant_model_is_the_unit_matrix(void)
{
    // f(x) = x, thus the slope is one.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    set_scalar(&ekf, ekf_set_state_matrix, 5.0f);

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, matrix_get_element(&jacobian, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_measurement_jacobian_of_a_square_is_two_times_the_state(void)
{
    // h(x) = x*x, thus the slope at the point x is 2x.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_measurement_function(&ekf, square_measurement);

    matrix_t jacobian = matrix_alloc(1, 1);

    set_scalar(&ekf, ekf_set_state_matrix, 3.0f);
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, matrix_get_element(&jacobian, 0, 0));

    set_scalar(&ekf, ekf_set_state_matrix, -2.5f);
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -5.0f, matrix_get_element(&jacobian, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_state_jacobian_of_the_constant_speed_model(void)
{
    // The next position is position + speed, and the next speed is the speed.
    // Thus the Jacobian is [[1,1],[0,1]].
    ekf_t ekf = ekf_alloc(1, 2, 1);
    ekf_set_state_function(&ekf, constant_speed);

    float state[2] = {10.0f, 2.0f};
    matrix_t x = make_matrix(2, 1, state);
    ekf_set_state_matrix(&ekf, &x);

    matrix_t jacobian = matrix_alloc(2, 2);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, matrix_get_element(&jacobian, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, matrix_get_element(&jacobian, 0, 1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, matrix_get_element(&jacobian, 1, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, matrix_get_element(&jacobian, 1, 1));

    matrix_free(&x);
    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_jacobian_does_not_change_the_state(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    set_scalar(&ekf, ekf_set_state_matrix, 7.0f);

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 7.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_predict_moves_the_state_through_the_function(void)
{
    ekf_t ekf = ekf_alloc(1, 2, 1);
    ekf_set_state_function(&ekf, constant_speed);

    float state[2] = {0.0f, 3.0f};
    matrix_t x = make_matrix(2, 1, state);
    ekf_set_state_matrix(&ekf, &x);

    ekf_predict(&ekf);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, matrix_get_element(&ekf.x, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, matrix_get_element(&ekf.x, 1, 0));

    ekf_predict(&ekf);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&x);
    ekf_free(&ekf);
}

void test_ekf_with_a_linear_model_agrees_with_the_kalman_filter(void)
{
    // A model where the function is linear must give the same answer as the
    // plain Kalman filter, because the Jacobian is then the matrix itself.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, direct_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_covariance_matrix, 1.0f);
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, 1.0f);

    kalman_t kalman = kalman_alloc(1, 1, 1);
    float one = 1.0f;
    float zero = 0.0f;
    matrix_t unit = make_matrix(1, 1, &one);
    matrix_t nothing = make_matrix(1, 1, &zero);
    kalman_set_state_matrix(&kalman, &nothing);
    kalman_set_state_transition_matrix(&kalman, &unit);
    kalman_set_control_matrix(&kalman, &nothing);
    kalman_set_input_matrix(&kalman, &nothing);
    kalman_set_covariance_matrix(&kalman, &unit);
    kalman_set_process_noise_covariance_matrix(&kalman, &nothing);
    kalman_set_measurement_covariance_matrix(&kalman, &unit);
    kalman_set_observation_matrix(&kalman, &unit);

    float measurement = 4.0f;
    matrix_t y = make_matrix(1, 1, &measurement);

    for(uint32_t step = 0; step < 10; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        TEST_ASSERT_EQUAL(true, kalman_step(&kalman, NULL, &y));

        TEST_ASSERT_FLOAT_WITHIN(0.01f,
                                 matrix_get_element(&kalman.x, 0, 0),
                                 matrix_get_element(&ekf.x, 0, 0));
        TEST_ASSERT_FLOAT_WITHIN(0.01f,
                                 matrix_get_element(&kalman.p, 0, 0),
                                 matrix_get_element(&ekf.p, 0, 0));
    }

    matrix_free(&unit);
    matrix_free(&nothing);
    matrix_free(&y);
    kalman_free(&kalman);
    ekf_free(&ekf);
}

void test_ekf_finds_the_state_behind_a_measurement_that_is_not_linear(void)
{
    // The measurement is the square of the state. The true state is 3, thus
    // the measurement is 9. The filter must find the value 3 from the
    // measurement 9, which a plain Kalman filter cannot do.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, square_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 1.0f);
    set_scalar(&ekf, ekf_set_covariance_matrix, 10.0f);
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, 0.001f);
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, 0.1f);

    float measurement = 9.0f;
    matrix_t y = make_matrix(1, 1, &measurement);

    for(uint32_t step = 0; step < 40; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
    }

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_removes_the_noise_of_a_measurement_that_is_not_linear(void)
{
    // The measurements move around the square of 2, which is 4. The filter
    // must come near the state 2.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, square_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 0.5f);
    set_scalar(&ekf, ekf_set_covariance_matrix, 5.0f);
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, 0.001f);
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, 0.5f);

    float samples[10] = {4.4f, 3.6f, 4.2f, 3.8f, 4.1f, 3.9f, 4.3f, 3.7f, 4.0f, 4.05f};
    matrix_t y = matrix_alloc(1, 1);

    for(uint32_t round = 0; round < 4; round++)
    {
        for(uint32_t index = 0; index < 10; index++)
        {
            matrix_add_element(&y, 0, 0, samples[index]);
            TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        }
    }

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_the_covariance_never_grows_when_there_is_no_process_noise(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, direct_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_covariance_matrix, 1.0f);
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, 1.0f);

    float measurement = 2.0f;
    matrix_t y = make_matrix(1, 1, &measurement);

    float previous = matrix_get_element(&ekf.p, 0, 0);
    for(uint32_t step = 0; step < 15; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        float current = matrix_get_element(&ekf.p, 0, 0);
        TEST_ASSERT_TRUE(current <= previous + 0.0001f);
        TEST_ASSERT_TRUE(current >= -0.0001f);
        previous = current;
    }

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_the_two_dimensional_filter_finds_the_speed(void)
{
    // The filter reads the position only. From a position that grows by 2 at
    // each step it must find the speed 2.
    ekf_t ekf = ekf_alloc(1, 2, 1);
    ekf_set_state_function(&ekf, constant_speed);
    ekf_set_measurement_function(&ekf, position_measurement);

    float state[2] = {0.0f, 0.0f};
    float covariance[4] = {10.0f, 0.0f, 0.0f, 10.0f};
    float process[4] = {0.001f, 0.0f, 0.0f, 0.001f};
    float noise = 0.1f;

    matrix_t x = make_matrix(2, 1, state);
    matrix_t p = make_matrix(2, 2, covariance);
    matrix_t q = make_matrix(2, 2, process);
    matrix_t r = make_matrix(1, 1, &noise);

    ekf_set_state_matrix(&ekf, &x);
    ekf_set_covariance_matrix(&ekf, &p);
    ekf_set_process_noise_covariance_matrix(&ekf, &q);
    ekf_set_measurement_covariance_matrix(&ekf, &r);

    matrix_t y = matrix_alloc(1, 1);

    for(uint32_t step = 1; step <= 30; step++)
    {
        matrix_add_element(&y, 0, 0, 2.0f*(float)step);
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
    }

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, matrix_get_element(&ekf.x, 1, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&x);
    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_update_reports_a_singular_innovation_covariance(void)
{
    // With no doubt in the state and no noise in the measurement the
    // innovation covariance becomes zero, which cannot be inverted.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, direct_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 1.0f);
    set_scalar(&ekf, ekf_set_covariance_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, 0.0f);
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, 0.0f);

    float measurement = 5.0f;
    matrix_t y = make_matrix(1, 1, &measurement);
    ekf_set_measurement_matrix(&ekf, &y);

    TEST_ASSERT_EQUAL(false, ekf_update(&ekf));
    TEST_ASSERT_EQUAL(true, ekf.singular);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_set_derivative_step(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);

    ekf_set_derivative_step(&ekf, 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, ekf.derivative_step);

    // A larger step still gives the correct slope for a square, because the
    // central difference has no error for a polynomial of the second power.
    ekf_set_measurement_function(&ekf, square_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, 4.0f);

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, matrix_get_element(&jacobian, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_get_functions(void)
{
    ekf_t ekf = ekf_alloc(1, 2, 1);

    TEST_ASSERT_EQUAL_PTR(&ekf.x, ekf_get_state_matrix(&ekf));
    TEST_ASSERT_EQUAL_PTR(&ekf.p, ekf_get_covariance_matrix(&ekf));
    TEST_ASSERT_EQUAL_PTR(&ekf.k, ekf_get_gain_matrix(&ekf));

    ekf_free(&ekf);
}

void test_ekf_a_static_filter_gives_the_same_result_as_a_dynamic_one(void)
{
    float mempool[EKF_MEMPOOL_SIZE(1, 1, 1)];

    ekf_t dynamic_ekf = ekf_alloc(1, 1, 1);
    ekf_t static_ekf = ekf_static_alloc(1, 1, 1, mempool);

    ekf_t* filters[2] = {&dynamic_ekf, &static_ekf};
    for(uint32_t which = 0; which < 2; which++)
    {
        ekf_set_state_function(filters[which], constant_state);
        ekf_set_measurement_function(filters[which], square_measurement);
        set_scalar(filters[which], ekf_set_state_matrix, 1.0f);
        set_scalar(filters[which], ekf_set_covariance_matrix, 5.0f);
        set_scalar(filters[which], ekf_set_process_noise_covariance_matrix, 0.01f);
        set_scalar(filters[which], ekf_set_measurement_covariance_matrix, 0.5f);
    }

    matrix_t y = matrix_alloc(1, 1);
    float samples[5] = {4.0f, 4.2f, 3.8f, 4.1f, 3.9f};

    for(uint32_t index = 0; index < 5; index++)
    {
        matrix_add_element(&y, 0, 0, samples[index]);
        TEST_ASSERT_EQUAL(true, ekf_step(&dynamic_ekf, NULL, &y));
        TEST_ASSERT_EQUAL(true, ekf_step(&static_ekf, NULL, &y));
        TEST_ASSERT_FLOAT_WITHIN(TOLERANCE,
                                 matrix_get_element(&dynamic_ekf.x, 0, 0),
                                 matrix_get_element(&static_ekf.x, 0, 0));
    }

    matrix_free(&y);
    ekf_free(&dynamic_ekf);
    ekf_free(&static_ekf);
}

void test_ekf_free_releases_a_dynamic_filter(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);

    ekf_free(&ekf);

    TEST_ASSERT_NULL(ekf.mempool);
    TEST_ASSERT_EQUAL(false, ekf.dynamic_alloc);

    ekf_free(&ekf);
    TEST_ASSERT_NULL(ekf.mempool);
}
