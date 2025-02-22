#include "unity.h"
#include "peakdetect.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_peakdetect_get_peaks(void)
{
    float input[5] = {1, 2, 3, 2, 1};
    float index_buffer[5];
    float peak_buffer[5];
    uint32_t peakcount = peakdetect_get_peaks(input, index_buffer, peak_buffer, 5);
    TEST_ASSERT_EQUAL(1, peakcount);
    TEST_ASSERT_EQUAL(3, peak_buffer[0]);
    TEST_ASSERT_EQUAL(2, index_buffer[0]);

    float input2[2] = {1, 2};
    float index_buffer2[2];
    float peak_buffer2[2];
    uint32_t peakcount2 = peakdetect_get_peaks(input2, index_buffer2, peak_buffer2, 2);
    TEST_ASSERT_EQUAL(0, peakcount2);
}