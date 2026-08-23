#ifndef TEST
#include <sptk/estimate/ukf.h>
#include <sptk/core/defs.h>
#else
#include "ukf.h"
#include "defs.h"
#endif

#include <math.h>

static void ukf_build_matrices(ukf_t* ukf, real_t* mempool);
static void ukf_build_weights(ukf_t* ukf);
static real_t ukf_spreading(const ukf_t* ukf);

// Allocation

ukf_t ukf_alloc(uint32_t ni, uint32_t nx, uint32_t ny)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);

    ukf_t ukf;
    real_t* mempool;

    mempool = (real_t*)malloc(sizeof(real_t)*UKF_MEMPOOL_SIZE(ni, nx, ny));

    ukf.ni = ni;
    ukf.nx = nx;
    ukf.ny = ny;
    ukf.mempool = mempool;
    ukf.state_function = NULL;
    ukf.measurement_function = NULL;
    ukf.alpha = UKF_DEFAULT_ALPHA;
    ukf.beta = UKF_DEFAULT_BETA;
    ukf.kappa = UKF_DEFAULT_KAPPA;
    ukf.singular = false;
    ukf.dynamic_alloc = true;

    ukf_build_matrices(&ukf, mempool);
    ukf_build_weights(&ukf);

    return ukf;
}

ukf_t ukf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool)
{
    ASSERT(ni > 0);
    ASSERT(nx > 0);
    ASSERT(ny > 0);
    ASSERT(mempool != NULL);

    ukf_t ukf;

    ukf.ni = ni;
    ukf.nx = nx;
    ukf.ny = ny;
    ukf.mempool = mempool;
    ukf.state_function = NULL;
    ukf.measurement_function = NULL;
    ukf.alpha = UKF_DEFAULT_ALPHA;
    ukf.beta = UKF_DEFAULT_BETA;
    ukf.kappa = UKF_DEFAULT_KAPPA;
    ukf.singular = false;
    ukf.dynamic_alloc = false;

    ukf_build_matrices(&ukf, mempool);
    ukf_build_weights(&ukf);

    return ukf;
}

static void ukf_build_matrices(ukf_t* ukf, real_t* mempool)
{
    uint32_t ni = ukf->ni;
    uint32_t nx = ukf->nx;
    uint32_t ny = ukf->ny;
    uint32_t points = UKF_POINT_COUNT(nx);
    real_t* pool = mempool;

    // Each matrix takes its memory from the pool one after another, thus the
    // whole filter holds one block and not two dozen.
    ukf->x = matrix_static_alloc(nx, 1, pool);              pool += nx;
    ukf->y = matrix_static_alloc(ny, 1, pool);              pool += ny;
    ukf->u = matrix_static_alloc(ni, 1, pool);              pool += ni;
    ukf->p = matrix_static_alloc(nx, nx, pool);             pool += nx*nx;
    ukf->q = matrix_static_alloc(nx, nx, pool);             pool += nx*nx;
    ukf->r = matrix_static_alloc(ny, ny, pool);             pool += ny*ny;
    ukf->k = matrix_static_alloc(nx, ny, pool);             pool += nx*ny;

    ukf->scratch.points = matrix_static_alloc(nx, points, pool);
    pool += nx*points;
    ukf->scratch.seen = matrix_static_alloc(ny, points, pool);
    pool += ny*points;
    ukf->scratch.weight_mean = matrix_static_alloc(points, 1, pool);
    pool += points;
    ukf->scratch.weight_spread = matrix_static_alloc(points, 1, pool);
    pool += points;
    ukf->scratch.factor = matrix_static_alloc(nx, nx, pool);    pool += nx*nx;
    ukf->scratch.nxnx_a = matrix_static_alloc(nx, nx, pool);    pool += nx*nx;
    ukf->scratch.nxnx_b = matrix_static_alloc(nx, nx, pool);    pool += nx*nx;
    ukf->scratch.moved = matrix_static_alloc(nx, points, pool);  pool += nx*points;
    ukf->scratch.nxny_a = matrix_static_alloc(nx, ny, pool);    pool += nx*ny;
    ukf->scratch.nxny_b = matrix_static_alloc(nx, ny, pool);    pool += nx*ny;
    ukf->scratch.nyny_a = matrix_static_alloc(ny, ny, pool);    pool += ny*ny;
    ukf->scratch.nyny_b = matrix_static_alloc(ny, ny, pool);    pool += ny*ny;
    ukf->scratch.measured = matrix_static_alloc(ny, points, pool);
    pool += ny*points;
    ukf->scratch.augmented = matrix_static_alloc(ny, 2*ny, pool); pool += 2*ny*ny;
    ukf->scratch.nx1_a = matrix_static_alloc(nx, 1, pool);      pool += nx;
    ukf->scratch.nx1_b = matrix_static_alloc(nx, 1, pool);      pool += nx;
    ukf->scratch.nx1_c = matrix_static_alloc(nx, 1, pool);      pool += nx;
    ukf->scratch.nx1_d = matrix_static_alloc(nx, 1, pool);      pool += nx;
    ukf->scratch.ny1_a = matrix_static_alloc(ny, 1, pool);      pool += ny;
    ukf->scratch.ny1_b = matrix_static_alloc(ny, 1, pool);      pool += ny;
    ukf->scratch.ny1_c = matrix_static_alloc(ny, 1, pool);      pool += ny;
    ukf->scratch.ny1_d = matrix_static_alloc(ny, 1, pool);      pool += ny;

    matrix_set_zero(&ukf->x);
    matrix_set_zero(&ukf->y);
    matrix_set_zero(&ukf->u);
    matrix_set_unit(&ukf->p);
    matrix_set_zero(&ukf->q);
    matrix_set_unit(&ukf->r);
    matrix_set_zero(&ukf->k);
}

// How far out the points stand.
//
// The books write this as nx plus lambda, where lambda is alpha squared times
// nx plus kappa, less nx. Written that way it takes nx off and then puts it
// back, and for a small alpha those are two nearly equal numbers whose
// difference is the answer. At 32 bits that alone loses several digits before
// anything else has happened.
//
// The two nx cancel exactly, thus the same number is alpha squared times nx
// plus kappa, and written that way there is nothing to lose.
static real_t ukf_spreading(const ukf_t* ukf)
{
    return (ukf->alpha * ukf->alpha) * ((real_t)ukf->nx + ukf->kappa);
}

static void ukf_build_weights(ukf_t* ukf)
{
    uint32_t points = UKF_POINT_COUNT(ukf->nx);
    real_t nx = (real_t)ukf->nx;
    real_t total = ukf_spreading(ukf);

    // The point at the middle carries one weight, and the 2nx points around it
    // carry another. The weight for the spread differs from the weight for the
    // mean at the middle point only, and that difference is what beta sets.
    //
    // The weight at the middle is lambda over the spreading, which is the same
    // as one less nx over the spreading. Written that way lambda need never be
    // formed at all.
    real_t middle_mean = REAL_C(1.0) - (nx / total);
    real_t middle_spread = middle_mean
                           + (REAL_C(1.0) - (ukf->alpha * ukf->alpha)
                              + ukf->beta);
    real_t other = REAL_C(1.0) / (REAL_C(2.0) * total);

    matrix_add_element(&ukf->scratch.weight_mean, 0, 0, middle_mean);
    matrix_add_element(&ukf->scratch.weight_spread, 0, 0, middle_spread);

    for(uint32_t index = 1; index < points; index++)
    {
        matrix_add_element(&ukf->scratch.weight_mean, index, 0, other);
        matrix_add_element(&ukf->scratch.weight_spread, index, 0, other);
    }
}

// Setting

void ukf_set_state_function(ukf_t* ukf, ukf_state_function_t function)
{
    ASSERT(ukf != NULL);

    ukf->state_function = function;
}

void ukf_set_measurement_function(ukf_t* ukf, ukf_measurement_function_t function)
{
    ASSERT(ukf != NULL);

    ukf->measurement_function = function;
}

bool ukf_is_valid_spread(uint32_t nx, real_t alpha, real_t kappa)
{
    if(alpha <= REAL_C(0.0))
    {
        return false;
    }

    real_t spreading = (alpha * alpha) * ((real_t)nx + kappa);

    // The weights are about one divided by this. Below the limit they are so
    // large that their sum, which must be 1, is lost in the rounding, and
    // every mean and every spread the filter works out is that sum.
    return spreading >= UKF_MIN_SPREAD;
}

bool ukf_set_spread(ukf_t* ukf, real_t alpha, real_t beta, real_t kappa)
{
    ASSERT(ukf != NULL);

    if(!ukf_is_valid_spread(ukf->nx, alpha, kappa))
    {
        return false;
    }

    ukf->alpha = alpha;
    ukf->beta = beta;
    ukf->kappa = kappa;

    ukf_build_weights(ukf);

    return true;
}

void ukf_set_state_matrix(ukf_t* ukf, matrix_t* state_matrix)
{
    ASSERT(ukf != NULL);
    ASSERT(state_matrix != NULL);

    matrix_copy(state_matrix, &ukf->x);
}

void ukf_set_covariance_matrix(ukf_t* ukf, matrix_t* covariance_matrix)
{
    ASSERT(ukf != NULL);
    ASSERT(covariance_matrix != NULL);

    matrix_copy(covariance_matrix, &ukf->p);
}

void ukf_set_process_noise_covariance_matrix(ukf_t* ukf, matrix_t* process_noise)
{
    ASSERT(ukf != NULL);
    ASSERT(process_noise != NULL);

    matrix_copy(process_noise, &ukf->q);
}

void ukf_set_measurement_covariance_matrix(ukf_t* ukf, matrix_t* measurement_noise)
{
    ASSERT(ukf != NULL);
    ASSERT(measurement_noise != NULL);

    matrix_copy(measurement_noise, &ukf->r);
}

void ukf_set_input_matrix(ukf_t* ukf, matrix_t* input_matrix)
{
    ASSERT(ukf != NULL);
    ASSERT(input_matrix != NULL);

    matrix_copy(input_matrix, &ukf->u);
}

void ukf_set_measurement_matrix(ukf_t* ukf, matrix_t* measurement_matrix)
{
    ASSERT(ukf != NULL);
    ASSERT(measurement_matrix != NULL);

    matrix_copy(measurement_matrix, &ukf->y);
}

// The points

bool ukf_place_points_into(ukf_t* ukf, matrix_t* dest)
{
    ASSERT(ukf != NULL);
    ASSERT(dest != NULL);

    uint32_t nx = ukf->nx;
    real_t total = ukf_spreading(ukf);

    // The spread, made larger by how far out the points are to stand. The
    // factor of that is the shape of the spread: each of its columns is a
    // direction, as long as the spread reaches that way.
    matrix_multiply_scalar_into(&ukf->p, total, &ukf->scratch.nxnx_a);

    if(!matrix_cholesky_into(&ukf->scratch.nxnx_a, &ukf->scratch.factor))
    {
        // The covariance is no longer a real spread. Placing points from it
        // would give a set that means nothing, thus the filter stops here.
        ukf->singular = true;
        return false;
    }

    // The first point stands at the middle, and the rest stand one step out
    // along each direction of the shape, both ways.
    for(uint32_t row = 0; row < nx; row++)
    {
        real_t middle = matrix_get_element(&ukf->x, row, 0);

        matrix_add_element(dest, row, 0, middle);

        for(uint32_t column = 0; column < nx; column++)
        {
            real_t step = matrix_get_element(&ukf->scratch.factor, row, column);

            matrix_add_element(dest, row, column + 1u, middle + step);
            matrix_add_element(dest, row, column + 1u + nx, middle - step);
        }
    }

    return true;
}

// The middle of a set of points, each carrying its weight.
static void ukf_mean_of_points(const matrix_t* points, const matrix_t* weights,
                               uint32_t rows, uint32_t count, matrix_t* dest)
{
    for(uint32_t row = 0; row < rows; row++)
    {
        real_t total = REAL_C(0.0);

        for(uint32_t index = 0; index < count; index++)
        {
            total += matrix_get_element((matrix_t*)weights, index, 0)
                     * matrix_get_element((matrix_t*)points, row, index);
        }

        matrix_add_element(dest, row, 0, total);
    }
}

// The spread of a set of points about a middle, each carrying its weight, plus
// a noise that is added to the result.
static void ukf_spread_of_points(const matrix_t* points, const matrix_t* middle,
                                 const matrix_t* weights, uint32_t rows,
                                 uint32_t count, const matrix_t* noise,
                                 matrix_t* dest)
{
    for(uint32_t i = 0; i < rows; i++)
    {
        for(uint32_t j = 0; j < rows; j++)
        {
            real_t total = REAL_C(0.0);

            for(uint32_t index = 0; index < count; index++)
            {
                real_t first = matrix_get_element((matrix_t*)points, i, index)
                               - matrix_get_element((matrix_t*)middle, i, 0);
                real_t second = matrix_get_element((matrix_t*)points, j, index)
                                - matrix_get_element((matrix_t*)middle, j, 0);

                total += matrix_get_element((matrix_t*)weights, index, 0)
                         * first * second;
            }

            total += matrix_get_element((matrix_t*)noise, i, j);
            matrix_add_element(dest, i, j, total);
        }
    }
}

// The spread of one set of points against another, which says how the two lean
// on each other.
static void ukf_spread_between(const matrix_t* first, const matrix_t* first_mean,
                               const matrix_t* second,
                               const matrix_t* second_mean,
                               const matrix_t* weights, uint32_t rows,
                               uint32_t columns, uint32_t count, matrix_t* dest)
{
    for(uint32_t i = 0; i < rows; i++)
    {
        for(uint32_t j = 0; j < columns; j++)
        {
            real_t total = REAL_C(0.0);

            for(uint32_t index = 0; index < count; index++)
            {
                real_t a = matrix_get_element((matrix_t*)first, i, index)
                           - matrix_get_element((matrix_t*)first_mean, i, 0);
                real_t b = matrix_get_element((matrix_t*)second, j, index)
                           - matrix_get_element((matrix_t*)second_mean, j, 0);

                total += matrix_get_element((matrix_t*)weights, index, 0)
                         * a * b;
            }

            matrix_add_element(dest, i, j, total);
        }
    }
}

// Take one column of a set of points out as a matrix of its own.
static void ukf_take_point(const matrix_t* points, uint32_t index,
                           uint32_t rows, matrix_t* dest)
{
    for(uint32_t row = 0; row < rows; row++)
    {
        matrix_add_element(dest, row, 0,
                           matrix_get_element((matrix_t*)points, row, index));
    }
}

// Put one column of a set of points back.
static void ukf_put_point(matrix_t* points, uint32_t index, uint32_t rows,
                          const matrix_t* source)
{
    for(uint32_t row = 0; row < rows; row++)
    {
        matrix_add_element(points, row, index,
                           matrix_get_element((matrix_t*)source, row, 0));
    }
}

bool ukf_predict(ukf_t* ukf)
{
    ASSERT(ukf != NULL);
    ASSERT(ukf->state_function != NULL);

    uint32_t nx = ukf->nx;
    uint32_t count = UKF_POINT_COUNT(nx);

    if(!ukf_place_points_into(ukf, &ukf->scratch.points))
    {
        return false;
    }

    // Every point goes through the model ITSELF. This is the whole of the
    // difference from the extended filter: no line is laid against the model
    // anywhere, thus a bend in it is carried rather than lost.
    for(uint32_t index = 0; index < count; index++)
    {
        ukf_take_point(&ukf->scratch.points, index, nx, &ukf->scratch.nx1_a);
        ukf->state_function(&ukf->scratch.nx1_a, &ukf->u, &ukf->scratch.nx1_b);
        ukf_put_point(&ukf->scratch.moved, index, nx, &ukf->scratch.nx1_b);
    }

    matrix_copy(&ukf->scratch.moved, &ukf->scratch.points);

    ukf_mean_of_points(&ukf->scratch.points, &ukf->scratch.weight_mean, nx,
                       count, &ukf->scratch.nx1_c);
    ukf_spread_of_points(&ukf->scratch.points, &ukf->scratch.nx1_c,
                         &ukf->scratch.weight_spread, nx, count, &ukf->q,
                         &ukf->scratch.nxnx_b);

    matrix_copy(&ukf->scratch.nx1_c, &ukf->x);
    matrix_copy(&ukf->scratch.nxnx_b, &ukf->p);

    ukf->singular = false;

    return true;
}

bool ukf_update(ukf_t* ukf)
{
    ASSERT(ukf != NULL);
    ASSERT(ukf->measurement_function != NULL);

    uint32_t nx = ukf->nx;
    uint32_t ny = ukf->ny;
    uint32_t count = UKF_POINT_COUNT(nx);

    // The points are placed again from the state as it now stands. Using the
    // ones the prediction left would miss the spread that the process noise
    // added to the covariance.
    if(!ukf_place_points_into(ukf, &ukf->scratch.points))
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        ukf_take_point(&ukf->scratch.points, index, nx, &ukf->scratch.nx1_a);
        ukf->measurement_function(&ukf->scratch.nx1_a, &ukf->scratch.ny1_a);
        ukf_put_point(&ukf->scratch.measured, index, ny, &ukf->scratch.ny1_a);
    }

    matrix_copy(&ukf->scratch.measured, &ukf->scratch.seen);

    // What the filter expects to measure, and how far that spreads.
    ukf_mean_of_points(&ukf->scratch.seen, &ukf->scratch.weight_mean, ny,
                       count, &ukf->scratch.ny1_b);
    ukf_spread_of_points(&ukf->scratch.seen, &ukf->scratch.ny1_b,
                         &ukf->scratch.weight_spread, ny, count, &ukf->r,
                         &ukf->scratch.nyny_a);

    // The middle of the points must be worked out again here as well, because
    // the weights for the mean and for the spread are not the same.
    ukf_mean_of_points(&ukf->scratch.points, &ukf->scratch.weight_mean, nx,
                       count, &ukf->scratch.nx1_c);

    // How the state and the measurement lean on each other.
    ukf_spread_between(&ukf->scratch.points, &ukf->scratch.nx1_c,
                       &ukf->scratch.seen, &ukf->scratch.ny1_b,
                       &ukf->scratch.weight_spread, nx, ny, count,
                       &ukf->scratch.nxny_a);

    if(!matrix_inverse_into(&ukf->scratch.nyny_a, &ukf->scratch.nyny_b,
                            &ukf->scratch.augmented))
    {
        ukf->singular = true;
        return false;
    }

    // The gain: how far to move the state for each unit that the measurement
    // differs from what was expected.
    matrix_multiply_into(&ukf->scratch.nxny_a, &ukf->scratch.nyny_b, &ukf->k);

    // Move the state by the gain times the surprise.
    matrix_subtract_into(&ukf->y, &ukf->scratch.ny1_b, &ukf->scratch.ny1_c);
    matrix_multiply_into(&ukf->k, &ukf->scratch.ny1_c, &ukf->scratch.nx1_d);
    matrix_add_into(&ukf->x, &ukf->scratch.nx1_d, &ukf->scratch.nx1_b);
    matrix_copy(&ukf->scratch.nx1_b, &ukf->x);

    // Take away the spread that the measurement explained.
    matrix_multiply_into(&ukf->k, &ukf->scratch.nyny_a, &ukf->scratch.nxny_b);
    matrix_transpose_into(&ukf->k, &ukf->scratch.nxnx_a);
    matrix_multiply_into(&ukf->scratch.nxny_b, &ukf->scratch.nxnx_a,
                         &ukf->scratch.nxnx_b);
    matrix_subtract_into(&ukf->p, &ukf->scratch.nxnx_b, &ukf->scratch.nxnx_a);
    matrix_copy(&ukf->scratch.nxnx_a, &ukf->p);

    ukf->singular = false;

    return true;
}

bool ukf_step(ukf_t* ukf, matrix_t* input_matrix, matrix_t* measurement_matrix)
{
    ASSERT(ukf != NULL);

    if(input_matrix != NULL)
    {
        ukf_set_input_matrix(ukf, input_matrix);
    }
    if(measurement_matrix != NULL)
    {
        ukf_set_measurement_matrix(ukf, measurement_matrix);
    }

    if(!ukf_predict(ukf))
    {
        return false;
    }

    return ukf_update(ukf);
}

matrix_t* ukf_get_state_matrix(ukf_t* ukf)
{
    ASSERT(ukf != NULL);

    return &ukf->x;
}

matrix_t* ukf_get_covariance_matrix(ukf_t* ukf)
{
    ASSERT(ukf != NULL);

    return &ukf->p;
}

matrix_t* ukf_get_gain_matrix(ukf_t* ukf)
{
    ASSERT(ukf != NULL);

    return &ukf->k;
}

void ukf_free(ukf_t* ukf)
{
    ASSERT(ukf != NULL);

    if(ukf->dynamic_alloc)
    {
        free(ukf->mempool);
        ukf->mempool = NULL;
        ukf->dynamic_alloc = false;
    }
}
