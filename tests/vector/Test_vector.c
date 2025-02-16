#include "unity.h"
#include "vector.h"
#include <stdlib.h>
#include <stdbool.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_vector_alloc(void)
{
    vector_t vector = vector_alloc(3);
    TEST_ASSERT_EQUAL(3, vector.size);
    TEST_ASSERT_NOT_NULL(vector.data);
    vector_free(&vector);
}

void test_vector_static_alloc(void)
{
    float elem[3];
    vector_t vector = vector_static_alloc(3, elem);
    TEST_ASSERT_EQUAL(3, vector.size);
    TEST_ASSERT_EQUAL(elem, vector.data);
    vector_free(&vector);
}

void test_vector_add_point_at_index(void)
{
    vector_t vector = vector_alloc(3);
    vector_add_point_at_index(&vector, 0, 1);
    vector_add_point_at_index(&vector, 1, 2);
    vector_add_point_at_index(&vector, 2, 3);
    TEST_ASSERT_EQUAL(1, vector_get(&vector, 0));
    TEST_ASSERT_EQUAL(2, vector_get(&vector, 1));
    TEST_ASSERT_EQUAL(3, vector_get(&vector, 2));
    vector_free(&vector);
}

void test_vector_add_from_array(void)
{
    vector_t vector = vector_alloc(3);
    float data[3] = {1, 2, 3};
    vector_add_from_array(&vector, 3, data);
    TEST_ASSERT_EQUAL(1, vector_get(&vector, 0));
    TEST_ASSERT_EQUAL(2, vector_get(&vector, 1));
    TEST_ASSERT_EQUAL(3, vector_get(&vector, 2));
    vector_free(&vector);
}

void test_vector_printf(void)
{
    vector_t vector = vector_alloc(3);
    vector_add_point_at_index(&vector, 0, 1);
    vector_add_point_at_index(&vector, 1, 2);
    vector_add_point_at_index(&vector, 2, 3);
    vector_printf(&vector, printf);
    vector_free(&vector);

    vector = vector_alloc(3);
    vector_add_point_at_index(&vector, 0, 1);
    vector_add_point_at_index(&vector, 1, 2);
    vector_add_point_at_index(&vector, 2, 3);
    vector_printf(&vector, NULL);
    vector_free(&vector);
}

void test_vector_get(void)
{
    vector_t vector = vector_alloc(3);
    vector_add_point_at_index(&vector, 0, 1);
    vector_add_point_at_index(&vector, 1, 2);
    vector_add_point_at_index(&vector, 2, 3);
    TEST_ASSERT_EQUAL(1, vector_get(&vector, 0));
    TEST_ASSERT_EQUAL(2, vector_get(&vector, 1));
    TEST_ASSERT_EQUAL(3, vector_get(&vector, 2));
    vector_free(&vector);
}

void test_vector_dot_product(void)
{
    vector_t x = vector_alloc(3);
    vector_t y = vector_alloc(3);
    vector_add_point_at_index(&x, 0, 1);
    vector_add_point_at_index(&x, 1, 2);
    vector_add_point_at_index(&x, 2, 3);
    vector_add_point_at_index(&y, 0, 4);
    vector_add_point_at_index(&y, 1, 5);
    vector_add_point_at_index(&y, 2, 6);
    TEST_ASSERT_EQUAL(32, vector_dot_product(&x, &y));
    vector_free(&x);
    vector_free(&y);
}

void test_vector_norm(void)
{
    vector_t x = vector_alloc(3);
    vector_add_point_at_index(&x, 0, 1);
    vector_add_point_at_index(&x, 1, 2);
    vector_add_point_at_index(&x, 2, 3);
    TEST_ASSERT_EQUAL(3.741657, vector_norm(&x));
    vector_free(&x);
}

void test_vector_free(void)
{
    vector_t vector = vector_alloc(3);
    vector_free(&vector);
}