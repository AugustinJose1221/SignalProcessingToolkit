#include "unity.h"
#include "real_assert.h"
#include "slide.h"
#include "fft.h"
#include "cnum.h"
#include "ringbuf.h"
#include "real.h"

#include <math.h>

#define WINDOW      64u
#define PI          REAL_C(3.14159265358979323846)
#define TOLERANCE   REAL_C(0.02)

void setUp(void) {}
void tearDown(void) {}

static void fill_tone(real_t* into, uint32_t count, real_t bin)
{
    for(uint32_t index = 0; index < count; index++)
    {
        into[index] = REAL_SIN((REAL_C(2.0) * PI * bin * (real_t)index)
                               / (real_t)WINDOW);
    }
}

void test_slide_refuses_a_size_below_two(void)
{
    TEST_ASSERT_FALSE(slide_is_valid_size(0u));
    TEST_ASSERT_FALSE(slide_is_valid_size(1u));
    TEST_ASSERT_TRUE(slide_is_valid_size(2u));
    TEST_ASSERT_TRUE(slide_is_valid_size(WINDOW));
}

void test_slide_refuses_a_damping_outside_its_range(void)
{
    TEST_ASSERT_FALSE(slide_is_valid_damping(REAL_C(0.0)));
    TEST_ASSERT_FALSE(slide_is_valid_damping(REAL_C(-0.5)));
    TEST_ASSERT_FALSE(slide_is_valid_damping(REAL_C(1.001)));
    TEST_ASSERT_TRUE(slide_is_valid_damping(REAL_C(1.0)));
    TEST_ASSERT_TRUE(slide_is_valid_damping(SLIDE_DAMPING));
}

// THE ONE THAT MATTERS. With the damping switched off, the running total must
// be what the transform of the same window gives, because it is the same sum
// worked out another way.
void test_slide_undamped_agrees_with_the_transform_of_the_same_window(void)
{
    static const uint32_t WATCHED = 7u;

    real_t signal[WINDOW * 8u];
    cnum_t spectrum[WINDOW];

    fill_tone(signal, WINDOW * 8u, REAL_C(7.0));

    slide_t slide = slide_alloc(WINDOW, 1u);
    fft_t fft = fft_alloc(WINDOW);

    TEST_ASSERT_TRUE(slide_design(&slide, REAL_C(1.0)));
    TEST_ASSERT_TRUE(slide_watch(&slide, 0u, WATCHED));

    slide_process_block(&slide, signal, WINDOW * 8u);

    // The last WINDOW samples are exactly the window the watcher now holds.
    fft_forward_real(&fft, &signal[(WINDOW * 8u) - WINDOW], spectrum);

    real_t by_transform = cnum_magnitude(spectrum[WATCHED]);
    real_t by_sliding = slide_magnitude(&slide, 0u);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE * by_transform, by_transform,
                            by_sliding);

    slide_free(&slide);
    fft_free(&fft);
}

// A bin that holds nothing must read as nothing, which is the half that says
// the watcher is not simply reporting the loudness of the window.
void test_slide_a_bin_with_nothing_in_it_reads_as_nothing(void)
{
    real_t signal[WINDOW * 8u];

    fill_tone(signal, WINDOW * 8u, REAL_C(7.0));

    slide_t slide = slide_alloc(WINDOW, 2u);

    TEST_ASSERT_TRUE(slide_design(&slide, REAL_C(1.0)));
    TEST_ASSERT_TRUE(slide_watch(&slide, 0u, 7u));
    TEST_ASSERT_TRUE(slide_watch(&slide, 1u, 20u));

    slide_process_block(&slide, signal, WINDOW * 8u);

    real_t on_the_tone = slide_magnitude(&slide, 0u);
    real_t elsewhere = slide_magnitude(&slide, 1u);

    TEST_ASSERT_TRUE(on_the_tone > (REAL_C(100.0) * elsewhere));

    slide_free(&slide);
}

// The answer must not depend on how many samples came before the window that
// is in hand: the same window gives the same answer whenever it arrives.
void test_slide_the_same_window_gives_the_same_answer_whenever_it_arrives(void)
{
    real_t signal[WINDOW * 12u];

    fill_tone(signal, WINDOW * 12u, REAL_C(5.0));

    slide_t early = slide_alloc(WINDOW, 1u);
    slide_t late = slide_alloc(WINDOW, 1u);

    (void)slide_design(&early, REAL_C(1.0));
    (void)slide_design(&late, REAL_C(1.0));
    (void)slide_watch(&early, 0u, 5u);
    (void)slide_watch(&late, 0u, 5u);

    // The tone repeats every WINDOW samples, thus these two hold the same
    // window even though one has seen three times as many samples.
    slide_process_block(&early, signal, WINDOW * 4u);
    slide_process_block(&late, signal, WINDOW * 12u);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05) * slide_magnitude(&early, 0u),
                            slide_magnitude(&early, 0u),
                            slide_magnitude(&late, 0u));

    slide_free(&early);
    slide_free(&late);
}

void test_slide_says_when_its_window_is_full(void)
{
    slide_t slide = slide_alloc(WINDOW, 1u);

    (void)slide_watch(&slide, 0u, 3u);

    for(uint32_t index = 0; index < WINDOW - 1u; index++)
    {
        slide_process_sample(&slide, REAL_C(1.0));
        TEST_ASSERT_FALSE(slide_is_full(&slide));
    }

    slide_process_sample(&slide, REAL_C(1.0));
    TEST_ASSERT_TRUE(slide_is_full(&slide));

    slide_reset(&slide);
    TEST_ASSERT_FALSE(slide_is_full(&slide));

    slide_free(&slide);
}

void test_slide_refuses_a_watcher_or_a_bin_it_does_not_hold(void)
{
    slide_t slide = slide_alloc(WINDOW, 2u);

    TEST_ASSERT_TRUE(slide_watch(&slide, 0u, 0u));
    TEST_ASSERT_TRUE(slide_watch(&slide, 1u, WINDOW - 1u));
    TEST_ASSERT_FALSE(slide_watch(&slide, 2u, 0u));
    TEST_ASSERT_FALSE(slide_watch(&slide, 0u, WINDOW));

    // Asking a watcher it does not hold gives nothing rather than reaching
    // past the end of the list.
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), slide_magnitude(&slide, 5u));

    slide_free(&slide);
}

void test_slide_bin_frequency_runs_from_nothing_to_the_sample_rate(void)
{
    slide_t slide = slide_alloc(WINDOW, 1u);

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           slide_bin_frequency(&slide, 0u, REAL_C(8000.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(4000.0),
                            slide_bin_frequency(&slide, WINDOW / 2u,
                                                REAL_C(8000.0)));

    slide_free(&slide);
}

// A watcher on the caller's memory must answer as one from the heap does.
void test_slide_on_the_caller_s_memory_answers_the_same(void)
{
    real_t signal[WINDOW * 6u];
    real_t window[SLIDE_WINDOW_COUNT(WINDOW)];
    cnum_t total[SLIDE_BIN_COUNT(2u)];
    cnum_t turn[SLIDE_TURN_COUNT(2u)];

    fill_tone(signal, WINDOW * 6u, REAL_C(9.0));

    slide_t heap = slide_alloc(WINDOW, 2u);
    slide_t given = slide_static_alloc(WINDOW, 2u, window, total, turn);

    TEST_ASSERT_TRUE(heap.dynamic_alloc);
    TEST_ASSERT_FALSE(given.dynamic_alloc);

    (void)slide_watch(&heap, 0u, 9u);
    (void)slide_watch(&given, 0u, 9u);

    slide_process_block(&heap, signal, WINDOW * 6u);
    slide_process_block(&given, signal, WINDOW * 6u);

    TEST_ASSERT_EQUAL_REAL(slide_magnitude(&heap, 0u),
                           slide_magnitude(&given, 0u));

    slide_free(&heap);
    slide_free(&given);
}

// A design that is refused must change nothing. A module that refused and half
// applied the change would leave a watcher that neither the caller nor the
// module could describe.
void test_slide_a_refused_damping_leaves_the_watcher_as_it_was(void)
{
    slide_t slide = slide_alloc(WINDOW, 1u);

    TEST_ASSERT_TRUE(slide_design(&slide, REAL_C(0.5)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.5), slide.damping);

    TEST_ASSERT_FALSE(slide_design(&slide, REAL_C(0.0)));
    TEST_ASSERT_FALSE(slide_design(&slide, REAL_C(1.5)));
    TEST_ASSERT_FALSE(slide_design(&slide, REAL_C(-1.0)));

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.5), slide.damping);

    slide_free(&slide);
}
