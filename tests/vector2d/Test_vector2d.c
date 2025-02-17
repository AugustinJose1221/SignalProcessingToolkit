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

void test_vector2d_add_from_array(void)
{
    float mempool[2];
    vector_t vector = {2, &mempool, true};
    vector_t* vector_ptr = &vector;
    float data[2] = {1, 2};
    vector_add_from_array_Expect(vector_ptr, 2, data);
    vector2d_add_from_array(vector_ptr, data);
}

void test_vector2d_printf(void)
{
    float mempool[2];
    vector_t vector = {2, &mempool, true};
    vector_t* vector_ptr = &vector;
    vector_printf_Expect(vector_ptr, printf);
    vector2d_printf(vector_ptr, printf);
}

void test_vector2d_get(void)
{
    float mempool[2] = {1, 2};
    vector_t vector = {2, &mempool, true};
    vector_t* vector_ptr = &vector;
    vector_get_ExpectAndReturn(vector_ptr, 0, 1);
    TEST_ASSERT_EQUAL(1, vector2d_get(vector_ptr, 0));
}

void test_vector2d_dot_product(void)
{
    float mempool_x[2] = {1, 2};
    vector_t x = {2, &mempool_x, true};
    vector_t* x_ptr = &x;
    float mempool_y[2] = {3, 4};
    vector_t y = {2, &mempool_y, true};
    vector_t* y_ptr = &y;
    vector_dot_product_ExpectAndReturn(x_ptr, y_ptr, 11);
    TEST_ASSERT_EQUAL(11, vector2d_dot_product(x_ptr, y_ptr));
}

void test_vector2d_norm(void)
{
    float mempool[2] = {3, 4};
    vector_t vector = {2, &mempool, true};
    vector_t* vector_ptr = &vector;
    vector_norm_ExpectAndReturn(vector_ptr, 5);
    TEST_ASSERT_EQUAL(5, vector2d_norm(vector_ptr));
}