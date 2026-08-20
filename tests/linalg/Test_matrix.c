#include "unity.h"
#include "real_assert.h"
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
    real_t elem[9];
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

void test_matrix_get_nth_col_of_a_matrix_with_more_rows_than_columns(void)
{
    matrix_t matrix = matrix_alloc(3, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 3);
    matrix_add_element(&matrix, 1, 1, 4);
    matrix_add_element(&matrix, 2, 0, 5);
    matrix_add_element(&matrix, 2, 1, 6);
    matrix_t col_matrix = matrix_get_nth_col(&matrix, 0);
    TEST_ASSERT_EQUAL(3, col_matrix.m);
    TEST_ASSERT_EQUAL(1, col_matrix.n);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&col_matrix, 0, 0));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&col_matrix, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&col_matrix, 2, 0));
    matrix_free(&matrix);
    matrix_free(&col_matrix);
}

void test_matrix_get_nth_col_of_a_matrix_with_more_columns_than_rows(void)
{
    matrix_t matrix = matrix_alloc(2, 4);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 0, 3, 4);
    matrix_add_element(&matrix, 1, 0, 5);
    matrix_add_element(&matrix, 1, 1, 6);
    matrix_add_element(&matrix, 1, 2, 7);
    matrix_add_element(&matrix, 1, 3, 8);
    matrix_t col_matrix = matrix_get_nth_col(&matrix, 3);
    TEST_ASSERT_EQUAL(2, col_matrix.m);
    TEST_ASSERT_EQUAL(1, col_matrix.n);
    TEST_ASSERT_EQUAL(4, matrix_get_element(&col_matrix, 0, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&col_matrix, 1, 0));
    matrix_free(&matrix);
    matrix_free(&col_matrix);
}

void test_matrix_get_nth_row_of_a_matrix_that_is_not_square(void)
{
    matrix_t matrix = matrix_alloc(3, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 3);
    matrix_add_element(&matrix, 1, 1, 4);
    matrix_add_element(&matrix, 2, 0, 5);
    matrix_add_element(&matrix, 2, 1, 6);
    matrix_t row_matrix = matrix_get_nth_row(&matrix, 2);
    TEST_ASSERT_EQUAL(1, row_matrix.m);
    TEST_ASSERT_EQUAL(2, row_matrix.n);
    TEST_ASSERT_EQUAL(5, matrix_get_element(&row_matrix, 0, 0));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&row_matrix, 0, 1));
    matrix_free(&matrix);
    matrix_free(&row_matrix);
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

void test_matrix_is_unit(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 0);
    matrix_add_element(&matrix, 0, 2, 0);
    matrix_add_element(&matrix, 1, 0, 0);
    matrix_add_element(&matrix, 1, 1, 1);
    matrix_add_element(&matrix, 1, 2, 0);
    matrix_add_element(&matrix, 2, 0, 0);
    matrix_add_element(&matrix, 2, 1, 0);
    matrix_add_element(&matrix, 2, 2, 1);
    TEST_ASSERT_EQUAL(true, matrix_is_unit(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 0);
    matrix_add_element(&matrix, 0, 2, 0);
    matrix_add_element(&matrix, 1, 0, 0);
    matrix_add_element(&matrix, 1, 1, 1);
    matrix_add_element(&matrix, 1, 2, 0);
    matrix_add_element(&matrix, 2, 0, 0);
    matrix_add_element(&matrix, 2, 1, 0);
    matrix_add_element(&matrix, 2, 2, 0);
    TEST_ASSERT_EQUAL(false, matrix_is_unit(&matrix));
    matrix_free(&matrix);

    matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 0);
    matrix_add_element(&matrix, 0, 2, 0);
    matrix_add_element(&matrix, 1, 0, 0);
    matrix_add_element(&matrix, 1, 1, 1);
    matrix_add_element(&matrix, 1, 2, 0);
    matrix_add_element(&matrix, 2, 0, 0);
    matrix_add_element(&matrix, 2, 1, 1);
    matrix_add_element(&matrix, 2, 2, 1);
    TEST_ASSERT_EQUAL(false, matrix_is_unit(&matrix));
    matrix_free(&matrix);
}

void test_matrix_is_multipliable(void)
{
    matrix_t matrix_a = matrix_alloc(3, 2);
    matrix_t matrix_b = matrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(true, matrix_is_multipliable(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);

    matrix_a = matrix_alloc(3, 2);
    matrix_b = matrix_alloc(3, 3);
    TEST_ASSERT_EQUAL(false, matrix_is_multipliable(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);

    matrix_a = matrix_alloc(3, 3);
    matrix_b = matrix_alloc(3, 3);
    TEST_ASSERT_EQUAL(true, matrix_is_multipliable(&matrix_a, &matrix_b));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
}

void test_matrix_add(void)
{
    matrix_t matrix_a = matrix_alloc(3, 3);
    matrix_add_element(&matrix_a, 0, 0, 1);
    matrix_add_element(&matrix_a, 0, 1, 2);
    matrix_add_element(&matrix_a, 0, 2, 3);
    matrix_add_element(&matrix_a, 1, 0, 4);
    matrix_add_element(&matrix_a, 1, 1, 5);
    matrix_add_element(&matrix_a, 1, 2, 6);
    matrix_add_element(&matrix_a, 2, 0, 7);
    matrix_add_element(&matrix_a, 2, 1, 8);
    matrix_add_element(&matrix_a, 2, 2, 9);
    matrix_t matrix_b = matrix_create_unit_matrix(3);
    matrix_t matrix_c = matrix_add(&matrix_a, &matrix_b);
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_c, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_c, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix_c, 0, 2));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_c, 1, 0));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_c, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_c, 1, 2));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&matrix_c, 2, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix_c, 2, 1));
    TEST_ASSERT_EQUAL(10, matrix_get_element(&matrix_c, 2, 2));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
    matrix_free(&matrix_c);
}

void test_matrix_subtract(void)
{
    matrix_t matrix_a = matrix_alloc(3, 3);
    matrix_add_element(&matrix_a, 0, 0, 1);
    matrix_add_element(&matrix_a, 0, 1, 2);
    matrix_add_element(&matrix_a, 0, 2, 3);
    matrix_add_element(&matrix_a, 1, 0, 4);
    matrix_add_element(&matrix_a, 1, 1, 5);
    matrix_add_element(&matrix_a, 1, 2, 6);
    matrix_add_element(&matrix_a, 2, 0, 7);
    matrix_add_element(&matrix_a, 2, 1, 8);
    matrix_add_element(&matrix_a, 2, 2, 9);
    matrix_t matrix_b = matrix_create_unit_matrix(3);
    matrix_t matrix_c = matrix_subtract(&matrix_a, &matrix_b);
    TEST_ASSERT_EQUAL(0, matrix_get_element(&matrix_c, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_c, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix_c, 0, 2));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_c, 1, 0));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_c, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_c, 1, 2));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&matrix_c, 2, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix_c, 2, 1));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix_c, 2, 2));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
    matrix_free(&matrix_c);
}

void test_matrix_subtract_gives_zero_for_equal_matrices(void)
{
    matrix_t matrix_a = matrix_alloc(2, 3);
    matrix_add_element(&matrix_a, 0, 0, -REAL_C(1.5));
    matrix_add_element(&matrix_a, 0, 1, 0);
    matrix_add_element(&matrix_a, 0, 2, REAL_C(2.5));
    matrix_add_element(&matrix_a, 1, 0, REAL_C(3.25));
    matrix_add_element(&matrix_a, 1, 1, -REAL_C(4.75));
    matrix_add_element(&matrix_a, 1, 2, 6);
    matrix_t matrix_b = matrix_alloc(2, 3);
    matrix_copy(&matrix_a, &matrix_b);
    matrix_t matrix_c = matrix_subtract(&matrix_a, &matrix_b);
    TEST_ASSERT_EQUAL(2, matrix_c.m);
    TEST_ASSERT_EQUAL(3, matrix_c.n);
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&matrix_c));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
    matrix_free(&matrix_c);
}

void test_matrix_subtract_is_not_commutative(void)
{
    matrix_t matrix_a = matrix_alloc(2, 2);
    matrix_add_element(&matrix_a, 0, 0, 5);
    matrix_add_element(&matrix_a, 0, 1, 6);
    matrix_add_element(&matrix_a, 1, 0, 7);
    matrix_add_element(&matrix_a, 1, 1, 8);
    matrix_t matrix_b = matrix_create_unit_matrix(2);
    matrix_t matrix_c = matrix_subtract(&matrix_a, &matrix_b);
    matrix_t matrix_d = matrix_subtract(&matrix_b, &matrix_a);
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_c, 0, 0));
    TEST_ASSERT_EQUAL(-4, matrix_get_element(&matrix_d, 0, 0));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_c, 0, 1));
    TEST_ASSERT_EQUAL(-6, matrix_get_element(&matrix_d, 0, 1));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
    matrix_free(&matrix_c);
    matrix_free(&matrix_d);
}

void test_matrix_multiply_scalar(void)
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
    matrix_t matrix_b = matrix_multiply_scalar(&matrix, 2);
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_b, 0, 0));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_b, 0, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_b, 0, 2));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix_b, 1, 0));
    TEST_ASSERT_EQUAL(10, matrix_get_element(&matrix_b, 1, 1));
    TEST_ASSERT_EQUAL(12, matrix_get_element(&matrix_b, 1, 2));
    TEST_ASSERT_EQUAL(14, matrix_get_element(&matrix_b, 2, 0));
    TEST_ASSERT_EQUAL(16, matrix_get_element(&matrix_b, 2, 1));
    TEST_ASSERT_EQUAL(18, matrix_get_element(&matrix_b, 2, 2));
    matrix_free(&matrix);
    matrix_free(&matrix_b);
}

void test_matrix_multiply(void)
{
    matrix_t matrix_a = matrix_alloc(3, 2);
    matrix_add_element(&matrix_a, 0, 0, 1);
    matrix_add_element(&matrix_a, 0, 1, 2);
    matrix_add_element(&matrix_a, 1, 0, 3);
    matrix_add_element(&matrix_a, 1, 1, 4);
    matrix_add_element(&matrix_a, 2, 0, 5);
    matrix_add_element(&matrix_a, 2, 1, 6);
    matrix_t matrix_b = matrix_alloc(2, 3);
    matrix_add_element(&matrix_b, 0, 0, 7);
    matrix_add_element(&matrix_b, 0, 1, 8);
    matrix_add_element(&matrix_b, 0, 2, 9);
    matrix_add_element(&matrix_b, 1, 0, 10);
    matrix_add_element(&matrix_b, 1, 1, 11);
    matrix_add_element(&matrix_b, 1, 2, 12);
    matrix_t matrix_c = matrix_multiply(&matrix_a, &matrix_b);
    TEST_ASSERT_EQUAL(27, matrix_get_element(&matrix_c, 0, 0));
    TEST_ASSERT_EQUAL(30, matrix_get_element(&matrix_c, 0, 1));
    TEST_ASSERT_EQUAL(33, matrix_get_element(&matrix_c, 0, 2));
    TEST_ASSERT_EQUAL(61, matrix_get_element(&matrix_c, 1, 0));
    TEST_ASSERT_EQUAL(68, matrix_get_element(&matrix_c, 1, 1));
    TEST_ASSERT_EQUAL(75, matrix_get_element(&matrix_c, 1, 2));
    TEST_ASSERT_EQUAL(95, matrix_get_element(&matrix_c, 2, 0));
    TEST_ASSERT_EQUAL(106, matrix_get_element(&matrix_c, 2, 1));
    TEST_ASSERT_EQUAL(117, matrix_get_element(&matrix_c, 2, 2));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
    matrix_free(&matrix_c);
}

void test_matrix_transpose(void)
{
    matrix_t matrix = matrix_alloc(3, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 3);
    matrix_add_element(&matrix, 1, 1, 4);
    matrix_add_element(&matrix, 2, 0, 5);
    matrix_add_element(&matrix, 2, 1, 6);
    matrix_t matrix_b = matrix_transpose(&matrix);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix_b, 0, 0));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix_b, 0, 1));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&matrix_b, 0, 2));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_b, 1, 0));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_b, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_b, 1, 2));
    matrix_free(&matrix);
    matrix_free(&matrix_b);
}

void test_matrix_inverse(void)
{
    // The inverse of [[4,7],[2,6]] is [[0.6,-0.7],[-0.2,0.4]].
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 4);
    matrix_add_element(&matrix, 0, 1, 7);
    matrix_add_element(&matrix, 1, 0, 2);
    matrix_add_element(&matrix, 1, 1, 6);
    matrix_t inverse = matrix_inverse(&matrix);
    TEST_ASSERT_EQUAL(2, inverse.m);
    TEST_ASSERT_EQUAL(2, inverse.n);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.6), matrix_get_element(&inverse, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(0.7), matrix_get_element(&inverse, 0, 1));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(0.2), matrix_get_element(&inverse, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.4), matrix_get_element(&inverse, 1, 1));
    matrix_free(&matrix);
    matrix_free(&inverse);
}

void test_matrix_inverse_of_a_unit_matrix_is_the_unit_matrix(void)
{
    matrix_t matrix = matrix_create_unit_matrix(4);
    matrix_t inverse = matrix_inverse(&matrix);
    TEST_ASSERT_EQUAL(true, matrix_is_unit(&inverse));
    matrix_free(&matrix);
    matrix_free(&inverse);
}

void test_matrix_multiply_by_the_inverse_gives_the_unit_matrix(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 2);
    matrix_add_element(&matrix, 0, 1, -1);
    matrix_add_element(&matrix, 0, 2, 0);
    matrix_add_element(&matrix, 1, 0, -1);
    matrix_add_element(&matrix, 1, 1, 2);
    matrix_add_element(&matrix, 1, 2, -1);
    matrix_add_element(&matrix, 2, 0, 0);
    matrix_add_element(&matrix, 2, 1, -1);
    matrix_add_element(&matrix, 2, 2, 2);
    matrix_t inverse = matrix_inverse(&matrix);
    matrix_t product = matrix_multiply(&matrix, &inverse);
    for(uint32_t i = 0; i < 3; i++)
    {
        for(uint32_t j = 0; j < 3; j++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), (i == j) ? REAL_C(1.0) : REAL_C(0.0),
                                     matrix_get_element(&product, i, j));
        }
    }
    matrix_free(&matrix);
    matrix_free(&inverse);
    matrix_free(&product);
}

void test_matrix_inverse_with_a_zero_in_the_pivot_position(void)
{
    // The determinant of [[0,1],[1,0]] is -1, thus the matrix has an inverse.
    // But the first element on the diagonal is zero. The elimination must
    // exchange the two rows.
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 0);
    matrix_add_element(&matrix, 0, 1, 1);
    matrix_add_element(&matrix, 1, 0, 1);
    matrix_add_element(&matrix, 1, 1, 0);
    matrix_t inverse = matrix_inverse(&matrix);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), matrix_get_element(&inverse, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), matrix_get_element(&inverse, 0, 1));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), matrix_get_element(&inverse, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), matrix_get_element(&inverse, 1, 1));
    matrix_free(&matrix);
    matrix_free(&inverse);
}

void test_matrix_inverse_of_a_singular_matrix_is_a_zero_matrix(void)
{
    // The second row is two times the first row. Thus the matrix is singular.
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 2);
    matrix_add_element(&matrix, 1, 1, 4);
    matrix_t inverse = matrix_inverse(&matrix);
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&inverse));
    matrix_free(&matrix);
    matrix_free(&inverse);
}

void test_matrix_inverse_of_a_matrix_with_one_element(void)
{
    matrix_t matrix = matrix_alloc(1, 1);
    matrix_add_element(&matrix, 0, 0, 4);
    matrix_t inverse = matrix_inverse(&matrix);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.25), matrix_get_element(&inverse, 0, 0));
    matrix_free(&matrix);
    matrix_free(&inverse);
}

void test_matrix_copy(void)
{
    matrix_t matrix_a = matrix_alloc(3, 3);
    matrix_add_element(&matrix_a, 0, 0, 1);
    matrix_add_element(&matrix_a, 0, 1, 2);
    matrix_add_element(&matrix_a, 0, 2, 3);
    matrix_add_element(&matrix_a, 1, 0, 4);
    matrix_add_element(&matrix_a, 1, 1, 5);
    matrix_add_element(&matrix_a, 1, 2, 6);
    matrix_add_element(&matrix_a, 2, 0, 7);
    matrix_add_element(&matrix_a, 2, 1, 8);
    matrix_add_element(&matrix_a, 2, 2, 9);
    matrix_t matrix_b = matrix_alloc(3, 3);
    matrix_copy(&matrix_a, &matrix_b);
    TEST_ASSERT_EQUAL(1, matrix_get_element(&matrix_b, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&matrix_b, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&matrix_b, 0, 2));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&matrix_b, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&matrix_b, 1, 1));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&matrix_b, 1, 2));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&matrix_b, 2, 0));
    TEST_ASSERT_EQUAL(8, matrix_get_element(&matrix_b, 2, 1));
    TEST_ASSERT_EQUAL(9, matrix_get_element(&matrix_b, 2, 2));
    matrix_free(&matrix_a);
    matrix_free(&matrix_b);
}

void test_matrix_printf(void)
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
    matrix_printf(&matrix, printf);
    matrix_free(&matrix);

    matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_add_element(&matrix, 2, 0, 7);
    matrix_add_element(&matrix, 2, 1, 8);
    matrix_add_element(&matrix, 2, 2, 9);
    matrix_printf(&matrix, NULL);
    matrix_free(&matrix);
}

void test_matrix_free(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_free(&matrix);
}
void test_matrix_add_into(void)
{
    matrix_t a = matrix_alloc(2, 2);
    matrix_add_element(&a, 0, 0, 1);
    matrix_add_element(&a, 0, 1, 2);
    matrix_add_element(&a, 1, 0, 3);
    matrix_add_element(&a, 1, 1, 4);
    matrix_t b = matrix_create_unit_matrix(2);
    matrix_t dest = matrix_alloc(2, 2);

    matrix_add_into(&a, &b, &dest);

    TEST_ASSERT_EQUAL(2, matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&dest, 0, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&dest, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&dest, 1, 1));

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&dest);
}

void test_matrix_subtract_into(void)
{
    matrix_t a = matrix_alloc(2, 2);
    matrix_add_element(&a, 0, 0, 5);
    matrix_add_element(&a, 0, 1, 6);
    matrix_add_element(&a, 1, 0, 7);
    matrix_add_element(&a, 1, 1, 8);
    matrix_t b = matrix_create_unit_matrix(2);
    matrix_t dest = matrix_alloc(2, 2);

    matrix_subtract_into(&a, &b, &dest);

    TEST_ASSERT_EQUAL(4, matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&dest, 0, 1));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&dest, 1, 0));
    TEST_ASSERT_EQUAL(7, matrix_get_element(&dest, 1, 1));

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&dest);
}

void test_matrix_multiply_into(void)
{
    matrix_t a = matrix_alloc(2, 3);
    for(uint32_t i = 0; i < 2; i++)
    {
        for(uint32_t j = 0; j < 3; j++)
        {
            matrix_add_element(&a, i, j, (real_t)((i*3)+j+1));
        }
    }
    matrix_t b = matrix_alloc(3, 2);
    for(uint32_t i = 0; i < 3; i++)
    {
        for(uint32_t j = 0; j < 2; j++)
        {
            matrix_add_element(&b, i, j, (real_t)((i*2)+j+1));
        }
    }
    matrix_t dest = matrix_alloc(2, 2);

    matrix_multiply_into(&a, &b, &dest);

    // The same result as the operation that makes a new matrix.
    matrix_t reference = matrix_multiply(&a, &b);
    TEST_ASSERT_EQUAL(true, matrix_is_equal(&dest, &reference));

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&dest);
    matrix_free(&reference);
}

void test_matrix_multiply_scalar_into(void)
{
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, -2);
    matrix_add_element(&matrix, 1, 0, 3);
    matrix_add_element(&matrix, 1, 1, -4);
    matrix_t dest = matrix_alloc(2, 2);

    matrix_multiply_scalar_into(&matrix, REAL_C(2.5), &dest);

    TEST_ASSERT_EQUAL_REAL(REAL_C(2.5), matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(5.0), matrix_get_element(&dest, 0, 1));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.5), matrix_get_element(&dest, 1, 0));
    TEST_ASSERT_EQUAL_REAL(-REAL_C(10.0), matrix_get_element(&dest, 1, 1));

    matrix_free(&matrix);
    matrix_free(&dest);
}

void test_matrix_transpose_into(void)
{
    matrix_t matrix = matrix_alloc(2, 3);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 0, 2, 3);
    matrix_add_element(&matrix, 1, 0, 4);
    matrix_add_element(&matrix, 1, 1, 5);
    matrix_add_element(&matrix, 1, 2, 6);
    matrix_t dest = matrix_alloc(3, 2);

    matrix_transpose_into(&matrix, &dest);

    TEST_ASSERT_EQUAL(1, matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_EQUAL(4, matrix_get_element(&dest, 0, 1));
    TEST_ASSERT_EQUAL(2, matrix_get_element(&dest, 1, 0));
    TEST_ASSERT_EQUAL(5, matrix_get_element(&dest, 1, 1));
    TEST_ASSERT_EQUAL(3, matrix_get_element(&dest, 2, 0));
    TEST_ASSERT_EQUAL(6, matrix_get_element(&dest, 2, 1));

    matrix_free(&matrix);
    matrix_free(&dest);
}

void test_matrix_set_unit(void)
{
    matrix_t matrix = matrix_alloc(3, 3);
    matrix_add_element(&matrix, 0, 0, 9);
    matrix_add_element(&matrix, 1, 2, 9);

    matrix_set_unit(&matrix);

    TEST_ASSERT_EQUAL(true, matrix_is_unit(&matrix));

    matrix_free(&matrix);
}

void test_matrix_set_zero(void)
{
    matrix_t matrix = matrix_alloc(2, 3);
    matrix_add_element(&matrix, 0, 0, 9);
    matrix_add_element(&matrix, 1, 2, 9);

    matrix_set_zero(&matrix);

    TEST_ASSERT_EQUAL(true, matrix_is_zero(&matrix));

    matrix_free(&matrix);
}

void test_matrix_inverse_into(void)
{
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 4);
    matrix_add_element(&matrix, 0, 1, 7);
    matrix_add_element(&matrix, 1, 0, 2);
    matrix_add_element(&matrix, 1, 1, 6);
    matrix_t dest = matrix_alloc(2, 2);
    matrix_t scratch = matrix_alloc(2, 4);

    TEST_ASSERT_EQUAL(true, matrix_inverse_into(&matrix, &dest, &scratch));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.6), matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(0.7), matrix_get_element(&dest, 0, 1));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(0.2), matrix_get_element(&dest, 1, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.4), matrix_get_element(&dest, 1, 1));

    matrix_free(&matrix);
    matrix_free(&dest);
    matrix_free(&scratch);
}

void test_matrix_inverse_into_reports_a_singular_matrix(void)
{
    matrix_t matrix = matrix_alloc(2, 2);
    matrix_add_element(&matrix, 0, 0, 1);
    matrix_add_element(&matrix, 0, 1, 2);
    matrix_add_element(&matrix, 1, 0, 2);
    matrix_add_element(&matrix, 1, 1, 4);
    matrix_t dest = matrix_alloc(2, 2);
    matrix_t scratch = matrix_alloc(2, 4);

    TEST_ASSERT_EQUAL(false, matrix_inverse_into(&matrix, &dest, &scratch));

    matrix_free(&matrix);
    matrix_free(&dest);
    matrix_free(&scratch);
}

void test_the_operations_that_write_into_a_matrix_need_no_heap(void)
{
    // Every matrix here holds memory that the caller gives. Thus this test
    // shows that a program with no heap can do the operations.
    real_t elements_a[4] = {1, 2, 3, 4};
    real_t elements_b[4] = {5, 6, 7, 8};
    real_t elements_dest[4];
    real_t elements_scratch[8];

    matrix_t a = matrix_static_alloc(2, 2, elements_a);
    matrix_t b = matrix_static_alloc(2, 2, elements_b);
    matrix_t dest = matrix_static_alloc(2, 2, elements_dest);
    matrix_t scratch = matrix_static_alloc(2, 4, elements_scratch);

    matrix_add_into(&a, &b, &dest);
    TEST_ASSERT_EQUAL(6, matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_EQUAL(12, matrix_get_element(&dest, 1, 1));

    matrix_subtract_into(&b, &a, &dest);
    TEST_ASSERT_EQUAL(4, matrix_get_element(&dest, 0, 0));

    matrix_multiply_into(&a, &b, &dest);
    TEST_ASSERT_EQUAL(19, matrix_get_element(&dest, 0, 0));

    matrix_transpose_into(&a, &dest);
    TEST_ASSERT_EQUAL(3, matrix_get_element(&dest, 0, 1));

    matrix_set_unit(&dest);
    TEST_ASSERT_EQUAL(true, matrix_is_unit(&dest));

    TEST_ASSERT_EQUAL(true, matrix_inverse_into(&a, &dest, &scratch));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), -REAL_C(2.0), matrix_get_element(&dest, 0, 0));

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&dest);
    matrix_free(&scratch);
}
