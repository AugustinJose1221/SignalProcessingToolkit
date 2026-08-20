#include "unity.h"
#include "real_assert.h"
#include "real.h"
#include "pmatrix.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265)

void setUp(void)
{

}

void tearDown(void)
{

}

// A function of the standard library fits the type of an element directly,
// because real_sin takes a float and gives a float. These two functions give the
// parts of a rotation matrix that the library does not hold.
static real_t negative_sine(real_t x)
{
    return -REAL_SIN(x);
}

static real_t twice(real_t x)
{
    return REAL_C(2.0) * x;
}

void test_pmatrix_alloc(void)
{
    pmatrix_t matrix = pmatrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(2, matrix.m);
    TEST_ASSERT_EQUAL(3, matrix.n);
    TEST_ASSERT_EQUAL(true, matrix.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(matrix.elem);
    pmatrix_free(&matrix);
}

void test_pmatrix_alloc_gives_a_matrix_of_zero(void)
{
    pmatrix_t matrix = pmatrix_alloc(2, 2);

    for(uint32_t i = 0; i < 2; i++)
    {
        for(uint32_t j = 0; j < 2; j++)
        {
            TEST_ASSERT_NULL(pmatrix_get_element(&matrix, i, j));
            TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), pmatrix_evaluate_element(&matrix, i, j, REAL_C(1.0)));
        }
    }

    pmatrix_free(&matrix);
}

void test_pmatrix_static_alloc(void)
{
    pmatrix_function_t elements[4];
    pmatrix_t matrix = pmatrix_static_alloc(2, 2, elements);

    TEST_ASSERT_EQUAL(2, matrix.m);
    TEST_ASSERT_EQUAL(2, matrix.n);
    TEST_ASSERT_EQUAL(false, matrix.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(elements, matrix.elem);

    pmatrix_free(&matrix);

    // The memory belongs to the caller, thus the pointer does not change.
    TEST_ASSERT_EQUAL_PTR(elements, matrix.elem);
}

void test_pmatrix_add_element_and_get_element(void)
{
    pmatrix_t matrix = pmatrix_alloc(1, 2);

    pmatrix_add_element(&matrix, 0, 0, real_sin);
    pmatrix_add_element(&matrix, 0, 1, real_cos);

    TEST_ASSERT_EQUAL_PTR(real_sin, pmatrix_get_element(&matrix, 0, 0));
    TEST_ASSERT_EQUAL_PTR(real_cos, pmatrix_get_element(&matrix, 0, 1));

    pmatrix_free(&matrix);
}

void test_pmatrix_evaluate_element(void)
{
    pmatrix_t matrix = pmatrix_alloc(1, 1);
    pmatrix_add_element(&matrix, 0, 0, twice);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(6.0), pmatrix_evaluate_element(&matrix, 0, 0, REAL_C(3.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(1.0), pmatrix_evaluate_element(&matrix, 0, 0, -REAL_C(0.5)));

    pmatrix_free(&matrix);
}

void test_pmatrix_evaluate_element_of_an_element_that_holds_nothing(void)
{
    pmatrix_t matrix = pmatrix_alloc(1, 1);

    pmatrix_add_element(&matrix, 0, 0, NULL);

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), pmatrix_evaluate_element(&matrix, 0, 0, REAL_C(42.0)));

    pmatrix_free(&matrix);
}

void test_pmatrix_evaluate_the_example_from_the_description(void)
{
    // [ sin(x)  cos(x) ]
    pmatrix_t matrix = pmatrix_alloc(1, 2);
    pmatrix_add_element(&matrix, 0, 0, real_sin);
    pmatrix_add_element(&matrix, 0, 1, real_cos);

    matrix_t at_zero = pmatrix_evaluate(&matrix, REAL_C(0.0));
    TEST_ASSERT_EQUAL(1, at_zero.m);
    TEST_ASSERT_EQUAL(2, at_zero.n);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&at_zero, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&at_zero, 0, 1));

    matrix_t at_quarter_turn = pmatrix_evaluate(&matrix, PI/REAL_C(2.0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&at_quarter_turn, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&at_quarter_turn, 0, 1));

    matrix_free(&at_zero);
    matrix_free(&at_quarter_turn);
    pmatrix_free(&matrix);
}

void test_pmatrix_a_rotation_matrix_turns_a_vector(void)
{
    // [ cos(x)  -sin(x) ]
    // [ sin(x)   cos(x) ]
    //
    // A quarter turn moves the point [1, 0] to the point [0, 1].
    pmatrix_t rotation = pmatrix_alloc(2, 2);
    pmatrix_add_element(&rotation, 0, 0, real_cos);
    pmatrix_add_element(&rotation, 0, 1, negative_sine);
    pmatrix_add_element(&rotation, 1, 0, real_sin);
    pmatrix_add_element(&rotation, 1, 1, real_cos);

    matrix_t values = pmatrix_evaluate(&rotation, PI/REAL_C(2.0));

    matrix_t point = matrix_alloc(2, 1);
    matrix_add_element(&point, 0, 0, REAL_C(1.0));
    matrix_add_element(&point, 1, 0, REAL_C(0.0));

    matrix_t turned = matrix_multiply(&values, &point);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&turned, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&turned, 1, 0));

    // A rotation does not change the length, thus the determinant is one.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_determinant(&values));

    matrix_free(&values);
    matrix_free(&point);
    matrix_free(&turned);
    pmatrix_free(&rotation);
}

void test_pmatrix_evaluate_into_needs_no_heap(void)
{
    // Every matrix here holds memory that the caller gives.
    pmatrix_function_t elements[4];
    real_t values[4];

    pmatrix_t matrix = pmatrix_static_alloc(2, 2, elements);
    matrix_t dest = matrix_static_alloc(2, 2, values);

    pmatrix_add_element(&matrix, 0, 0, real_cos);
    pmatrix_add_element(&matrix, 0, 1, negative_sine);
    pmatrix_add_element(&matrix, 1, 0, real_sin);
    pmatrix_add_element(&matrix, 1, 1, real_cos);

    pmatrix_evaluate_into(&matrix, REAL_C(0.0), &dest);

    TEST_ASSERT_EQUAL(true, matrix_is_unit(&dest));

    pmatrix_evaluate_into(&matrix, PI, &dest);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(1.0), matrix_get_element(&dest, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, -REAL_C(1.0), matrix_get_element(&dest, 1, 1));

    matrix_free(&dest);
    pmatrix_free(&matrix);
}

void test_pmatrix_evaluate_into_gives_a_new_result_for_each_value(void)
{
    pmatrix_t matrix = pmatrix_alloc(1, 1);
    pmatrix_add_element(&matrix, 0, 0, twice);
    matrix_t dest = matrix_alloc(1, 1);

    for(int step = 0; step < 5; step++)
    {
        pmatrix_evaluate_into(&matrix, (real_t)step, &dest);
        TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0)*(real_t)step,
                                 matrix_get_element(&dest, 0, 0));
    }

    matrix_free(&dest);
    pmatrix_free(&matrix);
}

void test_pmatrix_set_zero(void)
{
    pmatrix_t matrix = pmatrix_alloc(2, 2);
    pmatrix_add_element(&matrix, 0, 0, real_sin);
    pmatrix_add_element(&matrix, 1, 1, real_cos);

    pmatrix_set_zero(&matrix);

    matrix_t values = pmatrix_evaluate(&matrix, REAL_C(1.0));
    TEST_ASSERT_EQUAL(true, matrix_is_zero(&values));

    matrix_free(&values);
    pmatrix_free(&matrix);
}

void test_pmatrix_zero_and_one(void)
{
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), pmatrix_zero(REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), pmatrix_zero(-REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), pmatrix_one(REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(1.0), pmatrix_one(-REAL_C(5.0)));
}

void test_pmatrix_the_two_constant_functions_work_as_elements(void)
{
    // [ sin(x)  1 ]
    // [ 0       1 ]
    pmatrix_t matrix = pmatrix_alloc(2, 2);
    pmatrix_add_element(&matrix, 0, 0, real_sin);
    pmatrix_add_element(&matrix, 0, 1, pmatrix_one);
    pmatrix_add_element(&matrix, 1, 0, pmatrix_zero);
    pmatrix_add_element(&matrix, 1, 1, pmatrix_one);

    matrix_t values = pmatrix_evaluate(&matrix, PI/REAL_C(2.0));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&values, 0, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&values, 0, 1));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), matrix_get_element(&values, 1, 0));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), matrix_get_element(&values, 1, 1));

    matrix_free(&values);
    pmatrix_free(&matrix);
}

void test_pmatrix_free(void)
{
    pmatrix_t matrix = pmatrix_alloc(2, 2);
    pmatrix_free(&matrix);
    TEST_ASSERT_EQUAL(false, matrix.dynamic_alloc);
    // A second call must do nothing.
    pmatrix_free(&matrix);
}
