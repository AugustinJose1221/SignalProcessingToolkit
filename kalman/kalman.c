#include <kalman/kalman.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

kalman_t kalman_alloc(uint32_t ni, uint32_t nx, uint32_t ny)
{
    assert(ni > 0);
    assert(nx > 0);
    assert(ny > 0);

    kalman_t kalman;

    kalman.ni = ni;
    kalman.nx = nx;
    kalman.ny = ny;

    kalman._x = matrix_alloc(nx, 1);
    kalman.x = matrix_alloc(nx, 1);
    kalman.a = matrix_alloc(nx, nx);
    kalman.b = matrix_alloc(nx, ni);
    kalman.p = matrix_alloc(nx, nx);
    kalman.q = matrix_alloc(nx, ny);
    kalman.c = matrix_alloc(ny, nx);
    kalman.k = matrix_alloc(nx, ny);

    return kalman;
}

void kalman_set_state_matrix(kalman_t* kalman, matrix_t* state_matrix)
{
    assert(kalman != NULL);
    assert(state_matrix != NULL);

    matrix_copy(state_matrix, &kalman->_x);
}

void kalman_set_state_transition_matrix(kalman_t* kalman, matrix_t* state_transition_matrix)
{
    assert(kalman != NULL);
    assert(state_transition_matrix != NULL);

    matrix_copy(state_transition_matrix, &kalman->a);
}

void kalman_set_control_matrix(kalman_t* kalman, matrix_t* control_matrix)
{
    assert(kalman != NULL);
    assert(control_matrix != NULL);

    matrix_copy(control_matrix, &kalman->b);
}

void kalman_set_covariance_matrix(kalman_t* kalman, matrix_t* covariance_matrix)
{
    assert(kalman != NULL);
    assert(covariance_matrix != NULL);

    matrix_copy(covariance_matrix, &kalman->p);
}

void kalman_set_process_noise_covariance_matrix(kalman_t* kalman, matrix_t* process_noise_covariance)
{
    assert(kalman != NULL);
    assert(process_noise_covariance != NULL);

    matrix_copy(process_noise_covariance, &kalman->q);
}

void kalman_set_measurement_covariance_matrix(kalman_t* kalman, matrix_t* measurement_covariance)
{
    assert(kalman != NULL);
    assert(measurement_covariance != NULL);

    matrix_copy(measurement_covariance, &kalman->r);
}
