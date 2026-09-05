#include "unity.h"
#include "real_assert.h"
#include "emd.h"
#include <stdlib.h>
#include <string.h>
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
    // The spline must say that it got its memory. emd_alloc reads the size of
    // the spline to find out whether the spline got what it asked for, and it
    // frees everything when the size is zero. A spline left as the stack found
    // it holds whatever was there before, thus this test passed or failed by
    // luck: it passed on one machine and failed on the build machine, where
    // that stack held zeros.
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    memset(&cspline, 0, sizeof(cspline));
    memset(&cspline_mempool, 0, sizeof(cspline_mempool));
    cspline.size = 3;
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
    real_t peak_buffer[3];
    real_t valley_buffer[3];
    real_t membank[5];
    real_t mempool[5];
    real_t *membank_ptr[5] = {membank, membank, membank, membank, membank};
    real_t *mempool_ptr = mempool;
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    memset(&cspline, 0, sizeof(cspline));
    memset(&cspline_mempool, 0, sizeof(cspline_mempool));
    cspline.size = 3;
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
    real_t x[3] = {1, 2, 3};
    real_t y[3] = {4, 5, 6};
    real_t residue[3] = {7, 8, 9};
    real_t working_buffer[3] = {10, 11, 12};
    real_t peak_index_buffer[3] = {13, 14, 15};
    real_t valley_index_buffer[3] = {16, 17, 18};
    imf_t imf[3];
    // emd_initialize writes the fields it is given and reads none of them,
    // thus this struct is safe as it stands. It is set to zero all the same,
    // because a test that leaves a struct as the stack found it is safe only
    // until the function it calls begins to read a field. That is how the
    // fault in test_emd_alloc came about.
    emd_t emd;
    memset(&emd, 0, sizeof(emd));
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
