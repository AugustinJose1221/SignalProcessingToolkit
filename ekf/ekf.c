#ifndef TEST
#include <ekf/ekf.h>
#include <common/defs.h>
#else
#include "ekf.h"
#include "defs.h"
#endif

static matrix_t ekf_take_from_pool(float** pool, uint32_t m, uint32_t n);
static void ekf_build_matrices(ekf_t* ekf, float* mempool);

// Allocation

ekf_t ekf_alloc(uint32_t ni, uint32_t nx, uint32_t ny)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);

    ekf_t ekf;
    float* mempool;

    mempool = (float*)malloc(sizeof(float)*EKF_MEMPOOL_SIZE(ni, nx, ny));

    ekf.ni = ni;
    ekf.nx = nx;
    ekf.ny = ny;
    ekf.mempool = mempool;
    ekf.state_function = NULL;
    ekf.measurement_function = NULL;
    ekf.derivative_step = EKF_DEFAULT_DERIVATIVE_STEP;
    ekf.singular = false;
    ekf.dynamic_alloc = true;

    ekf_build_matrices(&ekf, mempool);

    return ekf;
}

ekf_t ekf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, float* mempool)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);
    ASSERT(mempool != NULL);

    ekf_t ekf;

    ekf.ni = ni;
    ekf.nx = nx;
    ekf.ny = ny;
    ekf.mempool = mempool;
    ekf.state_function = NULL;
    ekf.measurement_function = NULL;
    ekf.derivative_step = EKF_DEFAULT_DERIVATIVE_STEP;
    ekf.singular = false;
    ekf.dynamic_alloc = false;

    ekf_build_matrices(&ekf, mempool);

    return ekf;
}

// The two allocation functions share this layout.
static void ekf_build_matrices(ekf_t* ekf, float* mempool)
{
    uint32_t ni = ekf->ni;
    uint32_t nx = ekf->nx;
    uint32_t ny = ekf->ny;
    float* pool = mempool;

    ekf->x = ekf_take_from_pool(&pool, nx, 1);
    ekf->y = ekf_take_from_pool(&pool, ny, 1);
    ekf->u = ekf_take_from_pool(&pool, ni, 1);
    ekf->p = ekf_take_from_pool(&pool, nx, nx);
    ekf->q = ekf_take_from_pool(&pool, nx, nx);
    ekf->r = ekf_take_from_pool(&pool, ny, ny);
    ekf->a = ekf_take_from_pool(&pool, nx, nx);
    ekf->c = ekf_take_from_pool(&pool, ny, nx);
    ekf->k = ekf_take_from_pool(&pool, nx, ny);

    ekf->scratch.nxnx_a    = ekf_take_from_pool(&pool, nx, nx);
    ekf->scratch.nxnx_b    = ekf_take_from_pool(&pool, nx, nx);
    ekf->scratch.nxnx_c    = ekf_take_from_pool(&pool, nx, nx);
    ekf->scratch.nxny_a    = ekf_take_from_pool(&pool, nx, ny);
    ekf->scratch.nxny_b    = ekf_take_from_pool(&pool, nx, ny);
    ekf->scratch.nynx_a    = ekf_take_from_pool(&pool, ny, nx);
    ekf->scratch.nyny_a    = ekf_take_from_pool(&pool, ny, ny);
    ekf->scratch.nyny_b    = ekf_take_from_pool(&pool, ny, ny);
    ekf->scratch.nyny_c    = ekf_take_from_pool(&pool, ny, ny);
    ekf->scratch.augmented = ekf_take_from_pool(&pool, ny, 2*ny);
    ekf->scratch.nx1_a     = ekf_take_from_pool(&pool, nx, 1);
    ekf->scratch.nx1_b     = ekf_take_from_pool(&pool, nx, 1);
    ekf->scratch.nx1_c     = ekf_take_from_pool(&pool, nx, 1);
    ekf->scratch.nx1_d     = ekf_take_from_pool(&pool, nx, 1);
    ekf->scratch.ny1_a     = ekf_take_from_pool(&pool, ny, 1);
    ekf->scratch.ny1_b     = ekf_take_from_pool(&pool, ny, 1);
    ekf->scratch.ny1_c     = ekf_take_from_pool(&pool, ny, 1);

    for(uint32_t index = 0; index < EKF_MEMPOOL_SIZE(ni, nx, ny); index++)
    {
        mempool[index] = 0.0f;
    }
}

static matrix_t ekf_take_from_pool(float** pool, uint32_t m, uint32_t n)
{
    matrix_t matrix = matrix_static_alloc(m, n, *pool);
    *pool += m*n;
    return matrix;
}

// Configuration

void ekf_set_state_function(ekf_t* ekf, ekf_state_function_t function)
{
    ASSERT(ekf != NULL);
    ASSERT(function != NULL);

    ekf->state_function = function;
}

void ekf_set_measurement_function(ekf_t* ekf, ekf_measurement_function_t function)
{
    ASSERT(ekf != NULL);
    ASSERT(function != NULL);

    ekf->measurement_function = function;
}

void ekf_set_derivative_step(ekf_t* ekf, float step)
{
    ASSERT(ekf != NULL);
    ASSERT(step > 0.0f);

    ekf->derivative_step = step;
}

void ekf_set_state_matrix(ekf_t* ekf, matrix_t* state_matrix)
{
    ASSERT(ekf != NULL);
    ASSERT(state_matrix != NULL);

    matrix_copy(state_matrix, &ekf->x);
}

void ekf_set_covariance_matrix(ekf_t* ekf, matrix_t* covariance_matrix)
{
    ASSERT(ekf != NULL);
    ASSERT(covariance_matrix != NULL);

    matrix_copy(covariance_matrix, &ekf->p);
}

void ekf_set_process_noise_covariance_matrix(ekf_t* ekf, matrix_t* process_noise)
{
    ASSERT(ekf != NULL);
    ASSERT(process_noise != NULL);

    matrix_copy(process_noise, &ekf->q);
}

void ekf_set_measurement_covariance_matrix(ekf_t* ekf, matrix_t* measurement_noise)
{
    ASSERT(ekf != NULL);
    ASSERT(measurement_noise != NULL);

    matrix_copy(measurement_noise, &ekf->r);
}

void ekf_set_input_matrix(ekf_t* ekf, matrix_t* input_matrix)
{
    ASSERT(ekf != NULL);
    ASSERT(input_matrix != NULL);

    matrix_copy(input_matrix, &ekf->u);
}

void ekf_set_measurement_matrix(ekf_t* ekf, matrix_t* measurement_matrix)
{
    ASSERT(ekf != NULL);
    ASSERT(measurement_matrix != NULL);

    matrix_copy(measurement_matrix, &ekf->y);
}

// The Jacobian matrices

void ekf_state_jacobian_into(ekf_t* ekf, matrix_t* dest)
{
    ASSERT(ekf != NULL);
    ASSERT(dest != NULL);
    ASSERT(ekf->state_function != NULL);
    ASSERT(dest->m == ekf->nx && dest->n == ekf->nx);

    matrix_t* moved = &ekf->scratch.nx1_a;
    matrix_t* higher = &ekf->scratch.nx1_b;
    matrix_t* lower = &ekf->scratch.nx1_c;
    float step = ekf->derivative_step;

    for(uint32_t column = 0; column < ekf->nx; column++)
    {
        matrix_copy(&ekf->x, moved);

        // Move one element of the state to each side and call the function.
        // The difference of the two results, divided by the whole distance,
        // gives the slope. This central difference holds a smaller error than
        // a difference to one side only.
        matrix_add_element(moved, column, 0,
                           matrix_get_element(&ekf->x, column, 0) + step);
        ekf->state_function(moved, &ekf->u, higher);

        matrix_add_element(moved, column, 0,
                           matrix_get_element(&ekf->x, column, 0) - step);
        ekf->state_function(moved, &ekf->u, lower);

        for(uint32_t row = 0; row < ekf->nx; row++)
        {
            float slope = (matrix_get_element(higher, row, 0)
                           - matrix_get_element(lower, row, 0)) / (2.0f * step);
            matrix_add_element(dest, row, column, slope);
        }
    }
}

void ekf_measurement_jacobian_into(ekf_t* ekf, matrix_t* dest)
{
    ASSERT(ekf != NULL);
    ASSERT(dest != NULL);
    ASSERT(ekf->measurement_function != NULL);
    ASSERT(dest->m == ekf->ny && dest->n == ekf->nx);

    matrix_t* moved = &ekf->scratch.nx1_a;
    matrix_t* higher = &ekf->scratch.ny1_a;
    matrix_t* lower = &ekf->scratch.ny1_b;
    float step = ekf->derivative_step;

    for(uint32_t column = 0; column < ekf->nx; column++)
    {
        matrix_copy(&ekf->x, moved);

        matrix_add_element(moved, column, 0,
                           matrix_get_element(&ekf->x, column, 0) + step);
        ekf->measurement_function(moved, higher);

        matrix_add_element(moved, column, 0,
                           matrix_get_element(&ekf->x, column, 0) - step);
        ekf->measurement_function(moved, lower);

        for(uint32_t row = 0; row < ekf->ny; row++)
        {
            float slope = (matrix_get_element(higher, row, 0)
                           - matrix_get_element(lower, row, 0)) / (2.0f * step);
            matrix_add_element(dest, row, column, slope);
        }
    }
}

// The filter

void ekf_predict(ekf_t* ekf)
{
    ASSERT(ekf != NULL);
    ASSERT(ekf->state_function != NULL);

    ekf_scratch_t* scratch = &ekf->scratch;

    // The Jacobian belongs to the state before the step, thus it comes first.
    ekf_state_jacobian_into(ekf, &ekf->a);

    // x = f(x, u)
    ekf->state_function(&ekf->x, &ekf->u, &scratch->nx1_d);
    matrix_copy(&scratch->nx1_d, &ekf->x);

    // P = A*P*A' + Q
    matrix_multiply_into(&ekf->a, &ekf->p, &scratch->nxnx_a);
    matrix_transpose_into(&ekf->a, &scratch->nxnx_b);
    matrix_multiply_into(&scratch->nxnx_a, &scratch->nxnx_b, &scratch->nxnx_c);
    matrix_add_into(&scratch->nxnx_c, &ekf->q, &ekf->p);
}

bool ekf_update(ekf_t* ekf)
{
    ASSERT(ekf != NULL);
    ASSERT(ekf->measurement_function != NULL);

    ekf_scratch_t* scratch = &ekf->scratch;

    ekf_measurement_jacobian_into(ekf, &ekf->c);

    // The innovation, which is the measurement less the measurement that the
    // present state would produce.
    ekf->measurement_function(&ekf->x, &scratch->ny1_c);
    matrix_subtract_into(&ekf->y, &scratch->ny1_c, &scratch->ny1_a);

    // The innovation covariance.
    matrix_multiply_into(&ekf->c, &ekf->p, &scratch->nynx_a);
    matrix_transpose_into(&ekf->c, &scratch->nxny_a);
    matrix_multiply_into(&scratch->nynx_a, &scratch->nxny_a, &scratch->nyny_a);
    matrix_add_into(&scratch->nyny_a, &ekf->r, &scratch->nyny_b);

    if(!matrix_inverse_into(&scratch->nyny_b, &scratch->nyny_c, &scratch->augmented))
    {
        ekf->singular = true;
        return false;
    }

    // The gain.
    matrix_multiply_into(&ekf->p, &scratch->nxny_a, &scratch->nxny_b);
    matrix_multiply_into(&scratch->nxny_b, &scratch->nyny_c, &ekf->k);

    // The corrected state.
    matrix_multiply_into(&ekf->k, &scratch->ny1_a, &scratch->nx1_b);
    matrix_add_into(&ekf->x, &scratch->nx1_b, &scratch->nx1_c);
    matrix_copy(&scratch->nx1_c, &ekf->x);

    // The corrected covariance.
    matrix_multiply_into(&ekf->k, &ekf->c, &scratch->nxnx_a);
    matrix_set_unit(&scratch->nxnx_b);
    matrix_subtract_into(&scratch->nxnx_b, &scratch->nxnx_a, &scratch->nxnx_c);
    matrix_multiply_into(&scratch->nxnx_c, &ekf->p, &scratch->nxnx_a);
    matrix_copy(&scratch->nxnx_a, &ekf->p);

    ekf->singular = false;
    return true;
}

bool ekf_step(ekf_t* ekf, matrix_t* input_matrix, matrix_t* measurement_matrix)
{
    ASSERT(ekf != NULL);
    ASSERT(measurement_matrix != NULL);

    if(input_matrix != NULL)
    {
        ekf_set_input_matrix(ekf, input_matrix);
    }
    ekf_set_measurement_matrix(ekf, measurement_matrix);

    ekf_predict(ekf);
    return ekf_update(ekf);
}

// Results

matrix_t* ekf_get_state_matrix(ekf_t* ekf)
{
    ASSERT(ekf != NULL);

    return &ekf->x;
}

matrix_t* ekf_get_covariance_matrix(ekf_t* ekf)
{
    ASSERT(ekf != NULL);

    return &ekf->p;
}

matrix_t* ekf_get_gain_matrix(ekf_t* ekf)
{
    ASSERT(ekf != NULL);

    return &ekf->k;
}

void ekf_free(ekf_t* ekf)
{
    ASSERT(ekf != NULL);

    if(ekf->dynamic_alloc)
    {
        free(ekf->mempool);
        ekf->mempool = NULL;
        ekf->dynamic_alloc = false;
    }
}
