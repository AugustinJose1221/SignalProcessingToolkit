#include "unity.h"
#include "binarysearch.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_binarysearch_get_index(void)
{
    float data[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, 1, 5));
    TEST_ASSERT_EQUAL(1, binarysearch_get_index(data, 2, 5));
    TEST_ASSERT_EQUAL(2, binarysearch_get_index(data, 3, 5));
    TEST_ASSERT_EQUAL(3, binarysearch_get_index(data, 4, 5));
    TEST_ASSERT_EQUAL(4, binarysearch_get_index(data, 5, 5));
}