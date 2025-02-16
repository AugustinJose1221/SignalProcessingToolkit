#include "unity.h"
#include "Mock_vector.h"
#include "vector2d.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_vector2d_alloc(void)
{
    vector_t vector = {2, NULL, true};
    vector_alloc_ExpectAndReturn(2, vector);
    vector_t vector2d = vector2d_alloc();
    TEST_ASSERT_EQUAL(2, vector2d.size);
    TEST_ASSERT_EQUAL(true, vector2d.dynamic_alloc);
}

void test_vector2d_static_alloc(void)
{
    uint32_t mempool[2];
    vector_t vector = {2, (float*)mempool, false};
    vector_static_alloc_ExpectAndReturn(2, mempool, vector);
    vector_t vector2d = vector2d_static_alloc(mempool);
    TEST_ASSERT_EQUAL(2, vector2d.size);
    TEST_ASSERT_EQUAL(false, vector2d.dynamic_alloc);
}

void test_vector2d_add_point_at_index(void)
{
    float mempool[2];
    vector_t vector = {2, &mempool, true};
    vector_t* vector_ptr = &vector;
    vector_add_point_at_index_Expect(vector_ptr, 0, 1);
    vector2d_add_point_at_index(vector_ptr, 0, 1);
}