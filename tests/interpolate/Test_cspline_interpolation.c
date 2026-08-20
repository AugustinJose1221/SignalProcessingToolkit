// The tests in Test_cspline.c use a mock of the binary search. Thus they
// cannot show which interval the interpolation uses. The tests in this file
// use the real binary search module.

#include "unity.h"
#include "real_assert.h"
#include "cspline.h"
// The spline calls the binary search. This include has no mock prefix, thus
// the build takes the real module and not a mock of it.
#include "binarysearch.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_cspline_gives_the_knot_values_at_the_knots(void)
{
    real_t x[5] = {0, 1, 2, 3, 4};
    real_t y[5] = {0, 1, 0, 1, 0};
    cspline_t cspline = cspline_alloc(5);
    cspline_mempool_t mempool = cspline_alloc_mempool(5);

    cspline_init(&cspline, mempool, x, y);

    for(uint32_t index = 0; index < 5; index++)
    {
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, y[index],
                                 cspline_get_interpolated_point(&cspline, x[index]));
    }

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_is_continuous_at_the_knots(void)
{
    // A spline has no step at a knot. The value on the left of a knot and the
    // value on the right of a knot must be almost the same.
    real_t x[5] = {0, 1, 2, 3, 4};
    real_t y[5] = {0, 1, 0, 1, 0};
    cspline_t cspline = cspline_alloc(5);
    cspline_mempool_t mempool = cspline_alloc_mempool(5);

    cspline_init(&cspline, mempool, x, y);

    for(uint32_t knot = 1; knot < 4; knot++)
    {
        real_t left = cspline_get_interpolated_point(&cspline, (real_t)knot - REAL_C(0.001));
        real_t right = cspline_get_interpolated_point(&cspline, (real_t)knot + REAL_C(0.001));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), left, right);
    }

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_of_a_straight_line_is_the_straight_line(void)
{
    // The values lie on a straight line. Thus the spline must give that line
    // at every point between the knots.
    real_t x[4] = {0, 1, 2, 3};
    real_t y[4] = {0, 2, 4, 6};
    cspline_t cspline = cspline_alloc(4);
    cspline_mempool_t mempool = cspline_alloc_mempool(4);

    cspline_init(&cspline, mempool, x, y);

    for(int step = 0; step <= 30; step++)
    {
        real_t point = (real_t)step/REAL_C(10.0);
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(2.0)*point,
                                 cspline_get_interpolated_point(&cspline, point));
    }

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_stays_between_the_knot_values_of_a_rising_line(void)
{
    real_t x[4] = {0, 1, 2, 3};
    real_t y[4] = {0, 1, 4, 9};
    cspline_t cspline = cspline_alloc(4);
    cspline_mempool_t mempool = cspline_alloc_mempool(4);

    cspline_init(&cspline, mempool, x, y);

    real_t previous = cspline_get_interpolated_point(&cspline, REAL_C(0.0));
    for(int step = 1; step <= 30; step++)
    {
        real_t point = (real_t)step/REAL_C(10.0);
        real_t value = cspline_get_interpolated_point(&cspline, point);
        TEST_ASSERT_TRUE(value >= previous - TOLERANCE);
        previous = value;
    }

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_reads_the_last_interval_at_the_last_knot(void)
{
    // The coefficient arrays hold one element for each interval, thus they
    // hold size-1 elements. At the last knot the index must stay inside the
    // arrays.
    real_t x[3] = {1, 2, 3};
    real_t y[3] = {1, 4, 9};
    cspline_t cspline = cspline_alloc(3);
    cspline_mempool_t mempool = cspline_alloc_mempool(3);

    cspline_init(&cspline, mempool, x, y);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(9.0),
                             cspline_get_interpolated_point(&cspline, REAL_C(3.0)));

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_uses_the_first_interval_at_the_first_knot(void)
{
    real_t x[3] = {1, 2, 3};
    real_t y[3] = {5, 4, 9};
    cspline_t cspline = cspline_alloc(3);
    cspline_mempool_t mempool = cspline_alloc_mempool(3);

    cspline_init(&cspline, mempool, x, y);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0),
                             cspline_get_interpolated_point(&cspline, REAL_C(1.0)));

    cspline_free(cspline);
    cspline_free_mempool(mempool);
}
