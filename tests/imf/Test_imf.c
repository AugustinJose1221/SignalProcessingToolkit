#include "unity.h"
#include "imf.h"
#include <stdlib.h>

void setUp(void)
{

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