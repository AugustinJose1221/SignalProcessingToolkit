#include "unity.h"
#include "real_assert.h"
#include "cspline.h"
#include <stdlib.h>
#include "Mock_binarysearch.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_cspline_alloc(void)
{
    cspline_t cspline = cspline_alloc(3);
    TEST_ASSERT_EQUAL(3, cspline.size);
    TEST_ASSERT_EQUAL(true, cspline.dynamic_alloc);
    cspline_free(cspline);
}

void test_cspline_static_alloc(void)
{
    real_t bank0[3];
    real_t bank1[3];
    real_t bank2[3];
    real_t bank3[3];
    real_t bank4[3];
    real_t* membank[5] = {bank0, bank1, bank2, bank3, bank4};
    cspline_t cspline = cspline_static_alloc(3, membank);
    TEST_ASSERT_EQUAL(3, cspline.size);
    TEST_ASSERT_EQUAL(false, cspline.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(bank0, cspline.x);
    TEST_ASSERT_EQUAL_PTR(bank1, cspline.y);
    cspline_free(cspline);
}

void test_cspline_alloc_mempool(void)
{
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    TEST_ASSERT_EQUAL(true, mempool.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(mempool.d);
    TEST_ASSERT_NOT_NULL(mempool.b);
    TEST_ASSERT_NOT_NULL(mempool.q);
    TEST_ASSERT_NOT_NULL(mempool.dp);
    TEST_ASSERT_NOT_NULL(mempool.dx);
    cspline_free_mempool(mempool);
}

void test_cspline_static_alloc_mempool(void)
{
    real_t mempool0[5];
    real_t mempool1[5];
    real_t mempool2[5];
    real_t mempool3[5];
    real_t mempool4[5];
    real_t *membank[5]={mempool0, mempool1, mempool2, mempool3, mempool4};
    cspline_mempool_t mempool = cspline_static_alloc_mempool(membank);
    TEST_ASSERT_EQUAL(false, mempool.dynamic_alloc);
    TEST_ASSERT_EQUAL(mempool0, mempool.d);
    TEST_ASSERT_EQUAL(mempool1, mempool.b);
    TEST_ASSERT_EQUAL(mempool2, mempool.q);
    TEST_ASSERT_EQUAL(mempool3, mempool.dp);
    TEST_ASSERT_EQUAL(mempool4, mempool.dx);
}

void test_cspline_init(void)
{
    real_t x[3] = {1, 2, 3};
    real_t y[3] = {1, 2, 3};
    cspline_t cspline = cspline_alloc(3);
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    cspline_init(&cspline, mempool, x, y);
    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_update_size(void)
{
    cspline_t cspline = cspline_alloc(3);
    cspline_update_size(&cspline, 4);
    TEST_ASSERT_EQUAL(4, cspline.size);
    cspline_free(cspline);
}

void test_cspline_get_interpolated_point(void)
{
    real_t x[3] = {1, 2, 3};
    real_t y[3] = {1, 2, 3};
    cspline_t cspline = cspline_alloc(3);
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    cspline_init(&cspline, mempool, x, y);
    binarysearch_get_index_IgnoreAndReturn(2);
    real_t interpolated_point = cspline_get_interpolated_point(&cspline, 5);
    TEST_ASSERT_EQUAL(5, interpolated_point);
    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_free_mempool(void)
{
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    cspline_free_mempool(mempool);

    real_t mempool0[5];
    real_t mempool1[5];
    real_t mempool2[5];
    real_t mempool3[5];
    real_t mempool4[5];
    real_t *membank[5]={mempool0, mempool1, mempool2, mempool3, mempool4};
    mempool = cspline_static_alloc_mempool(membank);
    cspline_free_mempool(mempool);
}

void test_cspline_free_releases_the_memory_of_a_dynamic_spline(void)
{
    cspline_t cspline = cspline_alloc(4);

    TEST_ASSERT_EQUAL(true, cspline.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(cspline.x);
    TEST_ASSERT_NOT_NULL(cspline.y);
    TEST_ASSERT_NOT_NULL(cspline.b);
    TEST_ASSERT_NOT_NULL(cspline.c);
    TEST_ASSERT_NOT_NULL(cspline.d);

    for(uint32_t index = 0; index < cspline.size; index++)
    {
        cspline.x[index] = (real_t)index;
        cspline.y[index] = (real_t)index;
    }

    cspline_free(cspline);
}

void test_cspline_free_keeps_the_memory_of_a_static_spline(void)
{
    real_t bank0[3] = {1, 2, 3};
    real_t bank1[3] = {4, 5, 6};
    real_t bank2[3];
    real_t bank3[3];
    real_t bank4[3];
    real_t* membank[5] = {bank0, bank1, bank2, bank3, bank4};

    cspline_t cspline = cspline_static_alloc(3, membank);

    cspline_free(cspline);

    // The memory belongs to the caller. It must still hold the values.
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), bank0[0]);
    TEST_ASSERT_EQUAL_REAL(REAL_C(6.0), bank1[2]);
    TEST_ASSERT_EQUAL_PTR(bank0, cspline.x);
}
