#include "unity.h"
#include "emd.h"
#include <stdlib.h>
#include "Mock_peakdetect.h"
#include "Mock_valleydetect.h"
#include "Mock_cspline.h"
#include "Mock_imf.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_emd_alloc(void)
{
    cspline_t cspline;;
    cspline_mempool_t cspline_mempool;
    cspline_alloc_ExpectAndReturn(3, cspline);
    cspline_alloc_mempool_ExpectAndReturn(3, cspline_mempool);
    emd_t emd = emd_alloc(3);
    TEST_ASSERT_EQUAL(3, emd.size);
    TEST_ASSERT_EQUAL(true, emd.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(emd.peak_buffer);
    TEST_ASSERT_NOT_NULL(emd.valley_buffer);
    cspline_free_Ignore();
    cspline_free_mempool_Ignore();
    emd_free(emd);
}

void test_emd_static_alloc(void)
{
    float peak_buffer[3];
    float valley_buffer[3];
    float membank[5];
    float mempool[5];
    float *membank_ptr[5] = {membank, membank, membank, membank, membank};
    float *mempool_ptr = mempool;
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    cspline_static_alloc_ExpectAndReturn(3, membank_ptr, cspline);
    cspline_static_alloc_mempool_ExpectAndReturn(mempool_ptr, cspline_mempool);
    emd_t emd = emd_static_alloc(3, membank_ptr, mempool_ptr, peak_buffer, valley_buffer);
    TEST_ASSERT_EQUAL(3, emd.size);
    TEST_ASSERT_EQUAL(false, emd.dynamic_alloc);
    TEST_ASSERT_EQUAL(peak_buffer, emd.peak_buffer);
    TEST_ASSERT_EQUAL(valley_buffer, emd.valley_buffer);
    emd_free(emd);
}

void test_emd_initialize(void)
{
    float x[3] = {1, 2, 3};
    float y[3] = {4, 5, 6};
    float residue[3] = {7, 8, 9};
    float working_buffer[3] = {10, 11, 12};
    float peak_index_buffer[3] = {13, 14, 15};
    float valley_index_buffer[3] = {16, 17, 18};
    imf_t imf[3];
    emd_t emd;
    emd.valley_buffer = valley_index_buffer;
    emd_initialize(&emd, 3, imf, x, y, residue, working_buffer, peak_index_buffer, valley_index_buffer);
    TEST_ASSERT_EQUAL(3, emd.imf_count);
    TEST_ASSERT_EQUAL(x, emd.x);
    TEST_ASSERT_EQUAL(y, emd.y);
    TEST_ASSERT_EQUAL(residue, emd.residue);
    TEST_ASSERT_EQUAL(working_buffer, emd.working_buffer);
    TEST_ASSERT_EQUAL(peak_index_buffer, emd.peak_index_buffer);
    TEST_ASSERT_EQUAL(valley_index_buffer, emd.valley_index_buffer);
}