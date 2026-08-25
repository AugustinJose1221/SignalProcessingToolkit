#include "unity.h"
#include "real_assert.h"
#include "stft.h"
#include "cnum.h"
#include "fft.h"
#include "window.h"
#include <math.h>

#define BLOCK       64u
#define SIZE        512u

static real_t signal_buffer[SIZE];
static real_t rebuilt[SIZE];
static real_t weight[SIZE];
static cnum_t frames[SIZE * (BLOCK / 2u + 1u)];

void setUp(void)
{

}

void tearDown(void)
{

}

static void build_a_signal(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        // A level of 1 is there so that the first sample is not zero by
        // chance, which the test on the ends of the signal depends on.
        signal_buffer[index] = REAL_C(1.0)
                               + REAL_SIN(REAL_C(2.0) * REAL_PI * REAL_C(7.0)
                                          * (real_t)index / (real_t)BLOCK)
                               + (REAL_C(0.3) * (real_t)((index % 5u)));
    }
}

void test_stft_is_valid_block_and_hop(void)
{
    TEST_ASSERT_EQUAL(true, stft_is_valid_block(256));
    TEST_ASSERT_EQUAL(false, stft_is_valid_block(300));

    TEST_ASSERT_EQUAL(true, stft_is_valid_hop(256, 128));
    TEST_ASSERT_EQUAL(true, stft_is_valid_hop(256, 1));

    // A hop longer than the block would step over samples and never look at
    // them at all.
    TEST_ASSERT_EQUAL(true, stft_is_valid_hop(256, 256));
    TEST_ASSERT_EQUAL(false, stft_is_valid_hop(256, 257));
    TEST_ASSERT_EQUAL(false, stft_is_valid_hop(256, 0));
}

void test_stft_frame_count_takes_whole_blocks_only(void)
{
    // 512 samples, blocks of 64, a hop of 32: the first block starts at 0 and
    // the last one that fits whole starts at 448.
    TEST_ASSERT_EQUAL(15, stft_frame_count(512, 64, 32));
    TEST_ASSERT_EQUAL(8, stft_frame_count(512, 64, 64));

    // Exactly one block gives exactly one frame.
    TEST_ASSERT_EQUAL(1, stft_frame_count(64, 64, 32));

    // Anything shorter gives none. The samples are not thrown away quietly;
    // the count says there is nothing to transform.
    TEST_ASSERT_EQUAL(0, stft_frame_count(63, 64, 32));
}

void test_stft_signal_size(void)
{
    TEST_ASSERT_EQUAL(512, stft_signal_size(15, 64, 32));
    TEST_ASSERT_EQUAL(64, stft_signal_size(1, 64, 32));
    TEST_ASSERT_EQUAL(0, stft_signal_size(0, 64, 32));
}

void test_stft_bin_count(void)
{
    // Half the block and one more, because the signal is real and the upper
    // half of the transform is the mirror of the lower.
    TEST_ASSERT_EQUAL(33, STFT_BIN_COUNT(64));
    TEST_ASSERT_EQUAL(129, STFT_BIN_COUNT(256));
}

void test_stft_puts_a_tone_where_it_belongs_in_time_and_in_frequency(void)
{
    // The whole reason the module exists. A tone that is there for the first
    // half of a recording and gone for the second must show in the frames of
    // the first half and not in the frames of the second, which one transform
    // of the whole recording could never say.
    stft_t stft = stft_alloc(BLOCK);
    const uint32_t bins = STFT_BIN_COUNT(BLOCK);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        signal_buffer[index] = (index < (SIZE / 2u))
                               ? REAL_SIN(REAL_C(2.0) * REAL_PI * REAL_C(8.0)
                                          * (real_t)index / (real_t)BLOCK)
                               : REAL_C(0.0);
    }

    TEST_ASSERT_EQUAL(true, stft_design(&stft, BLOCK / 2u, WINDOW_HANN,
                                        REAL_C(0.0)));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);

    TEST_ASSERT_EQUAL(true, stft_forward(&stft, signal_buffer, SIZE, frames,
                                         count * bins));

    // Bin 8 holds the tone. An early frame must have it and a late one must
    // not.
    real_t early = cnum_magnitude(frames[(2u * bins) + 8u]);
    real_t late = cnum_magnitude(frames[((count - 2u) * bins) + 8u]);

    TEST_ASSERT_TRUE(early > REAL_C(1.0));
    TEST_ASSERT_TRUE(late < (early / REAL_C(100.0)));

    stft_free(&stft);
}

void test_stft_round_trip_is_exact_inside_the_solid_stretch(void)
{
    stft_t stft = stft_alloc(BLOCK);
    const uint32_t bins = STFT_BIN_COUNT(BLOCK);
    uint32_t first;
    uint32_t solid;

    build_a_signal();

    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));
    TEST_ASSERT_EQUAL(true, stft_can_rebuild(&stft));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);
    stft_forward(&stft, signal_buffer, SIZE, frames, count * bins);

    uint32_t size = stft_signal_size(count, BLOCK, BLOCK / 2u);

    TEST_ASSERT_EQUAL(true, stft_inverse(&stft, frames, count, rebuilt, size,
                                         weight));
    TEST_ASSERT_EQUAL(true, stft_solid_range(&stft, count, &first, &solid));

    for(uint32_t index = first; index < (first + solid); index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), signal_buffer[index],
                                rebuilt[index]);
    }

    stft_free(&stft);
}

void test_stft_sets_the_ends_to_nothing_rather_than_to_a_wrong_answer(void)
{
    // The trap that catches everyone. The first sample of the signal is under
    // the first block only, and a hann window is zero there, thus that sample
    // cannot come back. The module writes nothing rather than a number that
    // looks like an answer.
    stft_t stft = stft_alloc(BLOCK);
    const uint32_t bins = STFT_BIN_COUNT(BLOCK);
    uint32_t first;
    uint32_t solid;

    build_a_signal();

    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);
    stft_forward(&stft, signal_buffer, SIZE, frames, count * bins);
    uint32_t size = stft_signal_size(count, BLOCK, BLOCK / 2u);
    stft_inverse(&stft, frames, count, rebuilt, size, weight);
    stft_solid_range(&stft, count, &first, &solid);

    // One block less one hop at the start, which for a hop of half the block
    // is half a block.
    TEST_ASSERT_EQUAL(BLOCK / 2u, first);
    TEST_ASSERT_TRUE(solid > REAL_C(0.0));

    for(uint32_t index = 0; index < first; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), rebuilt[index]);
    }

    for(uint32_t index = first + solid; index < size; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), rebuilt[index]);
    }

    // And the signal really was not nothing there, thus this is a loss and not
    // a signal that happened to be quiet.
    TEST_ASSERT_TRUE(REAL_ABS(signal_buffer[0]) > REAL_C(0.0));

    stft_free(&stft);
}

void test_stft_can_rebuild_refuses_a_window_and_hop_that_lose_samples(void)
{
    // A hann window at a hop of the whole block leaves the first sample of
    // every block multiplied by zero. No arithmetic brings those back, thus
    // the module says no before any work is done.
    stft_t stft = stft_alloc(BLOCK);

    stft_design(&stft, BLOCK, WINDOW_HANN, REAL_C(0.0));
    TEST_ASSERT_EQUAL(false, stft_can_rebuild(&stft));

    // The same window at half the block covers everything.
    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));
    TEST_ASSERT_EQUAL(true, stft_can_rebuild(&stft));

    // A rectangular window loses nothing at any hop, because it takes nothing
    // away in the first place.
    stft_design(&stft, BLOCK, WINDOW_RECTANGULAR, REAL_C(0.0));
    TEST_ASSERT_EQUAL(true, stft_can_rebuild(&stft));

    stft_free(&stft);
}

void test_stft_inverse_refuses_where_it_cannot_rebuild(void)
{
    stft_t stft = stft_alloc(BLOCK);
    const uint32_t bins = STFT_BIN_COUNT(BLOCK);

    build_a_signal();

    stft_design(&stft, BLOCK, WINDOW_HANN, REAL_C(0.0));

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK);
    stft_forward(&stft, signal_buffer, SIZE, frames, count * bins);

    TEST_ASSERT_EQUAL(false, stft_inverse(&stft, frames, count, rebuilt, SIZE,
                                          weight));

    stft_free(&stft);
}

void test_stft_refuses_before_it_is_designed(void)
{
    stft_t stft = stft_alloc(BLOCK);
    uint32_t first;
    uint32_t solid;

    build_a_signal();

    TEST_ASSERT_EQUAL(false, stft_can_rebuild(&stft));
    TEST_ASSERT_EQUAL(false, stft_forward(&stft, signal_buffer, SIZE, frames,
                                          SIZE * STFT_BIN_COUNT(BLOCK)));
    TEST_ASSERT_EQUAL(false, stft_solid_range(&stft, 4u, &first, &solid));

    stft_free(&stft);
}

void test_stft_refuses_a_signal_shorter_than_one_block_and_room_too_small(void)
{
    stft_t stft = stft_alloc(BLOCK);

    build_a_signal();
    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    TEST_ASSERT_EQUAL(false, stft_forward(&stft, signal_buffer, BLOCK - 1u,
                                          frames, SIZE));

    // Room for one bin less than the answer needs.
    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);
    TEST_ASSERT_EQUAL(false, stft_forward(&stft, signal_buffer, SIZE, frames,
                                          (count * STFT_BIN_COUNT(BLOCK))
                                          - 1u));

    stft_free(&stft);
}

void test_stft_bin_frequency_and_frame_time(void)
{
    stft_t stft = stft_alloc(BLOCK);

    stft_design(&stft, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    // A block of 64 at 640 samples in a second puts the bins 10 hertz apart.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(10.0),
                            stft_bin_frequency(&stft, 1, REAL_C(640.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(320.0),
                            stft_bin_frequency(&stft, 32, REAL_C(640.0)));

    // The time of a frame is the MIDDLE of its block, which for the first
    // frame is 31.5 samples in.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(31.5) / REAL_C(640.0),
                            stft_frame_time(&stft, 0, REAL_C(640.0)));

    // The second frame starts one hop later.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                            (REAL_C(32.0) + REAL_C(31.5)) / REAL_C(640.0),
                            stft_frame_time(&stft, 1, REAL_C(640.0)));

    stft_free(&stft);
}

void test_stft_static_alloc(void)
{
    static real_t window[BLOCK];
    static real_t windowed[BLOCK];
    static cnum_t spectrum[BLOCK];
    static cnum_t twiddle[BLOCK / 2u];
    static uint32_t reverse[BLOCK];

    fft_t fft = fft_static_alloc(BLOCK, twiddle, reverse);
    stft_t stft = stft_static_alloc(BLOCK, window, windowed, spectrum, fft);

    TEST_ASSERT_EQUAL(BLOCK, stft.block);
    TEST_ASSERT_EQUAL(true, stft_design(&stft, BLOCK / 2u, WINDOW_HANN,
                                        REAL_C(0.0)));
    TEST_ASSERT_EQUAL(true, stft_can_rebuild(&stft));

    build_a_signal();

    uint32_t count = stft_frame_count(SIZE, BLOCK, BLOCK / 2u);
    TEST_ASSERT_EQUAL(true, stft_forward(&stft, signal_buffer, SIZE, frames,
                                         count * STFT_BIN_COUNT(BLOCK)));

    stft_free(&stft);
}

void test_stft_fewest_frames(void)
{
    // A sample in the middle is under as many blocks as fit across it, thus
    // this is the block divided by the hop, rounded up.
    TEST_ASSERT_EQUAL(4, stft_fewest_frames(8, 2));
    TEST_ASSERT_EQUAL(2, stft_fewest_frames(8, 4));
    TEST_ASSERT_EQUAL(1, stft_fewest_frames(8, 8));
    TEST_ASSERT_EQUAL(8, stft_fewest_frames(8, 1));

    // A hop that does not divide the block still rounds up.
    TEST_ASSERT_EQUAL(3, stft_fewest_frames(8, 3));

    TEST_ASSERT_EQUAL(0, stft_fewest_frames(9, 3));
    TEST_ASSERT_EQUAL(0, stft_fewest_frames(8, 0));
}

void test_stft_fewest_frames_is_where_the_rebuild_becomes_possible(void)
{
    // The number it gives must be exactly where stft_solid_range starts to
    // answer, or it would be advice that does not match the module.
    stft_t stft = stft_alloc(BLOCK);
    uint32_t first;
    uint32_t solid;

    stft_design(&stft, BLOCK / 4u, WINDOW_HANN, REAL_C(0.0));

    uint32_t fewest = stft_fewest_frames(BLOCK, BLOCK / 4u);

    TEST_ASSERT_TRUE(fewest > 1u);
    TEST_ASSERT_EQUAL(false, stft_solid_range(&stft, fewest - 1u, &first,
                                              &solid));
    TEST_ASSERT_EQUAL(true, stft_solid_range(&stft, fewest, &first, &solid));

    stft_free(&stft);
}
