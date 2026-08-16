#include "unity.h"
#include "imf.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// A print function that keeps the text. The tests use it to examine what the
// print functions of the module write.
#define CAPTURE_SIZE    1024
static char capture_buffer[CAPTURE_SIZE];
static size_t capture_length;

static int capture_printf(const char* format, ...)
{
    va_list arguments;
    int written;

    va_start(arguments, format);
    written = vsnprintf(&capture_buffer[capture_length], CAPTURE_SIZE - capture_length, format, arguments);
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

void test_imf_alloc(void)
{
    imf_t imf = imf_alloc(3);
    TEST_ASSERT_EQUAL(3, imf.size);
    TEST_ASSERT_EQUAL(true, imf.dynamic_alloc);
    imf_free(imf);
}

void test_imf_static_alloc(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    imf_t imf = imf_static_alloc(3, x, y);
    TEST_ASSERT_EQUAL(3, imf.size);
    TEST_ASSERT_EQUAL(false, imf.dynamic_alloc);
    imf_free(imf);
}

void test_imf_printf(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    imf_t imf = imf_static_alloc(3, x, y);
    imf_printf(&imf, printf);

    imf_printf(&imf, NULL);
}

void test_imf_print_all(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    imf_t imf = imf_static_alloc(3, x, y);
    imf_print_all(&imf, 3, 1, printf);

    imf_print_all(&imf, 3, 1, NULL);

    imf_t imflist[3] = {imf, imf, imf};
    imf_print_all(imflist, 3, 3, printf);
}

void test_imf_printf_writes_one_line_for_each_point(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    imf_t imf = imf_static_alloc(3, x, y);

    imf_printf(&imf, capture_printf);

    TEST_ASSERT_EQUAL_STRING("1.000000, 4.000000\n"
                             "2.000000, 5.000000\n"
                             "3.000000, 6.000000\n", capture_buffer);
}

void test_imf_print_all_writes_one_column_for_each_function(void)
{
    float x[2] = {1, 2};
    float y_first[2] = {10, 20};
    float y_second[2] = {30, 40};
    imf_t imflist[2];
    imflist[0] = imf_static_alloc(2, x, y_first);
    imflist[1] = imf_static_alloc(2, x, y_second);

    imf_print_all(imflist, 2, 2, capture_printf);

    TEST_ASSERT_EQUAL_STRING("10.000000, 30.000000\n"
                             "20.000000, 40.000000\n", capture_buffer);
}

void test_imf_free_releases_the_memory_of_a_dynamic_function(void)
{
    imf_t imf = imf_alloc(4);
    TEST_ASSERT_NOT_NULL(imf.x);
    TEST_ASSERT_NOT_NULL(imf.y);

    for(uint32_t index = 0; index < imf.size; index++)
    {
        imf.x[index] = (float)index;
        imf.y[index] = (float)index * 2.0f;
    }

    imf_free(imf);
}

void test_imf_free_keeps_the_memory_of_a_static_function(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    imf_t imf = imf_static_alloc(3, x, y);

    imf_free(imf);

    // The memory belongs to the caller. It must still hold the values.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, x[0]);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, y[2]);
    TEST_ASSERT_EQUAL_PTR(x, imf.x);
}