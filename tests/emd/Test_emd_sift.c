// The tests in Test_emd.c use mock modules. They examine how the module
// arranges its memory. The tests in this file use the real cspline, imf,
// peakdetect and valleydetect modules, because the sift operation only has a
// meaning together with them.

#include "unity.h"
#include "emd.h"
#include "imf.h"
// The sift operation needs these modules. None of these includes has a mock
// prefix, thus the build takes the real modules and not mocks of them.
#include "cspline.h"
#include "peakdetect.h"
#include "valleydetect.h"
#include "binarysearch.h"
#include <stdlib.h>
#include <math.h>

// The sifting adds and subtracts values that are much larger than the signal.
// Thus the tolerance for the reconstruction grows with the size of the value.
#define RECONSTRUCTION_TOLERANCE(value)     ((0.001f*fabsf(value)) + 0.01f)

#define SAMPLE_SIZE     64u
#define NUMBER_OF_IMF   4u
#define PI              3.14159265f

static float x[SAMPLE_SIZE];
static float y[SAMPLE_SIZE];
static float residue[SAMPLE_SIZE];
static float working_buffer[SAMPLE_SIZE];
static float peak_index_buffer[SAMPLE_SIZE];
static float valley_index_buffer[SAMPLE_SIZE];
static imf_t imf[NUMBER_OF_IMF];

// A signal with two frequencies. The empirical mode decomposition must find
// more than one intrinsic mode function in it.
static void make_signal(void)
{
    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        x[index] = (float)index;
        y[index] = sinf(2.0f*PI*(float)index/8.0f) + (0.5f*sinf(2.0f*PI*(float)index/21.0f));
    }
}

void setUp(void)
{
    make_signal();
    for(uint32_t i = 0; i < NUMBER_OF_IMF; i++)
    {
        imf[i] = imf_alloc(SAMPLE_SIZE);
    }
}

void tearDown(void)
{
    for(uint32_t i = 0; i < NUMBER_OF_IMF; i++)
    {
        imf_free(imf[i]);
    }
}

void test_emd_get_imf_gives_the_function_at_the_given_index(void)
{
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    uint32_t status = 0;

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        residue[index] = y[index];
    }

    imf_t* result = emd_get_imf(&emd, 0, 10, &status);

    TEST_ASSERT_EQUAL_PTR(&imf[0], result);
    TEST_ASSERT_EQUAL(1, status);

    emd_free(emd);
}

void test_emd_get_imf_fills_the_x_values_with_the_sample_number(void)
{
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    uint32_t status = 0;

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        residue[index] = y[index];
    }

    imf_t* result = emd_get_imf(&emd, 0, 10, &status);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        TEST_ASSERT_EQUAL_FLOAT((float)index, result->x[index]);
    }

    emd_free(emd);
}

void test_emd_get_imf_gives_a_function_that_moves_around_zero(void)
{
    // An intrinsic mode function moves around zero, because the sifting takes
    // away the mean of the two envelopes. The function must also stay near the
    // size of the signal.
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    uint32_t status = 0;
    float sum = 0.0f;

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        residue[index] = y[index];
    }

    imf_t* result = emd_get_imf(&emd, 0, 10, &status);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        sum += result->y[index];
        TEST_ASSERT_TRUE(fabsf(result->y[index]) < 5.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, sum/(float)SAMPLE_SIZE);

    emd_free(emd);
}

void test_emd_get_imf_stops_at_the_given_number_of_iterations(void)
{
    // With a threshold of zero the loop does not start. Thus the status stays
    // at zero, and the function gives no result.
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    uint32_t status = 1;

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        residue[index] = y[index];
    }

    emd_get_imf(&emd, 0, 0, &status);

    TEST_ASSERT_EQUAL(0, status);

    emd_free(emd);
}

void test_emd_sift_gives_at_least_one_intrinsic_mode_function(void)
{
    emd_t emd = emd_alloc(SAMPLE_SIZE);

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    uint32_t imf_count = emd_sift(&emd, 3);

    TEST_ASSERT_GREATER_THAN(0, imf_count);
    TEST_ASSERT_TRUE(imf_count <= NUMBER_OF_IMF);

    emd_free(emd);
}

void test_emd_sift_keeps_the_sum_of_the_functions_and_the_residue(void)
{
    // This is the rule that defines the decomposition. The sum of all the
    // intrinsic mode functions and the residue must give the signal again.
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    float original[SAMPLE_SIZE];

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        original[index] = y[index];
    }

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    uint32_t imf_count = emd_sift(&emd, 3);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        float sum = residue[index];
        for(uint32_t i = 0; i < imf_count; i++)
        {
            sum += imf[i].y[index];
        }
        TEST_ASSERT_FLOAT_WITHIN(RECONSTRUCTION_TOLERANCE(original[index]),
                                 original[index], sum);
    }

    emd_free(emd);
}

void test_emd_sift_does_not_give_more_functions_than_the_given_number(void)
{
    emd_t emd = emd_alloc(SAMPLE_SIZE);

    emd_initialize(&emd, 1, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    uint32_t imf_count = emd_sift(&emd, 3);

    TEST_ASSERT_EQUAL(1, imf_count);

    emd_free(emd);
}

void test_emd_sift_with_a_straight_line_keeps_the_signal(void)
{
    // A straight line has no peak and no valley. The sifting must still stop,
    // and the sum of the results must still give the signal again.
    emd_t emd = emd_alloc(SAMPLE_SIZE);
    float original[SAMPLE_SIZE];

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        y[index] = 2.0f*(float)index;
        original[index] = y[index];
    }

    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    uint32_t imf_count = emd_sift(&emd, 3);

    TEST_ASSERT_GREATER_THAN(0, imf_count);
    TEST_ASSERT_TRUE(imf_count <= NUMBER_OF_IMF);

    for(uint32_t index = 0; index < SAMPLE_SIZE; index++)
    {
        float sum = residue[index];
        for(uint32_t i = 0; i < imf_count; i++)
        {
            sum += imf[i].y[index];
        }
        TEST_ASSERT_FLOAT_WITHIN(RECONSTRUCTION_TOLERANCE(original[index]),
                                 original[index], sum);
    }

    emd_free(emd);
}

void test_emd_free_releases_the_memory_of_a_dynamic_decomposition(void)
{
    emd_t emd = emd_alloc(SAMPLE_SIZE);

    TEST_ASSERT_NOT_NULL(emd.peak_buffer);
    TEST_ASSERT_NOT_NULL(emd.valley_buffer);
    TEST_ASSERT_EQUAL(true, emd.dynamic_alloc);

    emd_free(emd);
}

void test_emd_free_keeps_the_memory_of_a_static_decomposition(void)
{
    float membank[5][SAMPLE_SIZE];
    float mempool[5][SAMPLE_SIZE];
    float* membank_pointers[5];
    float* mempool_pointers[5];
    float peak_buffer[SAMPLE_SIZE];
    float valley_buffer[SAMPLE_SIZE];

    for(int i = 0; i < 5; i++)
    {
        membank_pointers[i] = membank[i];
        mempool_pointers[i] = mempool[i];
    }

    emd_t emd = emd_static_alloc(SAMPLE_SIZE, membank_pointers, mempool_pointers,
                                 peak_buffer, valley_buffer);

    TEST_ASSERT_EQUAL(false, emd.dynamic_alloc);

    emd_free(emd);

    // The memory belongs to the caller, thus the pointers must not change.
    TEST_ASSERT_EQUAL_PTR(peak_buffer, emd.peak_buffer);
    TEST_ASSERT_EQUAL_PTR(valley_buffer, emd.valley_buffer);
}
