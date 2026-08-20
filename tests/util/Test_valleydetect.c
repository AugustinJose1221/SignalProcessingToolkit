#include "unity.h"
#include "real_assert.h"
#include "valleydetect.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_valleydetect_get_valley(void)
{
    real_t input[5] = {3, 2, 1, 2, 3};
    real_t index_buffer[5];
    real_t valley_buffer[5];
    uint32_t valleycount = valleydetect_get_valley(input, index_buffer, valley_buffer, 5);
    TEST_ASSERT_EQUAL(1, valleycount);
    TEST_ASSERT_EQUAL(1, valley_buffer[0]);
    TEST_ASSERT_EQUAL(2, index_buffer[0]);

    real_t input2[2] = {1, 2};
    real_t index_buffer2[2];
    real_t valley_buffer2[2];
    uint32_t valleycount2 = valleydetect_get_valley(input2, index_buffer2, valley_buffer2, 2);
    TEST_ASSERT_EQUAL(0, valleycount2);
}
