#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/linalg/matrix.h>

#include <stddef.h>
#include <stdlib.h>

static const uint32_t ORDERS[] = {2, 4, 8, 16, 32};
static const uint32_t ORDER_COUNT = sizeof(ORDERS)/sizeof(ORDERS[0]);

// The determinant uses the rule of the cofactors, thus its cost grows with the
// factorial of the order. An order above 9 takes too long to measure.
static const uint32_t DETERMINANT_ORDERS[] = {2, 4, 6, 8, 10};
static const uint32_t DETERMINANT_ORDER_COUNT =
    sizeof(DETERMINANT_ORDERS)/sizeof(DETERMINANT_ORDERS[0]);

static matrix_t make_random_matrix(uint32_t m, uint32_t n)
{
    matrix_t matrix = matrix_alloc(m, n);

    for(uint32_t i = 0; i < m; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            matrix_add_element(&matrix, i, j,
                               ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5));
        }
    }

    return matrix;
}

// A matrix where each element of the diagonal is larger than the sum of the
// other elements of its row. Such a matrix always has an inverse.
static matrix_t make_invertible_matrix(uint32_t order)
{
    matrix_t matrix = make_random_matrix(order, order);

    for(uint32_t i = 0; i < order; i++)
    {
        real_t sum = REAL_C(0.0);
        for(uint32_t j = 0; j < order; j++)
        {
            if(i != j)
            {
                real_t value = matrix_get_element(&matrix, i, j);
                sum += (value < REAL_C(0.0)) ? -value : value;
            }
        }
        matrix_add_element(&matrix, i, i, sum + REAL_C(1.0));
    }

    return matrix;
}

void run_matrix_benchmark(void)
{
    for(uint32_t index = 0; index < ORDER_COUNT; index++)
    {
        uint32_t order = ORDERS[index];
        uint32_t repeats = 2000 / order;

        matrix_t a = make_random_matrix(order, order);
        matrix_t b = make_random_matrix(order, order);
        matrix_t result;

        BENCHMARK_MEASURE("matrix", "add",
                          (order == 8u) ? "add two 8 by 8 matrices" : NULL,
                          order, repeats,
                          { result = matrix_add(&a, &b); matrix_free(&result); });

        BENCHMARK_MEASURE("matrix", "subtract",
                          (order == 8u) ? "subtract two 8 by 8 matrices" : NULL,
                          order, repeats,
                          { result = matrix_subtract(&a, &b); matrix_free(&result); });

        BENCHMARK_MEASURE("matrix", "multiply",
                          (order == 8u) ? "multiply two 8 by 8 matrices" : NULL,
                          order, repeats,
                          { result = matrix_multiply(&a, &b); matrix_free(&result); });

        BENCHMARK_MEASURE("matrix", "transpose",
                          (order == 8u) ? "transpose an 8 by 8 matrix" : NULL,
                          order, repeats,
                          { result = matrix_transpose(&a); matrix_free(&result); });

        // The operation that writes into a matrix gets no memory. The
        // difference between the two lines shows the cost of that memory.
        matrix_t destination = matrix_alloc(order, order);
        BENCHMARK_MEASURE("matrix", "multiply_into",
                          (order == 8u)
                              ? "multiply two 8 by 8 matrices into the caller's memory"
                              : NULL,
                          order, repeats,
                          matrix_multiply_into(&a, &b, &destination));

        matrix_free(&destination);
        matrix_free(&a);
        matrix_free(&b);
    }

    for(uint32_t index = 0; index < ORDER_COUNT; index++)
    {
        uint32_t order = ORDERS[index];
        uint32_t repeats = 1000 / order;

        matrix_t matrix = make_invertible_matrix(order);
        matrix_t result;

        BENCHMARK_MEASURE("matrix", "inverse",
                          (order == 8u) ? "invert an 8 by 8 matrix" : NULL,
                          order, repeats,
                          { result = matrix_inverse(&matrix); matrix_free(&result); });

        matrix_free(&matrix);
    }

    for(uint32_t index = 0; index < DETERMINANT_ORDER_COUNT; index++)
    {
        uint32_t order = DETERMINANT_ORDERS[index];
        matrix_t matrix = make_random_matrix(order, order);
        real_t value;

        BENCHMARK_MEASURE("matrix", "determinant",
                          (order == 8u) ? "the determinant of an 8 by 8 matrix" : NULL,
                          order, 2000,
                          value = matrix_determinant(&matrix));
        (void)value;

        matrix_free(&matrix);
    }
}
