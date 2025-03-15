#include "unity.h"
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
    float membank[5];
    cspline_t cspline = cspline_static_alloc(3, membank);
    TEST_ASSERT_EQUAL(3, cspline.size);
    TEST_ASSERT_EQUAL(false, cspline.dynamic_alloc);
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
    float mempool0[5];
    float mempool1[5];
    float mempool2[5];
    float mempool3[5];
    float mempool4[5];
    float *membank[5]={mempool0, mempool1, mempool2, mempool3, mempool4};
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
    float x[3] = {1, 2, 3};
    float y[3] = {1, 2, 3};
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
    float x[3] = {1, 2, 3};
    float y[3] = {1, 2, 3};
    cspline_t cspline = cspline_alloc(3);
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    cspline_init(&cspline, mempool, x, y);
    binarysearch_get_index_IgnoreAndReturn(2);
    float interpolated_point = cspline_get_interpolated_point(&cspline, 5);
    TEST_ASSERT_EQUAL(5, interpolated_point);
    cspline_free(cspline);
    cspline_free_mempool(mempool);
}

void test_cspline_free_mempool(void)
{
    cspline_mempool_t mempool = cspline_alloc_mempool(3);
    cspline_free_mempool(mempool);

    float mempool0[5];
    float mempool1[5];
    float mempool2[5];
    float mempool3[5];
    float mempool4[5];
    float *membank[5]={mempool0, mempool1, mempool2, mempool3, mempool4};
    mempool = cspline_static_alloc_mempool(membank);
    cspline_free_mempool(mempool);
}