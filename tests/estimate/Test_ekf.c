#include "unity.h"
#include "real_assert.h"
#include "ekf.h"
#include "kalman.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.001)

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

// A model that is DRIVEN FROM OUTSIDE: the state grows by whatever the input
// says at each step.
//
// Every other model in this file writes (void)input and ignores it, which is
// why the input of the filter went untested for so long: nothing that was ever
// predicted with could tell whether the input had been read at all.
static void driven_state(const matrix_t* state, const matrix_t* input,
                         matrix_t* result)
{
    real_t held = matrix_get_element((matrix_t*)state, 0, 0);
    real_t drive = matrix_get_element((matrix_t*)input, 0, 0);

    matrix_add_element(result, 0, 0, held + drive);
}

// The measurement is the square of the state. Its slope at the point x is 2x.
static void square_measurement(const matrix_t* state, matrix_t* result)
{
    real_t value = matrix_get_element((matrix_t*)state, 0, 0);
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
    real_t position = matrix_get_element((matrix_t*)state, 0, 0);
    real_t speed = matrix_get_element((matrix_t*)state, 1, 0);

    matrix_add_element(result, 0, 0, position + speed);
    matrix_add_element(result, 1, 0, speed);
}

// The measurement reads the position only.
static void position_measurement(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0, matrix_get_element((matrix_t*)state, 0, 0));
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

static void set_scalar(ekf_t* ekf, void (*setter)(ekf_t*, matrix_t*), real_t value)
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
    TEST_ASSERT_EQUAL_REAL(EKF_DEFAULT_DERIVATIVE_STEP, ekf.derivative_step);

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
    real_t mempool[EKF_MEMPOOL_SIZE(2, 3, 2)];
    ekf_t ekf = ekf_static_alloc(2, 3, 2, mempool);

    real_t* end = ekf.scratch.ny1_c.elem
                 + (ekf.scratch.ny1_c.m * ekf.scratch.ny1_c.n);

    TEST_ASSERT_EQUAL_PTR(mempool + EKF_MEMPOOL_SIZE(2, 3, 2), end);
    TEST_ASSERT_EQUAL(false, ekf.dynamic_alloc);
}

void test_ekf_the_state_jacobian_of_a_constant_model_is_the_unit_matrix(void)
{
    // f(x) = x, thus the slope is one.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(5.0));

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&jacobian, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_measurement_jacobian_of_a_square_is_two_times_the_state(void)
{
    // h(x) = x*x, thus the slope at the point x is 2x.
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_measurement_function(&ekf, square_measurement);

    matrix_t jacobian = matrix_alloc(1, 1);

    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(3.0));
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(6.0), matrix_get_element(&jacobian, 0, 0));

    set_scalar(&ekf, ekf_set_state_matrix, -REAL_C(2.5));
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), -REAL_C(5.0), matrix_get_element(&jacobian, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_state_jacobian_of_the_constant_speed_model(void)
{
    // The next position is position + speed, and the next speed is the speed.
    // Thus the Jacobian is [[1,1],[0,1]].
    ekf_t ekf = ekf_alloc(1, 2, 1);
    ekf_set_state_function(&ekf, constant_speed);

    real_t state[2] = {REAL_C(10.0), REAL_C(2.0)};
    matrix_t x = make_matrix(2, 1, state);
    ekf_set_state_matrix(&ekf, &x);

    matrix_t jacobian = matrix_alloc(2, 2);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), matrix_get_element(&jacobian, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), matrix_get_element(&jacobian, 0, 1));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0), matrix_get_element(&jacobian, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.0), matrix_get_element(&jacobian, 1, 1));

    matrix_free(&x);
    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_the_jacobian_does_not_change_the_state(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(7.0));

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_state_jacobian_into(&ekf, &jacobian);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(7.0), matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&jacobian);
    ekf_free(&ekf);
}

void test_ekf_predict_moves_the_state_through_the_function(void)
{
    ekf_t ekf = ekf_alloc(1, 2, 1);
    ekf_set_state_function(&ekf, constant_speed);

    real_t state[2] = {REAL_C(0.0), REAL_C(3.0)};
    matrix_t x = make_matrix(2, 1, state);
    ekf_set_state_matrix(&ekf, &x);

    ekf_predict(&ekf);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(3.0), matrix_get_element(&ekf.x, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(3.0), matrix_get_element(&ekf.x, 1, 0));

    ekf_predict(&ekf);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(6.0), matrix_get_element(&ekf.x, 0, 0));

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
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_covariance_matrix, REAL_C(1.0));
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, REAL_C(1.0));

    kalman_t kalman = kalman_alloc(1, 1, 1);
    real_t one = REAL_C(1.0);
    real_t zero = REAL_C(0.0);
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

    real_t measurement = REAL_C(4.0);
    matrix_t y = make_matrix(1, 1, &measurement);

    for(uint32_t step = 0; step < 10; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        TEST_ASSERT_EQUAL(true, kalman_step(&kalman, NULL, &y));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01),
                                 matrix_get_element(&kalman.x, 0, 0),
                                 matrix_get_element(&ekf.x, 0, 0));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01),
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
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(1.0));
    set_scalar(&ekf, ekf_set_covariance_matrix, REAL_C(10.0));
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, REAL_C(0.001));
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, REAL_C(0.1));

    real_t measurement = REAL_C(9.0);
    matrix_t y = make_matrix(1, 1, &measurement);

    for(uint32_t step = 0; step < 40; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(3.0), matrix_get_element(&ekf.x, 0, 0));

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
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(0.5));
    set_scalar(&ekf, ekf_set_covariance_matrix, REAL_C(5.0));
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, REAL_C(0.001));
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, REAL_C(0.5));

    real_t samples[10] = {REAL_C(4.4), REAL_C(3.6), REAL_C(4.2), REAL_C(3.8), REAL_C(4.1), REAL_C(3.9), REAL_C(4.3), REAL_C(3.7), REAL_C(4.0), REAL_C(4.05)};
    matrix_t y = matrix_alloc(1, 1);

    for(uint32_t round = 0; round < 4; round++)
    {
        for(uint32_t index = 0; index < 10; index++)
        {
            matrix_add_element(&y, 0, 0, samples[index]);
            TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        }
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0), matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_the_covariance_never_grows_when_there_is_no_process_noise(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);
    ekf_set_state_function(&ekf, constant_state);
    ekf_set_measurement_function(&ekf, direct_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_covariance_matrix, REAL_C(1.0));
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, REAL_C(1.0));

    real_t measurement = REAL_C(2.0);
    matrix_t y = make_matrix(1, 1, &measurement);

    real_t previous = matrix_get_element(&ekf.p, 0, 0);
    for(uint32_t step = 0; step < 15; step++)
    {
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
        real_t current = matrix_get_element(&ekf.p, 0, 0);
        TEST_ASSERT_TRUE(current <= previous + REAL_C(0.0001));
        TEST_ASSERT_TRUE(current >= -REAL_C(0.0001));
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

    real_t state[2] = {REAL_C(0.0), REAL_C(0.0)};
    real_t covariance[4] = {REAL_C(10.0), REAL_C(0.0), REAL_C(0.0), REAL_C(10.0)};
    real_t process[4] = {REAL_C(0.001), REAL_C(0.0), REAL_C(0.0), REAL_C(0.001)};
    real_t noise = REAL_C(0.1);

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
        matrix_add_element(&y, 0, 0, REAL_C(2.0)*(real_t)step);
        TEST_ASSERT_EQUAL(true, ekf_step(&ekf, NULL, &y));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0), matrix_get_element(&ekf.x, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), REAL_C(60.0), matrix_get_element(&ekf.x, 0, 0));

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
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(1.0));
    set_scalar(&ekf, ekf_set_covariance_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_process_noise_covariance_matrix, REAL_C(0.0));
    set_scalar(&ekf, ekf_set_measurement_covariance_matrix, REAL_C(0.0));

    real_t measurement = REAL_C(5.0);
    matrix_t y = make_matrix(1, 1, &measurement);
    ekf_set_measurement_matrix(&ekf, &y);

    TEST_ASSERT_EQUAL(false, ekf_update(&ekf));
    TEST_ASSERT_EQUAL(true, ekf.singular);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&y);
    ekf_free(&ekf);
}

void test_ekf_set_derivative_step(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);

    ekf_set_derivative_step(&ekf, REAL_C(0.01));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.01), ekf.derivative_step);

    // A larger step still gives the correct slope for a square, because the
    // central difference has no error for a polynomial of the second power.
    ekf_set_measurement_function(&ekf, square_measurement);
    set_scalar(&ekf, ekf_set_state_matrix, REAL_C(4.0));

    matrix_t jacobian = matrix_alloc(1, 1);
    ekf_measurement_jacobian_into(&ekf, &jacobian);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(8.0), matrix_get_element(&jacobian, 0, 0));

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
    real_t mempool[EKF_MEMPOOL_SIZE(1, 1, 1)];

    ekf_t dynamic_ekf = ekf_alloc(1, 1, 1);
    ekf_t static_ekf = ekf_static_alloc(1, 1, 1, mempool);

    ekf_t* filters[2] = {&dynamic_ekf, &static_ekf};
    for(uint32_t which = 0; which < 2; which++)
    {
        ekf_set_state_function(filters[which], constant_state);
        ekf_set_measurement_function(filters[which], square_measurement);
        set_scalar(filters[which], ekf_set_state_matrix, REAL_C(1.0));
        set_scalar(filters[which], ekf_set_covariance_matrix, REAL_C(5.0));
        set_scalar(filters[which], ekf_set_process_noise_covariance_matrix, REAL_C(0.01));
        set_scalar(filters[which], ekf_set_measurement_covariance_matrix, REAL_C(0.5));
    }

    matrix_t y = matrix_alloc(1, 1);
    real_t samples[5] = {REAL_C(4.0), REAL_C(4.2), REAL_C(3.8), REAL_C(4.1), REAL_C(3.9)};

    for(uint32_t index = 0; index < 5; index++)
    {
        matrix_add_element(&y, 0, 0, samples[index]);
        TEST_ASSERT_EQUAL(true, ekf_step(&dynamic_ekf, NULL, &y));
        TEST_ASSERT_EQUAL(true, ekf_step(&static_ekf, NULL, &y));
        TEST_ASSERT_REAL_WITHIN(TOLERANCE,
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

// THE INPUT IS WHAT DRIVES THE STATE FROM OUTSIDE. A throttle, a heater, a
// steering angle: something the filter is told rather than something it works
// out. Without it a filter can only watch, and every model in this file but one
// ignores it.
void test_ekf_the_input_drives_the_state(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);

    ekf_set_state_function(&ekf, driven_state);

    real_t begins[1] = {REAL_C(0.0)};
    matrix_t x = make_matrix(1, 1, begins);

    ekf_set_state_matrix(&ekf, &x);

    // With nothing driving it the state stays where it is.
    real_t nothing[1] = {REAL_C(0.0)};
    matrix_t none = make_matrix(1, 1, nothing);

    ekf_set_input_matrix(&ekf, &none);
    ekf_predict(&ekf);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0),
                            matrix_get_element(&ekf.x, 0, 0));

    // And with something driving it, it moves by that much at every step.
    real_t drive[1] = {REAL_C(2.0)};
    matrix_t u = make_matrix(1, 1, drive);

    ekf_set_input_matrix(&ekf, &u);

    ekf_predict(&ekf);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0),
                            matrix_get_element(&ekf.x, 0, 0));

    ekf_predict(&ekf);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.0),
                            matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&x);
    matrix_free(&none);
    matrix_free(&u);
    ekf_free(&ekf);
}

// THE INPUT IS COPIED AND NOT HELD BY REFERENCE. A caller that set the input
// from a matrix on the stack and then let that matrix go would otherwise be
// giving the filter memory that is no longer theirs, and the filter would carry
// on predicting from it.
void test_ekf_the_input_matrix_is_copied(void)
{
    ekf_t ekf = ekf_alloc(1, 1, 1);

    ekf_set_state_function(&ekf, driven_state);

    real_t begins[1] = {REAL_C(0.0)};
    matrix_t x = make_matrix(1, 1, begins);

    ekf_set_state_matrix(&ekf, &x);

    real_t drive[1] = {REAL_C(5.0)};
    matrix_t u = make_matrix(1, 1, drive);

    ekf_set_input_matrix(&ekf, &u);

    // Change what the caller holds AFTER giving it to the filter. The filter
    // must carry on with what it was given.
    matrix_add_element(&u, 0, 0, REAL_C(100.0));

    ekf_predict(&ekf);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(5.0),
                            matrix_get_element(&ekf.x, 0, 0));

    matrix_free(&x);
    matrix_free(&u);
    ekf_free(&ekf);
}
