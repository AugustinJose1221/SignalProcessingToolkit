#include "unity.h"
#include "cmatrix.h"
#include "cnum.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TOLERANCE   0.0001f

#define CAPTURE_SIZE    1024
static char capture_buffer[CAPTURE_SIZE];
static size_t capture_length;

static int capture_printf(const char* format, ...)
{
    va_list arguments;
    int written;

    va_start(arguments, format);
    written = vsnprintf(&capture_buffer[capture_length], CAPTURE_SIZE - capture_length,
                        format, arguments);
    va_end(arguments);

    if(written > 0)
    {
        capture_length += (size_t)written;
    }

    return written;
}

void setUp(void)
{
    capture_buffer[0] = '\0';
    capture_length = 0;
}

void tearDown(void)
{

}

// Fill a matrix from a list of real parts and a list of imaginary parts.
static void fill(cmatrix_t* matrix, const float* real, const float* imaginary)
{
    for(uint32_t i = 0; i < matrix->m; i++)
    {
        for(uint32_t j = 0; j < matrix->n; j++)
        {
            uint32_t index = (i*matrix->n) + j;
            cmatrix_add_element(matrix, i, j, cnum_make(real[index], imaginary[index]));
        }
    }
}

void test_cmatrix_alloc(void)
{
    cmatrix_t matrix = cmatrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(2, matrix.m);
    TEST_ASSERT_EQUAL(3, matrix.n);
    TEST_ASSERT_EQUAL(true, matrix.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(matrix.elem);
    cmatrix_free(&matrix);
}

void test_cmatrix_static_alloc(void)
{
    cnum_t elements[4];
    cmatrix_t matrix = cmatrix_static_alloc(2, 2, elements);
    TEST_ASSERT_EQUAL(2, matrix.m);
    TEST_ASSERT_EQUAL(2, matrix.n);
    TEST_ASSERT_EQUAL(false, matrix.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(elements, matrix.elem);
    cmatrix_free(&matrix);
    TEST_ASSERT_EQUAL_PTR(elements, matrix.elem);
}

void test_cmatrix_add_element_and_get_element(void)
{
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_add_element(&matrix, 0, 1, cnum_make(1.0f, -2.0f));
    cnum_t value = cmatrix_get_element(&matrix, 0, 1);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, value.re);
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, value.im);
    cmatrix_free(&matrix);
}

void test_cmatrix_create_unit_matrix(void)
{
    cmatrix_t matrix = cmatrix_create_unit_matrix(3);
    TEST_ASSERT_EQUAL(true, cmatrix_is_unit(&matrix));
    TEST_ASSERT_EQUAL(false, cmatrix_is_zero(&matrix));
    cmatrix_free(&matrix);
}

void test_cmatrix_create_zero_matrix(void)
{
    cmatrix_t matrix = cmatrix_create_zero_matrix(2, 3);
    TEST_ASSERT_EQUAL(true, cmatrix_is_zero(&matrix));
    cmatrix_free(&matrix);
}

void test_cmatrix_is_equal(void)
{
    float real[4] = {1, 2, 3, 4};
    float imaginary[4] = {5, 6, 7, 8};
    cmatrix_t a = cmatrix_alloc(2, 2);
    cmatrix_t b = cmatrix_alloc(2, 2);
    cmatrix_t c = cmatrix_alloc(1, 2);
    fill(&a, real, imaginary);
    fill(&b, real, imaginary);

    TEST_ASSERT_EQUAL(true, cmatrix_is_equal(&a, &b));
    TEST_ASSERT_EQUAL(false, cmatrix_is_equal(&a, &c));

    cmatrix_add_element(&b, 1, 1, cnum_make(0.0f, 0.0f));
    TEST_ASSERT_EQUAL(false, cmatrix_is_equal(&a, &b));

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&c);
}

void test_cmatrix_is_near(void)
{
    cmatrix_t a = cmatrix_alloc(1, 1);
    cmatrix_t b = cmatrix_alloc(1, 1);
    cmatrix_t c = cmatrix_alloc(2, 1);
    cmatrix_add_element(&a, 0, 0, cnum_make(1.0f, 2.0f));
    cmatrix_add_element(&b, 0, 0, cnum_make(1.001f, 2.001f));

    TEST_ASSERT_EQUAL(true, cmatrix_is_near(&a, &b, 0.01f));
    TEST_ASSERT_EQUAL(false, cmatrix_is_near(&a, &b, 0.00001f));
    TEST_ASSERT_EQUAL(false, cmatrix_is_near(&a, &c, 1.0f));

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&c);
}

void test_cmatrix_is_square(void)
{
    cmatrix_t square = cmatrix_alloc(3, 3);
    cmatrix_t oblong = cmatrix_alloc(2, 3);
    TEST_ASSERT_EQUAL(true, cmatrix_is_square(&square));
    TEST_ASSERT_EQUAL(false, cmatrix_is_square(&oblong));
    cmatrix_free(&square);
    cmatrix_free(&oblong);
}

void test_cmatrix_is_multipliable(void)
{
    cmatrix_t a = cmatrix_alloc(2, 3);
    cmatrix_t b = cmatrix_alloc(3, 4);
    cmatrix_t c = cmatrix_alloc(2, 2);
    TEST_ASSERT_EQUAL(true, cmatrix_is_multipliable(&a, &b));
    TEST_ASSERT_EQUAL(false, cmatrix_is_multipliable(&a, &c));
    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&c);
}

void test_cmatrix_is_hermitian(void)
{
    // [[1, 2+i], [2-i, 3]] does not change when the conjugate transpose is
    // taken.
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 0.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(2.0f, 1.0f));
    cmatrix_add_element(&matrix, 1, 0, cnum_make(2.0f, -1.0f));
    cmatrix_add_element(&matrix, 1, 1, cnum_make(3.0f, 0.0f));

    TEST_ASSERT_EQUAL(true, cmatrix_is_hermitian(&matrix));

    // A value on the diagonal that is not real breaks the rule.
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 1.0f));
    TEST_ASSERT_EQUAL(false, cmatrix_is_hermitian(&matrix));

    cmatrix_t oblong = cmatrix_create_zero_matrix(2, 3);
    TEST_ASSERT_EQUAL(false, cmatrix_is_hermitian(&oblong));

    cmatrix_free(&matrix);
    cmatrix_free(&oblong);
}

void test_cmatrix_add(void)
{
    float real_a[4] = {1, 2, 3, 4};
    float imaginary_a[4] = {1, 1, 1, 1};
    float real_b[4] = {10, 20, 30, 40};
    float imaginary_b[4] = {-1, -1, -1, -1};

    cmatrix_t a = cmatrix_alloc(2, 2);
    cmatrix_t b = cmatrix_alloc(2, 2);
    fill(&a, real_a, imaginary_a);
    fill(&b, real_b, imaginary_b);

    cmatrix_t sum = cmatrix_add(&a, &b);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 11.0f, cmatrix_get_element(&sum, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, cmatrix_get_element(&sum, 0, 0).im);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 44.0f, cmatrix_get_element(&sum, 1, 1).re);

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&sum);
}

void test_cmatrix_subtract(void)
{
    cmatrix_t a = cmatrix_alloc(1, 2);
    cmatrix_add_element(&a, 0, 0, cnum_make(5.0f, 5.0f));
    cmatrix_add_element(&a, 0, 1, cnum_make(1.0f, 0.0f));
    cmatrix_t b = cmatrix_alloc(1, 2);
    cmatrix_add_element(&b, 0, 0, cnum_make(2.0f, 8.0f));
    cmatrix_add_element(&b, 0, 1, cnum_make(1.0f, 0.0f));

    cmatrix_t difference = cmatrix_subtract(&a, &b);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, cmatrix_get_element(&difference, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -3.0f, cmatrix_get_element(&difference, 0, 0).im);
    TEST_ASSERT_EQUAL(true, cnum_is_zero(cmatrix_get_element(&difference, 0, 1)));

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&difference);
}

void test_cmatrix_multiply(void)
{
    // [[i]] times [[i]] is [[-1]].
    cmatrix_t a = cmatrix_alloc(1, 1);
    cmatrix_add_element(&a, 0, 0, cnum_make(0.0f, 1.0f));

    cmatrix_t product = cmatrix_multiply(&a, &a);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, cmatrix_get_element(&product, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, cmatrix_get_element(&product, 0, 0).im);

    cmatrix_free(&a);
    cmatrix_free(&product);
}

void test_cmatrix_multiply_gives_the_correct_order(void)
{
    cmatrix_t a = cmatrix_create_zero_matrix(2, 3);
    cmatrix_t b = cmatrix_create_zero_matrix(3, 4);

    cmatrix_t product = cmatrix_multiply(&a, &b);

    TEST_ASSERT_EQUAL(2, product.m);
    TEST_ASSERT_EQUAL(4, product.n);

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&product);
}

void test_cmatrix_the_unit_matrix_does_not_change_a_matrix(void)
{
    float real[4] = {1, 2, 3, 4};
    float imaginary[4] = {-1, -2, -3, -4};
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    fill(&matrix, real, imaginary);
    cmatrix_t unit = cmatrix_create_unit_matrix(2);

    cmatrix_t product = cmatrix_multiply(&matrix, &unit);

    TEST_ASSERT_EQUAL(true, cmatrix_is_near(&product, &matrix, TOLERANCE));

    cmatrix_free(&matrix);
    cmatrix_free(&unit);
    cmatrix_free(&product);
}

void test_cmatrix_multiply_scalar(void)
{
    cmatrix_t matrix = cmatrix_alloc(1, 1);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 2.0f));

    // (1 + 2i)(3 + 4i) = -5 + 10i
    cmatrix_t product = cmatrix_multiply_scalar(&matrix, cnum_make(3.0f, 4.0f));

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -5.0f, cmatrix_get_element(&product, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 10.0f, cmatrix_get_element(&product, 0, 0).im);

    cmatrix_free(&matrix);
    cmatrix_free(&product);
}

void test_cmatrix_transpose(void)
{
    float real[6] = {1, 2, 3, 4, 5, 6};
    float imaginary[6] = {1, 1, 1, 1, 1, 1};
    cmatrix_t matrix = cmatrix_alloc(2, 3);
    fill(&matrix, real, imaginary);

    cmatrix_t transpose = cmatrix_transpose(&matrix);

    TEST_ASSERT_EQUAL(3, transpose.m);
    TEST_ASSERT_EQUAL(2, transpose.n);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, cmatrix_get_element(&transpose, 0, 1).re);
    // The plain transpose keeps the sign of the imaginary part.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, cmatrix_get_element(&transpose, 0, 1).im);

    cmatrix_free(&matrix);
    cmatrix_free(&transpose);
}

void test_cmatrix_conjugate_transpose(void)
{
    cmatrix_t matrix = cmatrix_alloc(1, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 2.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(3.0f, -4.0f));

    cmatrix_t transpose = cmatrix_conjugate_transpose(&matrix);

    TEST_ASSERT_EQUAL(2, transpose.m);
    TEST_ASSERT_EQUAL(1, transpose.n);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, cmatrix_get_element(&transpose, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -2.0f, cmatrix_get_element(&transpose, 0, 0).im);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, cmatrix_get_element(&transpose, 1, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, cmatrix_get_element(&transpose, 1, 0).im);

    cmatrix_free(&matrix);
    cmatrix_free(&transpose);
}

void test_cmatrix_trace(void)
{
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 2.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(9.0f, 9.0f));
    cmatrix_add_element(&matrix, 1, 0, cnum_make(9.0f, 9.0f));
    cmatrix_add_element(&matrix, 1, 1, cnum_make(3.0f, -5.0f));

    cnum_t trace = cmatrix_trace(&matrix);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, trace.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -3.0f, trace.im);

    cmatrix_free(&matrix);
}

void test_cmatrix_determinant(void)
{
    // The determinant of [[1+i, 2], [3, 4-i]] is (1+i)(4-i) - 6 = -1 + 3i.
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 1.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(2.0f, 0.0f));
    cmatrix_add_element(&matrix, 1, 0, cnum_make(3.0f, 0.0f));
    cmatrix_add_element(&matrix, 1, 1, cnum_make(4.0f, -1.0f));

    cnum_t determinant = cmatrix_determinant(&matrix);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, determinant.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, determinant.im);

    cmatrix_free(&matrix);
}

void test_cmatrix_determinant_of_a_unit_matrix_is_one(void)
{
    cmatrix_t matrix = cmatrix_create_unit_matrix(4);
    cnum_t determinant = cmatrix_determinant(&matrix);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, determinant.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, determinant.im);
    cmatrix_free(&matrix);
}

void test_cmatrix_determinant_of_a_singular_matrix_is_zero(void)
{
    // The second row is two times the first row.
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 1.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(2.0f, 0.0f));
    cmatrix_add_element(&matrix, 1, 0, cnum_make(2.0f, 2.0f));
    cmatrix_add_element(&matrix, 1, 1, cnum_make(4.0f, 0.0f));

    cnum_t determinant = cmatrix_determinant(&matrix);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, determinant.re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, determinant.im);

    cmatrix_free(&matrix);
}

void test_cmatrix_determinant_into(void)
{
    cmatrix_t matrix = cmatrix_create_unit_matrix(3);
    cmatrix_t scratch = cmatrix_alloc(3, 3);

    cnum_t determinant = cmatrix_determinant_into(&matrix, &scratch);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, determinant.re);
    // The matrix itself does not change.
    TEST_ASSERT_EQUAL(true, cmatrix_is_unit(&matrix));

    cmatrix_free(&matrix);
    cmatrix_free(&scratch);
}

void test_cmatrix_inverse(void)
{
    // The inverse of [[i, 0], [0, i]] is [[-i, 0], [0, -i]], because i times
    // -i is 1.
    cmatrix_t matrix = cmatrix_create_zero_matrix(2, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(0.0f, 1.0f));
    cmatrix_add_element(&matrix, 1, 1, cnum_make(0.0f, 1.0f));

    cmatrix_t inverse = cmatrix_inverse(&matrix);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, cmatrix_get_element(&inverse, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, cmatrix_get_element(&inverse, 0, 0).im);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, cmatrix_get_element(&inverse, 1, 1).im);

    cmatrix_free(&matrix);
    cmatrix_free(&inverse);
}

void test_cmatrix_a_matrix_multiplied_by_its_inverse_gives_the_unit_matrix(void)
{
    cmatrix_t matrix = cmatrix_alloc(3, 3);
    float real[9] = {4, 1, 0, 1, 3, 1, 0, 1, 5};
    float imaginary[9] = {0, 1, 0, -1, 0, 2, 0, -2, 0};
    fill(&matrix, real, imaginary);

    cmatrix_t inverse = cmatrix_inverse(&matrix);
    cmatrix_t product = cmatrix_multiply(&matrix, &inverse);
    cmatrix_t unit = cmatrix_create_unit_matrix(3);

    TEST_ASSERT_EQUAL(true, cmatrix_is_near(&product, &unit, 0.001f));

    cmatrix_free(&matrix);
    cmatrix_free(&inverse);
    cmatrix_free(&product);
    cmatrix_free(&unit);
}

void test_cmatrix_inverse_with_a_zero_in_the_pivot_position(void)
{
    // The first element on the diagonal is zero, but the matrix has an
    // inverse. The elimination must exchange the two rows.
    cmatrix_t matrix = cmatrix_create_zero_matrix(2, 2);
    cmatrix_add_element(&matrix, 0, 1, cnum_make(0.0f, 1.0f));
    cmatrix_add_element(&matrix, 1, 0, cnum_make(0.0f, 1.0f));

    cmatrix_t inverse = cmatrix_inverse(&matrix);
    cmatrix_t product = cmatrix_multiply(&matrix, &inverse);
    cmatrix_t unit = cmatrix_create_unit_matrix(2);

    TEST_ASSERT_EQUAL(true, cmatrix_is_near(&product, &unit, 0.001f));

    cmatrix_free(&matrix);
    cmatrix_free(&inverse);
    cmatrix_free(&product);
    cmatrix_free(&unit);
}

void test_cmatrix_inverse_of_a_singular_matrix_is_a_zero_matrix(void)
{
    cmatrix_t matrix = cmatrix_create_zero_matrix(2, 2);

    cmatrix_t inverse = cmatrix_inverse(&matrix);

    TEST_ASSERT_EQUAL(true, cmatrix_is_zero(&inverse));

    cmatrix_free(&matrix);
    cmatrix_free(&inverse);
}

void test_cmatrix_inverse_into_reports_a_singular_matrix(void)
{
    cmatrix_t matrix = cmatrix_create_zero_matrix(2, 2);
    cmatrix_t dest = cmatrix_alloc(2, 2);
    cmatrix_t scratch = cmatrix_alloc(2, 4);

    TEST_ASSERT_EQUAL(false, cmatrix_inverse_into(&matrix, &dest, &scratch));

    cmatrix_free(&matrix);
    cmatrix_free(&dest);
    cmatrix_free(&scratch);
}

void test_cmatrix_copy(void)
{
    float real[4] = {1, 2, 3, 4};
    float imaginary[4] = {5, 6, 7, 8};
    cmatrix_t source = cmatrix_alloc(2, 2);
    cmatrix_t dest = cmatrix_alloc(2, 2);
    fill(&source, real, imaginary);

    cmatrix_copy(&source, &dest);

    TEST_ASSERT_EQUAL(true, cmatrix_is_equal(&source, &dest));

    cmatrix_free(&source);
    cmatrix_free(&dest);
}

void test_cmatrix_printf(void)
{
    cmatrix_t matrix = cmatrix_alloc(1, 2);
    cmatrix_add_element(&matrix, 0, 0, cnum_make(1.0f, 2.0f));
    cmatrix_add_element(&matrix, 0, 1, cnum_make(3.0f, -4.0f));

    cmatrix_printf(&matrix, capture_printf);

    TEST_ASSERT_EQUAL_STRING("1.000000 + 2.000000i\t3.000000 - 4.000000i\t\n",
                             capture_buffer);

    // The default print function must also work.
    cmatrix_printf(&matrix, NULL);

    cmatrix_free(&matrix);
}

void test_cmatrix_free(void)
{
    cmatrix_t matrix = cmatrix_alloc(2, 2);
    cmatrix_free(&matrix);
    TEST_ASSERT_EQUAL(false, matrix.dynamic_alloc);
    // A second call must do nothing.
    cmatrix_free(&matrix);
}

void test_cmatrix_add_into_and_subtract_into(void)
{
    cmatrix_t a = cmatrix_alloc(1, 1);
    cmatrix_t b = cmatrix_alloc(1, 1);
    cmatrix_t dest = cmatrix_alloc(1, 1);
    cmatrix_add_element(&a, 0, 0, cnum_make(1.0f, 2.0f));
    cmatrix_add_element(&b, 0, 0, cnum_make(3.0f, 4.0f));

    cmatrix_add_into(&a, &b, &dest);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, cmatrix_get_element(&dest, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 6.0f, cmatrix_get_element(&dest, 0, 0).im);

    cmatrix_subtract_into(&a, &b, &dest);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -2.0f, cmatrix_get_element(&dest, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -2.0f, cmatrix_get_element(&dest, 0, 0).im);

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&dest);
}

void test_cmatrix_set_unit_and_set_zero(void)
{
    cmatrix_t matrix = cmatrix_alloc(3, 3);

    cmatrix_set_unit(&matrix);
    TEST_ASSERT_EQUAL(true, cmatrix_is_unit(&matrix));

    cmatrix_set_zero(&matrix);
    TEST_ASSERT_EQUAL(true, cmatrix_is_zero(&matrix));

    cmatrix_free(&matrix);
}

void test_the_operations_that_write_into_a_matrix_need_no_heap(void)
{
    // Every matrix here holds memory that the caller gives. Thus a program
    // with no heap can do these operations.
    cnum_t elements_a[4];
    cnum_t elements_b[4];
    cnum_t elements_dest[4];
    cnum_t elements_scratch[8];

    cmatrix_t a = cmatrix_static_alloc(2, 2, elements_a);
    cmatrix_t b = cmatrix_static_alloc(2, 2, elements_b);
    cmatrix_t dest = cmatrix_static_alloc(2, 2, elements_dest);
    cmatrix_t scratch = cmatrix_static_alloc(2, 4, elements_scratch);

    cmatrix_set_unit(&a);
    cmatrix_set_unit(&b);
    cmatrix_multiply_scalar_into(&b, cnum_make(0.0f, 1.0f), &b);

    cmatrix_add_into(&a, &b, &dest);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, cmatrix_get_element(&dest, 0, 0).re);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, cmatrix_get_element(&dest, 0, 0).im);

    cmatrix_multiply_into(&a, &b, &dest);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, cmatrix_get_element(&dest, 0, 0).im);

    cmatrix_transpose_into(&a, &dest);
    TEST_ASSERT_EQUAL(true, cmatrix_is_unit(&dest));

    cmatrix_conjugate_transpose_into(&b, &dest);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, cmatrix_get_element(&dest, 0, 0).im);

    TEST_ASSERT_EQUAL(true, cmatrix_inverse_into(&b, &dest, &scratch));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -1.0f, cmatrix_get_element(&dest, 0, 0).im);

    cmatrix_free(&a);
    cmatrix_free(&b);
    cmatrix_free(&dest);
    cmatrix_free(&scratch);
}
