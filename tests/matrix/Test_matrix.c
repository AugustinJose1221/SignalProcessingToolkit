#include "unity.h"
#include "matrix.h"
#include <stdlib.h>

void setUp(void)
{

}

void tearDown(void)
{

}

void test_matrix_alloc(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    TEST_ASSERT_EQUAL(3, matrix.m);
    TEST_ASSERT_EQUAL(3, matrix.n);
    TEST_ASSERT_EQUAL(true, matrix.dynamic_alloc);
    matrix_free(&matrix);
}

void test_matrix_static_alloc(void)
{
    float elem[9];
    matrix_t matrix = matrix_static_alloc(3, 3, elem);
    TEST_ASSERT_EQUAL(3, matrix.m);
    TEST_ASSERT_EQUAL(3, matrix.n);
    TEST_ASSERT_EQUAL(false, matrix.dynamic_alloc);
    matrix_free(&matrix);
}

void test_matrix_add_element(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix, 0, 2));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&matrix, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix, 1, 2));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&matrix, 2, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix, 2, 1));
    TEST_ASSERT_EQUAL(9, matrix_get_element(&matrix, 2, 2));
    matrix_free(&matrix);
}

void test_matrix_get_element(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix, 0, 2));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&matrix, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix, 1, 2));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&matrix, 2, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix, 2, 1));
    TEST_ASSERT_EQUAL(9, matrix_get_element(&matrix, 2, 2));
    matrix_free(&matrix);
}

void test_matrix_get_nth_row(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    matrix_t row_matrix = matrix_get_nth_row(&matrix, 1);
    TEST_ASSERT_EQUAL(4, matrix_get_element(&row_matrix, 0, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&row_matrix, 0, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&row_matrix, 0, 2));
    matrix_free(&matrix);
    matrix_free(&row_matrix);
}

void test_matrix_get_nth_col(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    matrix_t col_matrix = matrix_get_nth_col(&matrix, 1);
    TEST_ASSERT_EQUAL(2, matrix_get_element(&col_matrix, 0, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&col_matrix, 1, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&col_matrix, 2, 0));
    matrix_free(&matrix);
    matrix_free(&col_matrix);
}

void test_matrix_get_order(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    matrix_t order = matrix_get_order(&matrix);
    TEST_ASSERT_EQUAL(3, matrix_get_element(&order, 0, 0));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&order, 0, 1));
    matrix_free(&matrix);
    matrix_free(&order);
}

void test_matrix_trace(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    TEST_ASSERT_EQUAL(15, matrix_trace(&matrix));
    matrix_free(&matrix);
}

void test_matrix_determinant(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    TEST_ASSERT_EQUAL(0, matrix_determinant(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 3);
    matrix_add_element(&matrix, 1, 1, 4);
    TEST_ASSERT_EQUAL(-2, matrix_determinant(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(1, 1);
    matrix_add_element(&matrix, 0, 0, 1);
    TEST_ASSERT_EQUAL(1, matrix_determinant(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(4, 4);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 0, 3, 4);
    matrix_add_element(&matrix, 1, 0, 5);
    matrix_add_element(&matrix, 1, 1, 6);
    matrix_add_element(&matrix, 1, 2, 7);
    matrix_add_element(&matrix, 1, 3, 8);
    matrix_add_element(&matrix, 2, 0, 9);
    matrix_add_element(&matrix, 2, 1, 10);
    matrix_add_element(&matrix, 2, 2, 11);
    matrix_add_element(&matrix, 2, 3, 12);
    matrix_add_element(&matrix, 3, 0, 13);
    matrix_add_element(&matrix, 3, 1, 14);
    matrix_add_element(&matrix, 3, 2, 15);
    matrix_add_element(&matrix, 3, 3, 16);
    TEST_ASSERT_EQUAL(0, matrix_determinant(&matrix));
    matrix_free(&matrix);
}

void test_matrix_create_unit_matrix(void)
{
    matrix_t matrix = matrix_create_unit_matrix(3);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix, 0, 0));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 0, 1));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 0, 2));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 1, 0));
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix, 1, 1));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 1, 2));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 2, 0));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 2, 1));
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix, 2, 2));
    matrix_free(&matrix);
}

void test_matrix_create_zero_matrix(void)
{
    matrix_t matrix = matrix_create_zero_matrix(3, 3);
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 0, 0));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 0, 1));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 0, 2));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 1, 0));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 1, 1));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 1, 2));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 2, 0));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 2, 1));
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix, 2, 2));
    matrix_free(&matrix);
}

void test_matrix_is_equal(void)
{
    matrix_t matrix_a = matrix_alloc(3, 3);
    matrix_add_element(&matrix_a, 0, 0, 1);
    matrix_add_element(&matrix_a, 0, 1, 0);
    matrix_add_element(&matrix_a, 0, 2, 0);
    matrix_add_element(&matrix_a, 1, 0, 0);
    matrix_add_element(&matrix_a, 1, 1, 1);
    matrix_add_element(&matrix_a, 1, 2, 0);
    matrix_add_element(&matrix_a, 2, 0, 0);
    matrix_add_element(&matrix_a, 2, 1, 0);
    matrix_add_element(&matrix_a, 2, 2, 1);
    matrix_t matrix_b = matrix_create_unit_matrix(3);
    TEST_ASSERT_EQUAL(true, matrix_is_equal(&matrix_a, &matrix_b));

    matrix_add_element(&matrix_b, 0, 0, 0);
    TEST_ASSERT_EQUAL(false, matrix_is_equal(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);

    matrix_a = matrix_alloc(3, 3);
    matrix_b = matrix_alloc(3, 2);
    TEST_ASSERT_EQUAL(false, matrix_is_equal(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);

    matrix_a = matrix_alloc(3, 3);
    matrix_b = matrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(false, matrix_is_equal(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
}

void test_matrix_is_square(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    TEST_ASSERT_EQUAL(true, matrix_is_square(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(3, 2);
    TEST_ASSERT_EQUAL(false, matrix_is_square(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(false, matrix_is_square(&matrix));
    matrix_free(&matrix);
}

void test_matrix_is_zero(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 0);
    matrix_add_element(&matrix, 0, 1, 0);
    matrix_add_element(&matrix, 0, 2, 0);
    matrix_add_element(&matrix, 1, 0, 0);
    matrix_add_element(&matrix, 1, 1, 0);
    matrix_add_element(&matrix, 1, 2, 0);
    matrix_add_element(&matrix, 2, 0, 0);
    matrix_add_element(&matrix, 2, 1, 0);
    matrix_add_element(&matrix, 2, 2, 0);
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    TEST_ASSERT_EQUAL(false, matrix_is_zero(&matrix));
    matrix_free(&matrix);
}