#include "unity.h"
#include "real_assert.h"
#include "goertzel.h"
#include <stdlib.h>
#include <math.h>

#define PI          REAL_C(3.14159265358979323846)
#define BLOCK       64u
#define RATE        REAL_C(8000.0)

void setUp(void)
{

}

void tearDown(void)
{

}

// Fill a block with a sine of the given frequency.
static void fill_tone(real_t* block, real_t frequency, real_t amplitude)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = amplitude * REAL_SIN((REAL_C(2.0)*PI*frequency*(real_t)index)/RATE);
    }
}

void test_goertzel_init(void)
{
    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, BLOCK);

    TEST_ASSERT_EQUAL(BLOCK, goertzel.block_size);
    TEST_ASSERT_EQUAL(0, goertzel.count);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), goertzel.first);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), goertzel.second);
}

void test_goertzel_is_block_complete(void)
{
    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, 4);

    TEST_ASSERT_EQUAL(false, goertzel_is_block_complete(&goertzel));
    for(uint32_t index = 0; index < 3; index++)
    {
        goertzel_process_sample(&goertzel, REAL_C(1.0));
    }
    TEST_ASSERT_EQUAL(false, goertzel_is_block_complete(&goertzel));

    goertzel_process_sample(&goertzel, REAL_C(1.0));
    TEST_ASSERT_EQUAL(true, goertzel_is_block_complete(&goertzel));
}

void test_goertzel_finds_the_tone_that_is_there(void)
{
    // The bin 8 of a block of 64 at 8000 hertz is 1000 hertz.
    real_t block[BLOCK];
    fill_tone(block, REAL_C(1000.0), REAL_C(1.0));

    goertzel_t present = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_process_block(&present, block, BLOCK);

    goertzel_t absent = goertzel_init(REAL_C(2000.0), RATE, BLOCK);
    goertzel_process_block(&absent, block, BLOCK);

    TEST_ASSERT_TRUE(goertzel_magnitude(&present) > (REAL_C(10.0) * goertzel_magnitude(&absent)));
}

void test_goertzel_gives_a_larger_answer_for_a_stronger_tone(void)
{
    real_t quiet[BLOCK];
    real_t loud[BLOCK];
    fill_tone(quiet, REAL_C(1000.0), REAL_C(1.0));
    fill_tone(loud, REAL_C(1000.0), REAL_C(3.0));

    goertzel_t first = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_t second = goertzel_init(REAL_C(1000.0), RATE, BLOCK);

    goertzel_process_block(&first, quiet, BLOCK);
    goertzel_process_block(&second, loud, BLOCK);

    // The answer grows with the amplitude, thus three times the amplitude
    // gives three times the answer.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.05), REAL_C(3.0),
                             goertzel_magnitude(&second) / goertzel_magnitude(&first));
}

void test_goertzel_gives_almost_nothing_for_a_signal_that_does_not_change(void)
{
    real_t block[BLOCK];
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = REAL_C(2.0);
    }

    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    TEST_ASSERT_TRUE(goertzel_magnitude(&goertzel) < REAL_C(0.001));
}

void test_goertzel_agrees_with_the_answer_of_a_transform(void)
{
    // For a tone that holds a whole number of turns inside the block, the
    // answer must be half the amplitude times the block size, which is what a
    // Fourier transform gives for one of its two peaks.
    real_t block[BLOCK];
    fill_tone(block, REAL_C(1000.0), REAL_C(1.0));

    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.5), (real_t)BLOCK/REAL_C(2.0), goertzel_magnitude(&goertzel));
}

void test_goertzel_magnitude_squared_is_the_square_of_the_magnitude(void)
{
    real_t block[BLOCK];
    fill_tone(block, REAL_C(1500.0), REAL_C(1.0));

    goertzel_t goertzel = goertzel_init(REAL_C(1500.0), RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    real_t magnitude = goertzel_magnitude(&goertzel);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), magnitude*magnitude,
                             goertzel_magnitude_squared(&goertzel));
}

void test_goertzel_phase_lies_between_minus_pi_and_pi(void)
{
    real_t block[BLOCK];
    fill_tone(block, REAL_C(1000.0), REAL_C(1.0));

    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    real_t phase = goertzel_phase(&goertzel);
    TEST_ASSERT_TRUE(phase >= -PI - REAL_C(0.001));
    TEST_ASSERT_TRUE(phase <= PI + REAL_C(0.001));
}

void test_goertzel_reset_clears_the_state(void)
{
    real_t block[BLOCK];
    fill_tone(block, REAL_C(1000.0), REAL_C(1.0));

    goertzel_t goertzel = goertzel_init(REAL_C(1000.0), RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);
    real_t first = goertzel_magnitude(&goertzel);

    goertzel_reset(&goertzel);
    TEST_ASSERT_EQUAL(0, goertzel.count);
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), goertzel.first);

    // The same block must give the same answer again.
    goertzel_process_block(&goertzel, block, BLOCK);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), first, goertzel_magnitude(&goertzel));
}

void test_goertzel_finds_one_tone_among_several(void)
{
    // A signal that holds three tones. A detector for each one must find its
    // own tone, and a detector for a fourth frequency must find little.
    real_t block[BLOCK];
    real_t tones[3] = {REAL_C(1000.0), REAL_C(2000.0), REAL_C(3000.0)};

    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = REAL_C(0.0);
        for(uint32_t which = 0; which < 3; which++)
        {
            block[index] += REAL_SIN((REAL_C(2.0)*PI*tones[which]*(real_t)index)/RATE);
        }
    }

    goertzel_t absent = goertzel_init(REAL_C(1500.0), RATE, BLOCK);
    goertzel_process_block(&absent, block, BLOCK);
    real_t quiet = goertzel_magnitude(&absent);

    for(uint32_t which = 0; which < 3; which++)
    {
        goertzel_t detector = goertzel_init(tones[which], RATE, BLOCK);
        goertzel_process_block(&detector, block, BLOCK);
        TEST_ASSERT_TRUE(goertzel_magnitude(&detector) > (REAL_C(5.0) * quiet));
    }
}
