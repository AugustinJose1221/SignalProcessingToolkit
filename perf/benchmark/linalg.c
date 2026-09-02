// The benchmarks of the linear algebra modules that modules.c and matrix.c do
// not already cover.
//
// A SQUARE OF 10 BY 10 wherever a matrix is asked for, because that is the
// size the table in README.md names and because it is the size at which the
// cost of the arithmetic has grown past the cost of the call.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/linalg/cmatrix.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/linalg/eigen.h>
#include <ffitt/linalg/lstsq.h>
#include <ffitt/linalg/matrix.h>
#include <ffitt/linalg/pmatrix.h>
#include <ffitt/linalg/poly.h>
#include <ffitt/linalg/quaternion.h>
#include <ffitt/linalg/vector2d.h>

#include <stdlib.h>

#define LINALG_ORDER        10u
#define LINALG_POINTS       256u
#define LINALG_POLY_ORDER   8u

static real_t linalg_x[LINALG_POINTS];
static real_t linalg_y[LINALG_POINTS];

static real_t linalg_random(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

static real_t linalg_one(real_t x)
{
    return x;
}

static void run_cmatrix_benchmark(void)
{
    cmatrix_t a = cmatrix_alloc(LINALG_ORDER, LINALG_ORDER);
    cmatrix_t b = cmatrix_alloc(LINALG_ORDER, LINALG_ORDER);
    cmatrix_t answer = cmatrix_alloc(LINALG_ORDER, LINALG_ORDER);
    cmatrix_t scratch = cmatrix_alloc(LINALG_ORDER, 2u * LINALG_ORDER);
    cmatrix_t square = cmatrix_alloc(LINALG_ORDER, LINALG_ORDER);
    cnum_t value = cnum_zero();

    for(uint32_t row = 0u; row < LINALG_ORDER; row++)
    {
        for(uint32_t column = 0u; column < LINALG_ORDER; column++)
        {
            real_t weight = (row == column) ? REAL_C(4.0) : REAL_C(0.0);

            cmatrix_add_element(&a, row, column,
                                cnum_make(weight + linalg_random(),
                                          linalg_random()));
            cmatrix_add_element(&b, row, column,
                                cnum_make(linalg_random(), linalg_random()));
        }
    }

    BENCHMARK_MEASURE("cmatrix", "multiply_into",
                      "multiply two 10 by 10 complex matrices",
                      LINALG_ORDER, 5000,
                      cmatrix_multiply_into(&a, &b, &answer));

    BENCHMARK_MEASURE("cmatrix", "determinant_into",
                      "the determinant of a 10 by 10 complex matrix",
                      LINALG_ORDER, 5000,
                      value = cmatrix_determinant_into(&a, &square));

    BENCHMARK_MEASURE("cmatrix", "inverse_into",
                      "invert a 10 by 10 complex matrix",
                      LINALG_ORDER, 2000,
                      (void)cmatrix_inverse_into(&a, &answer, &scratch));

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&answer);
    cmatrix_free(&scratch);
    cmatrix_free(&square);
    (void)value;
}

static void run_cnum_benchmark(void)
{
    cnum_t a = cnum_make(REAL_C(0.7), REAL_C(-0.3));
    cnum_t b = cnum_make(REAL_C(-0.2), REAL_C(0.9));
    cnum_t answer = cnum_zero();
    real_t size = REAL_C(0.0);

    BENCHMARK_MEASURE("cnum", "multiply",
                      "multiply two complex numbers",
                      1u, 500000,
                      answer = cnum_multiply(a, b));

    BENCHMARK_MEASURE("cnum", "divide",
                      "divide one complex number by another",
                      1u, 500000,
                      answer = cnum_divide(a, b));

    BENCHMARK_MEASURE("cnum", "magnitude",
                      "the size of a complex number",
                      1u, 500000,
                      size = cnum_magnitude(answer));
    (void)size;
}

static void run_eigen_benchmark(void)
{
    matrix_t matrix = matrix_alloc(LINALG_ORDER, LINALG_ORDER);
    matrix_t vectors = matrix_alloc(LINALG_ORDER, LINALG_ORDER);
    real_t values[LINALG_ORDER];
    real_t number = REAL_C(0.0);

    // Symmetric, because that is the shape the module is written for.
    for(uint32_t row = 0u; row < LINALG_ORDER; row++)
    {
        for(uint32_t column = row; column < LINALG_ORDER; column++)
        {
            real_t value = linalg_random()
                           + ((row == column) ? REAL_C(4.0) : REAL_C(0.0));

            matrix_add_element(&matrix, row, column, value);
            matrix_add_element(&matrix, column, row, value);
        }
    }

    BENCHMARK_MEASURE("eigen", "solve",
                      "the eigenvalues and vectors of a 10 by 10 matrix",
                      LINALG_ORDER, 500,
                      (void)eigen_solve(&matrix, values, &vectors));

    BENCHMARK_MEASURE("eigen", "condition",
                      "how badly conditioned that matrix is",
                      LINALG_ORDER, 100000,
                      number = eigen_condition(values, LINALG_ORDER));
    (void)number;

    matrix_free(&matrix);
    matrix_free(&vectors);
}

static void run_lstsq_benchmark(void)
{
    real_t coefficients[LINALG_POLY_ORDER + 1u];
    real_t centre = REAL_C(0.0);
    real_t width = REAL_C(1.0);
    real_t quality = REAL_C(0.0);

    BENCHMARK_MEASURE("lstsq", "polyfit_scaled",
                      "fit a curve of order 8 through 256 readings",
                      LINALG_POINTS, 2000,
                      (void)lstsq_polyfit_scaled(linalg_x, linalg_y,
                                                 LINALG_POINTS,
                                                 LINALG_POLY_ORDER,
                                                 coefficients, &centre,
                                                 &width));

    BENCHMARK_MEASURE("lstsq", "fit_quality_scaled",
                      "how well that curve holds the readings",
                      LINALG_POINTS, 5000,
                      quality = lstsq_fit_quality_scaled(linalg_x, linalg_y,
                                                         LINALG_POINTS,
                                                         coefficients,
                                                         LINALG_POLY_ORDER,
                                                         centre, width));
    (void)quality;
}

static void run_poly_benchmark(void)
{
    real_t coefficient[LINALG_POLY_ORDER + 1u];
    real_t answer[(2u * LINALG_POLY_ORDER) + 1u];
    cnum_t roots[LINALG_POLY_ORDER];
    real_t value = REAL_C(0.0);

    for(uint32_t index = 0u; index <= LINALG_POLY_ORDER; index++)
    {
        coefficient[index] = linalg_random() + REAL_C(1.0);
    }

    BENCHMARK_MEASURE("poly", "evaluate",
                      "read a curve of order 8 at one place",
                      LINALG_POLY_ORDER, 500000,
                      value = poly_evaluate(coefficient, LINALG_POLY_ORDER,
                                            REAL_C(0.5)));
    (void)value;

    BENCHMARK_MEASURE("poly", "multiply",
                      "multiply two curves of order 8",
                      LINALG_POLY_ORDER, 200000,
                      (void)poly_multiply(coefficient, LINALG_POLY_ORDER,
                                          coefficient, LINALG_POLY_ORDER,
                                          answer,
                                          (2u * LINALG_POLY_ORDER) + 1u));

    // ORDER 4 AND NOT 8. The header caps the order whose roots this module
    // will find, and the cap follows the width: 4 at 32 bits and 12 at 64.
    // Asked for 8, the 32 bit build gives false at once and measures 0.03
    // microseconds against the 41 of the 64 bit build. That row would have
    // read as a speed and been a refusal. 4 is the highest order both widths
    // will do, thus it is the only order the two can be compared at.
    BENCHMARK_MEASURE("poly", "roots",
                      "every root of a curve of order 4",
                      4u, 2000,
                      (void)poly_roots(coefficient, 4u, roots));
}

static void run_quaternion_benchmark(void)
{
    quaternion_t a = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), REAL_C(0.5));
    quaternion_t b = quaternion_from_axis_angle(REAL_C(1.0), REAL_C(0.0),
                                                REAL_C(0.0), REAL_C(0.3));
    quaternion_t answer = quaternion_identity();
    matrix_t rotation = matrix_alloc(3u, 3u);
    real_t x = REAL_C(0.0);
    real_t y = REAL_C(0.0);
    real_t z = REAL_C(0.0);

    BENCHMARK_MEASURE("quaternion", "multiply",
                      "join two turns into one",
                      1u, 500000,
                      answer = quaternion_multiply(a, b));

    BENCHMARK_MEASURE("quaternion", "rotate",
                      "turn one point by a quaternion",
                      1u, 500000,
                      quaternion_rotate(a, REAL_C(1.0), REAL_C(2.0),
                                        REAL_C(3.0), &x, &y, &z));

    BENCHMARK_MEASURE("quaternion", "to_matrix_into",
                      "the same turn written as a 3 by 3 matrix",
                      3u, 200000,
                      quaternion_to_matrix_into(a, &rotation));

    BENCHMARK_MEASURE("quaternion", "slerp",
                      "the turn part way between two others",
                      1u, 200000,
                      answer = quaternion_slerp(a, b, REAL_C(0.5)));

    BENCHMARK_MEASURE("quaternion", "integrate",
                      "carry a turn forward by a rate over a step",
                      1u, 200000,
                      answer = quaternion_integrate(a, REAL_C(0.1),
                                                    REAL_C(0.2), REAL_C(0.3),
                                                    REAL_C(0.01)));
    (void)answer;

    matrix_free(&rotation);
}

static void run_pmatrix_benchmark(void)
{
    pmatrix_t pmatrix = pmatrix_alloc(LINALG_ORDER, LINALG_ORDER);
    matrix_t answer = matrix_alloc(LINALG_ORDER, LINALG_ORDER);

    for(uint32_t row = 0u; row < LINALG_ORDER; row++)
    {
        for(uint32_t column = 0u; column < LINALG_ORDER; column++)
        {
            pmatrix_add_element(&pmatrix, row, column, linalg_one);
        }
    }

    BENCHMARK_MEASURE("pmatrix", "evaluate_into",
                      "read a 10 by 10 matrix of functions at one place",
                      LINALG_ORDER, 20000,
                      pmatrix_evaluate_into(&pmatrix, REAL_C(0.5), &answer));

    pmatrix_free(&pmatrix);
    matrix_free(&answer);
}

static void run_cholesky_benchmark(void)
{
    matrix_t matrix = matrix_alloc(LINALG_ORDER, LINALG_ORDER);
    matrix_t answer = matrix_alloc(LINALG_ORDER, LINALG_ORDER);

    // Diagonally strong, thus it has a factor of Cholesky to find.
    for(uint32_t row = 0u; row < LINALG_ORDER; row++)
    {
        for(uint32_t column = row; column < LINALG_ORDER; column++)
        {
            real_t value = (linalg_random() * REAL_C(0.1))
                           + ((row == column) ? REAL_C(4.0) : REAL_C(0.0));

            matrix_add_element(&matrix, row, column, value);
            matrix_add_element(&matrix, column, row, value);
        }
    }

    BENCHMARK_MEASURE("matrix", "cholesky_into",
                      "the factor of Cholesky of a 10 by 10 matrix",
                      LINALG_ORDER, 20000,
                      (void)matrix_cholesky_into(&matrix, &answer));

    matrix_free(&matrix);
    matrix_free(&answer);
}

static void run_vector2d_benchmark(void)
{
    vector_t a = vector2d_alloc();
    vector_t b = vector2d_alloc();
    real_t value = REAL_C(0.0);

    vector2d_add_point_at_index(&a, 0u, REAL_C(0.3));
    vector2d_add_point_at_index(&a, 1u, REAL_C(0.4));
    vector2d_add_point_at_index(&b, 0u, REAL_C(-0.2));
    vector2d_add_point_at_index(&b, 1u, REAL_C(0.6));

    BENCHMARK_MEASURE("vector2d", "dot_product",
                      "the dot product of two vectors of two",
                      2u, 500000,
                      value = vector2d_dot_product(&a, &b));

    BENCHMARK_MEASURE("vector2d", "norm",
                      "the length of a vector of two",
                      2u, 500000,
                      value = vector2d_norm(&a));
    (void)value;

    vector_free(&a);
    vector_free(&b);
}

void run_linalg_benchmark(void)
{
    for(uint32_t index = 0u; index < LINALG_POINTS; index++)
    {
        linalg_x[index] = ((real_t)index / (real_t)LINALG_POINTS)
                          - REAL_C(0.5);
        linalg_y[index] = linalg_random();
    }

    run_cmatrix_benchmark();
    run_cnum_benchmark();
    run_eigen_benchmark();
    run_lstsq_benchmark();
    run_poly_benchmark();
    run_quaternion_benchmark();
    run_pmatrix_benchmark();
    run_cholesky_benchmark();
    run_vector2d_benchmark();
}
