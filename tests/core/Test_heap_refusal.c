// WHAT EVERY ALLOCATOR DOES WHEN THE HEAP GIVES NOTHING.
//
// Every module that takes memory offers a *_alloc that takes it from the heap.
// That call can fail, and ffitt/core/README.md says what each one must then
// do: give back what it got, leave every pointer NULL, set the size fields to
// nothing, set dynamic_alloc to false, and stay safe to free.
//
// Eleven of them did none of that. They read the answer of malloc straight
// into the struct, and four of those then cleared the list they had just
// failed to get, which is a write through NULL. Seven more were found after.
// Nothing caught any of it, because a test cannot ask the heap to refuse.
//
// HOW THIS ASKS. The linker is told, for this one executable only, to send
// malloc and calloc through the two functions below. They refuse while a
// switch is on and hand the request to the real allocator while it is off,
// thus the test framework around them keeps working. project.yml carries the
// flag that arranges it.

#include "unity.h"
#include "real_assert.h"

#include "ringbuf.h"
#include "imf.h"
#include "emd.h"
#include "fir.h"
#include "iir.h"
#include "medfilt.h"
#include "savgol.h"
#include "hampel.h"
#include "adaptive.h"
#include "matrix.h"
#include "cmatrix.h"
#include "pmatrix.h"
#include "vector.h"
#include "cspline.h"
#include "kalman.h"
#include "fft.h"
#include "psd.h"
#include "slide.h"

// The modules that the ones above stand on. They are named so that the test
// links against them, which is how ceedling decides what to build.
#include "cnum.h"
#include "window.h"
#include "real.h"
#include "peakdetect.h"
#include "valleydetect.h"
#include "binarysearch.h"
#include "stats.h"

#include <stdlib.h>
#include <stdbool.h>

void* __real_malloc(size_t size);
void* __real_calloc(size_t count, size_t size);

static bool heap_refuses = false;

void* __wrap_malloc(size_t size)
{
    return heap_refuses ? NULL : __real_malloc(size);
}

void* __wrap_calloc(size_t count, size_t size)
{
    return heap_refuses ? NULL : __real_calloc(count, size);
}

void setUp(void)
{
    heap_refuses = false;
}

void tearDown(void)
{
    heap_refuses = false;
}

void test_a_buffer_of_the_last_samples_holds_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    ringbuf_t buffer = ringbuf_alloc(64u);
    heap_refuses = false;

    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_UINT32(0u, buffer.size);
    TEST_ASSERT_FALSE(buffer.dynamic_alloc);

    ringbuf_free(&buffer);
}

void test_a_matrix_holds_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    matrix_t matrix = matrix_alloc(4u, 4u);
    cmatrix_t complex_matrix = cmatrix_alloc(4u, 4u);
    pmatrix_t of_functions = pmatrix_alloc(4u, 4u);
    vector_t vector = vector_alloc(16u);
    heap_refuses = false;

    TEST_ASSERT_NULL(matrix.elem);
    TEST_ASSERT_EQUAL_UINT32(0u, matrix.m);
    TEST_ASSERT_FALSE(matrix.dynamic_alloc);

    TEST_ASSERT_NULL(complex_matrix.elem);
    TEST_ASSERT_EQUAL_UINT32(0u, complex_matrix.m);

    // The clearing that follows the allocation walks every place, thus this
    // one would have written through nothing.
    TEST_ASSERT_NULL(of_functions.elem);
    TEST_ASSERT_EQUAL_UINT32(0u, of_functions.m);

    TEST_ASSERT_NULL(vector.data);
    TEST_ASSERT_EQUAL_UINT32(0u, vector.size);

    matrix_free(&matrix);
    cmatrix_free(&complex_matrix);
    pmatrix_free(&of_functions);
    vector_free(&vector);
}

void test_the_filters_hold_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    fir_t fir = fir_alloc(32u);
    iir_t iir = iir_alloc(2u);
    medfilt_t medfilt = medfilt_alloc(5u);
    savgol_t savgol = savgol_alloc(5u);
    hampel_t hampel = hampel_alloc(5u);
    adaptive_t adaptive = adaptive_alloc(8u);
    heap_refuses = false;

    // Each of these cleared the list it had just failed to get.
    TEST_ASSERT_NULL(fir.coefficient);
    TEST_ASSERT_EQUAL_UINT32(0u, fir.length);

    TEST_ASSERT_NULL(iir.coefficient);
    TEST_ASSERT_EQUAL_UINT32(0u, iir.sections);

    TEST_ASSERT_NULL(medfilt.sorted);
    TEST_ASSERT_NULL(savgol.coefficient);
    TEST_ASSERT_EQUAL_UINT32(0u, savgol.window);

    TEST_ASSERT_NULL(hampel.distance);
    TEST_ASSERT_NULL(adaptive.coefficient);
    TEST_ASSERT_EQUAL_UINT32(0u, adaptive.length);

    fir_free(&fir);
    iir_free(&iir);
    medfilt_free(&medfilt);
    savgol_free(&savgol);
    hampel_free(&hampel);
    adaptive_free(&adaptive);
}

void test_the_transforms_hold_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    fft_t fft = fft_alloc(64u);
    psd_t psd = psd_alloc(64u);
    slide_t slide = slide_alloc(64u, 2u);
    heap_refuses = false;

    // The tables of the transform are written through both lists.
    TEST_ASSERT_NULL(fft.twiddle);
    TEST_ASSERT_EQUAL_UINT32(0u, fft.size);

    TEST_ASSERT_NULL(psd.window);
    TEST_ASSERT_EQUAL_UINT32(0u, psd.block);

    // The building of a watcher writes a turning factor for every frequency
    // it holds, thus it too must not be reached with nothing to write to.
    TEST_ASSERT_NULL(slide.total);
    TEST_ASSERT_EQUAL_UINT32(0u, slide.size);
    TEST_ASSERT_EQUAL_UINT32(0u, slide.count);

    fft_free(&fft);
    psd_free(&psd);
    slide_free(&slide);
}

void test_the_spline_and_what_stands_on_it_hold_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    cspline_t spline = cspline_alloc(8u);
    cspline_mempool_t mempool = cspline_alloc_mempool(8u);
    emd_t emd = emd_alloc(8u);
    imf_t imf = imf_alloc(8u);
    heap_refuses = false;

    TEST_ASSERT_NULL(spline.x);
    TEST_ASSERT_EQUAL_UINT32(0u, spline.size);

    TEST_ASSERT_NULL(mempool.d);

    TEST_ASSERT_NULL(emd.peak_buffer);
    TEST_ASSERT_EQUAL_UINT32(0u, emd.size);

    TEST_ASSERT_NULL(imf.x);
    TEST_ASSERT_EQUAL_UINT32(0u, imf.size);

    cspline_free(spline);
    cspline_free_mempool(mempool);
    emd_free(emd);
    imf_free(imf);
}

void test_the_filter_of_kalman_holds_nothing_when_the_heap_refuses(void)
{
    heap_refuses = true;
    kalman_t kalman = kalman_alloc(1u, 2u, 1u);
    heap_refuses = false;

    // The matrices are laid out across the pool, thus there must be a pool.
    TEST_ASSERT_NULL(kalman.mempool);
    TEST_ASSERT_EQUAL_UINT32(0u, kalman.nx);
    TEST_ASSERT_FALSE(kalman.dynamic_alloc);

    kalman_free(&kalman);
}
