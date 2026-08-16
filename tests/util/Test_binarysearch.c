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
void test_binarysearch_gives_an_index_that_is_inside_the_list(void)
{
    // Every value of the list is less than the value that the caller looks
    // for. The search must still give an index that the caller can use. An
    // index equal to the size would make the caller read after the end of the
    // list.
    float data[3] = {1, 2, 3};

    TEST_ASSERT_EQUAL(2, binarysearch_get_index(data, 99, 3));
    TEST_ASSERT_EQUAL(2, binarysearch_get_index(data, 3, 3));
}

void test_binarysearch_gives_the_first_index_for_a_small_value(void)
{
    float data[3] = {1, 2, 3};

    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, -99, 3));
    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, 1, 3));
}

void test_binarysearch_with_one_element(void)
{
    float data[1] = {5};

    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, 5, 1));
    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, 99, 1));
    TEST_ASSERT_EQUAL(0, binarysearch_get_index(data, -99, 1));
}
