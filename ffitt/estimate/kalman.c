#ifndef TEST
#include <ffitt/estimate/kalman.h>
#include <ffitt/core/defs.h>
#else
#include "kalman.h"
#include "defs.h"
#endif

static matrix_t take_from_pool(real_t** pool, uint32_t m, uint32_t n);
static void build_matrices(kalman_t* kalman, real_t* mempool);

// Allocation

kalman_t kalman_alloc(uint32_t ni, uint32_t nx, uint32_t ny)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);

    kalman_t kalman;
    real_t* mempool;

    mempool = (real_t*)malloc(sizeof(real_t)*KALMAN_MEMPOOL_SIZE(ni, nx, ny));

    kalman.ni = ni;
    kalman.nx = nx;
    kalman.ny = ny;
    kalman.mempool = mempool;
    kalman.singular = false;
    kalman.dynamic_alloc = true;

    // The matrices are laid out across the pool, thus there must be a pool.
    if(mempool == NULL)
    {
        kalman.ni = 0;
        kalman.nx = 0;
        kalman.ny = 0;
        kalman.dynamic_alloc = false;

        return kalman;
    }

    build_matrices(&kalman, mempool);

    return kalman;
}

kalman_t kalman_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);
    ASSERT(mempool != NULL);

    kalman_t kalman;

    kalman.ni = ni;
    kalman.nx = nx;
    kalman.ny = ny;
    kalman.mempool = mempool;
    kalman.singular = false;
    kalman.dynamic_alloc = false;

    build_matrices(&kalman, mempool);

    return kalman;
}

// The two allocation functions share this layout. Thus a dynamic filter and a
// static filter hold their matrices in the same order.
static void build_matrices(kalman_t* kalman, real_t* mempool)
{
    uint32_t ni = kalman->ni;
    uint32_t nx = kalman->nx;
    uint32_t ny = kalman->ny;
    real_t* pool = mempool;

    kalman->_x = take_from_pool(&pool, nx, 1);
    kalman->x  = take_from_pool(&pool, nx, 1);
    kalman->y  = take_from_pool(&pool, ny, 1);
    kalman->u  = take_from_pool(&pool, ni, 1);
    kalman->a  = take_from_pool(&pool, nx, nx);
    kalman->b  = take_from_pool(&pool, nx, ni);
    kalman->p  = take_from_pool(&pool, nx, nx);
    kalman->q  = take_from_pool(&pool, nx, nx);
    kalman->r  = take_from_pool(&pool, ny, ny);
    kalman->c  = take_from_pool(&pool, ny, nx);
    kalman->k  = take_from_pool(&pool, nx, ny);

    kalman->scratch.nxnx_a    = take_from_pool(&pool, nx, nx);
    kalman->scratch.nxnx_b    = take_from_pool(&pool, nx, nx);
    kalman->scratch.nxnx_c    = take_from_pool(&pool, nx, nx);
    kalman->scratch.nxny_a    = take_from_pool(&pool, nx, ny);
    kalman->scratch.nxny_b    = take_from_pool(&pool, nx, ny);
    kalman->scratch.nynx_a    = take_from_pool(&pool, ny, nx);
    kalman->scratch.nyny_a    = take_from_pool(&pool, ny, ny);
    kalman->scratch.nyny_b    = take_from_pool(&pool, ny, ny);
    kalman->scratch.nyny_c    = take_from_pool(&pool, ny, ny);
    kalman->scratch.augmented = take_from_pool(&pool, ny, 2*ny);
    kalman->scratch.nx1_a     = take_from_pool(&pool, nx, 1);
    kalman->scratch.nx1_b     = take_from_pool(&pool, nx, 1);
    kalman->scratch.ny1_a     = take_from_pool(&pool, ny, 1);
    kalman->scratch.ny1_b     = take_from_pool(&pool, ny, 1);

    for(uint32_t i = 0; i < KALMAN_MEMPOOL_SIZE(ni, nx, ny); i++)
    {
        mempool[i] = REAL_C(0.0);
    }
}

static matrix_t take_from_pool(real_t** pool, uint32_t m, uint32_t n)
{
    matrix_t matrix = matrix_static_alloc(m, n, *pool);
    *pool += m*n;
    return matrix;
}

// Configuration

void kalman_set_state_matrix(kalman_t* kalman, matrix_t* state_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(state_matrix != NULL);

    matrix_copy(state_matrix, &kalman->_x);
    matrix_copy(state_matrix, &kalman->x);
}

void kalman_set_state_transition_matrix(kalman_t* kalman, matrix_t* state_transition_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(state_transition_matrix != NULL);

    matrix_copy(state_transition_matrix, &kalman->a);
}

void kalman_set_control_matrix(kalman_t* kalman, matrix_t* control_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(control_matrix != NULL);

    matrix_copy(control_matrix, &kalman->b);
}

void kalman_set_covariance_matrix(kalman_t* kalman, matrix_t* covariance_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(covariance_matrix != NULL);

    matrix_copy(covariance_matrix, &kalman->p);
}

void kalman_set_process_noise_covariance_matrix(kalman_t* kalman, matrix_t* process_noise_covariance)
{
    ASSERT(kalman != NULL);
    ASSERT(process_noise_covariance != NULL);

    matrix_copy(process_noise_covariance, &kalman->q);
}

void kalman_set_measurement_covariance_matrix(kalman_t* kalman, matrix_t* measurement_covariance)
{
    ASSERT(kalman != NULL);
    ASSERT(measurement_covariance != NULL);

    matrix_copy(measurement_covariance, &kalman->r);
}

void kalman_set_observation_matrix(kalman_t* kalman, matrix_t* observation_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(observation_matrix != NULL);

    matrix_copy(observation_matrix, &kalman->c);
}

void kalman_set_input_matrix(kalman_t* kalman, matrix_t* input_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(input_matrix != NULL);

    matrix_copy(input_matrix, &kalman->u);
}

void kalman_set_measurement_matrix(kalman_t* kalman, matrix_t* measurement_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(measurement_matrix != NULL);

    matrix_copy(measurement_matrix, &kalman->y);
}

// Filter

// The predict step calculates the state before the measurement:
//     x = A*x + B*u
//     P = A*P*A' + Q
void kalman_predict(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    kalman_scratch_t* scratch = &kalman->scratch;

    matrix_multiply_into(&kalman->a, &kalman->_x, &scratch->nx1_a);
    matrix_multiply_into(&kalman->b, &kalman->u, &scratch->nx1_b);
    matrix_add_into(&scratch->nx1_a, &scratch->nx1_b, &kalman->x);

    matrix_multiply_into(&kalman->a, &kalman->p, &scratch->nxnx_a);
    matrix_transpose_into(&kalman->a, &scratch->nxnx_b);
    matrix_multiply_into(&scratch->nxnx_a, &scratch->nxnx_b, &scratch->nxnx_c);
    matrix_add_into(&scratch->nxnx_c, &kalman->q, &kalman->p);

    matrix_copy(&kalman->x, &kalman->_x);
}

// The update step corrects the state with the measurement:
//     S = C*P*C' + R
//     K = P*C'*inverse(S)
//     x = x + K*(y - C*x)
//     P = (I - K*C)*P
//
// The function gives false if S is singular. The state does not change then.
bool kalman_update(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    kalman_scratch_t* scratch = &kalman->scratch;

    // The innovation, which is the difference between the measurement and the
    // expected measurement.
    matrix_multiply_into(&kalman->c, &kalman->x, &scratch->ny1_a);
    matrix_subtract_into(&kalman->y, &scratch->ny1_a, &scratch->ny1_b);

    // The innovation covariance.
    matrix_multiply_into(&kalman->c, &kalman->p, &scratch->nynx_a);
    matrix_transpose_into(&kalman->c, &scratch->nxny_a);
    matrix_multiply_into(&scratch->nynx_a, &scratch->nxny_a, &scratch->nyny_a);
    matrix_add_into(&scratch->nyny_a, &kalman->r, &scratch->nyny_b);

    if(!matrix_inverse_into(&scratch->nyny_b, &scratch->nyny_c, &scratch->augmented))
    {
        kalman->singular = true;
        return false;
    }

    // The gain.
    matrix_multiply_into(&kalman->p, &scratch->nxny_a, &scratch->nxny_b);
    matrix_multiply_into(&scratch->nxny_b, &scratch->nyny_c, &kalman->k);

    // The corrected state.
    matrix_multiply_into(&kalman->k, &scratch->ny1_b, &scratch->nx1_a);
    matrix_add_into(&kalman->x, &scratch->nx1_a, &scratch->nx1_b);
    matrix_copy(&scratch->nx1_b, &kalman->x);

    // The corrected covariance.
    matrix_multiply_into(&kalman->k, &kalman->c, &scratch->nxnx_a);
    matrix_set_unit(&scratch->nxnx_b);
    matrix_subtract_into(&scratch->nxnx_b, &scratch->nxnx_a, &scratch->nxnx_c);
    matrix_multiply_into(&scratch->nxnx_c, &kalman->p, &scratch->nxnx_a);
    matrix_copy(&scratch->nxnx_a, &kalman->p);

    matrix_copy(&kalman->x, &kalman->_x);

    kalman->singular = false;
    return true;
}

// One full cycle of the filter. Give NULL as the input matrix if the model has
// no control input. The input matrix keeps its last value then.
bool kalman_step(kalman_t* kalman, matrix_t* input_matrix, matrix_t* measurement_matrix)
{
    ASSERT(kalman != NULL);
    ASSERT(measurement_matrix != NULL);

    if(input_matrix != NULL)
    {
        kalman_set_input_matrix(kalman, input_matrix);
    }
    kalman_set_measurement_matrix(kalman, measurement_matrix);

    kalman_predict(kalman);
    return kalman_update(kalman);
}

// Results

matrix_t* kalman_get_state_matrix(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    return &kalman->x;
}

matrix_t* kalman_get_covariance_matrix(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    return &kalman->p;
}

matrix_t* kalman_get_gain_matrix(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    return &kalman->k;
}

void kalman_free(kalman_t* kalman)
{
    ASSERT(kalman != NULL);

    if(kalman->dynamic_alloc)
    {
        free(kalman->mempool);
        kalman->mempool = NULL;
        kalman->dynamic_alloc = false;
    }
}
