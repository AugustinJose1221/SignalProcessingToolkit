#include "unity.h"
#include "goertzel.h"
#include <stdlib.h>
#include <math.h>

#define PI          3.14159265358979323846f
#define BLOCK       64u
#define RATE        8000.0f

void setUp(void)
{

}

void tearDown(void)
{

}

// Fill a block with a sine of the given frequency.
static void fill_tone(float* block, float frequency, float amplitude)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = amplitude * sinf((2.0f*PI*frequency*(float)index)/RATE);
    }
}

void test_goertzel_init(void)
{
    goertzel_t goertzel = goertzel_init(1000.0f, RATE, BLOCK);

    TEST_ASSERT_EQUAL(BLOCK, goertzel.block_size);
    TEST_ASSERT_EQUAL(0, goertzel.count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, goertzel.first);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, goertzel.second);
}

void test_goertzel_is_block_complete(void)
{
    goertzel_t goertzel = goertzel_init(1000.0f, RATE, 4);

    TEST_ASSERT_EQUAL(false, goertzel_is_block_complete(&goertzel));
    for(uint32_t index = 0; index < 3; index++)
    {
        goertzel_process_sample(&goertzel, 1.0f);
    }
    TEST_ASSERT_EQUAL(false, goertzel_is_block_complete(&goertzel));

    goertzel_process_sample(&goertzel, 1.0f);
    TEST_ASSERT_EQUAL(true, goertzel_is_block_complete(&goertzel));
}

void test_goertzel_finds_the_tone_that_is_there(void)
{
    // The bin 8 of a block of 64 at 8000 hertz is 1000 hertz.
    float block[BLOCK];
    fill_tone(block, 1000.0f, 1.0f);

    goertzel_t present = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_process_block(&present, block, BLOCK);

    goertzel_t absent = goertzel_init(2000.0f, RATE, BLOCK);
    goertzel_process_block(&absent, block, BLOCK);

    TEST_ASSERT_TRUE(goertzel_magnitude(&present) > (10.0f * goertzel_magnitude(&absent)));
}

void test_goertzel_gives_a_larger_answer_for_a_stronger_tone(void)
{
    float quiet[BLOCK];
    float loud[BLOCK];
    fill_tone(quiet, 1000.0f, 1.0f);
    fill_tone(loud, 1000.0f, 3.0f);

    goertzel_t first = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_t second = goertzel_init(1000.0f, RATE, BLOCK);

    goertzel_process_block(&first, quiet, BLOCK);
    goertzel_process_block(&second, loud, BLOCK);

    // The answer grows with the amplitude, thus three times the amplitude
    // gives three times the answer.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f,
                             goertzel_magnitude(&second) / goertzel_magnitude(&first));
}

void test_goertzel_gives_almost_nothing_for_a_signal_that_does_not_change(void)
{
    float block[BLOCK];
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = 2.0f;
    }

    goertzel_t goertzel = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    TEST_ASSERT_TRUE(goertzel_magnitude(&goertzel) < 0.001f);
}

void test_goertzel_agrees_with_the_answer_of_a_transform(void)
{
    // For a tone that holds a whole number of turns inside the block, the
    // answer must be half the amplitude times the block size, which is what a
    // Fourier transform gives for one of its two peaks.
    float block[BLOCK];
    fill_tone(block, 1000.0f, 1.0f);

    goertzel_t goertzel = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    TEST_ASSERT_FLOAT_WITHIN(0.5f, (float)BLOCK/2.0f, goertzel_magnitude(&goertzel));
}

void test_goertzel_magnitude_squared_is_the_square_of_the_magnitude(void)
{
    float block[BLOCK];
    fill_tone(block, 1500.0f, 1.0f);

    goertzel_t goertzel = goertzel_init(1500.0f, RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    float magnitude = goertzel_magnitude(&goertzel);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, magnitude*magnitude,
                             goertzel_magnitude_squared(&goertzel));
}

void test_goertzel_phase_lies_between_minus_pi_and_pi(void)
{
    float block[BLOCK];
    fill_tone(block, 1000.0f, 1.0f);

    goertzel_t goertzel = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);

    float phase = goertzel_phase(&goertzel);
    TEST_ASSERT_TRUE(phase >= -PI - 0.001f);
    TEST_ASSERT_TRUE(phase <= PI + 0.001f);
}

void test_goertzel_reset_clears_the_state(void)
{
    float block[BLOCK];
    fill_tone(block, 1000.0f, 1.0f);

    goertzel_t goertzel = goertzel_init(1000.0f, RATE, BLOCK);
    goertzel_process_block(&goertzel, block, BLOCK);
    float first = goertzel_magnitude(&goertzel);

    goertzel_reset(&goertzel);
    TEST_ASSERT_EQUAL(0, goertzel.count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, goertzel.first);

    // The same block must give the same answer again.
    goertzel_process_block(&goertzel, block, BLOCK);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, first, goertzel_magnitude(&goertzel));
}

void test_goertzel_finds_one_tone_among_several(void)
{
    // A signal that holds three tones. A detector for each one must find its
    // own tone, and a detector for a fourth frequency must find little.
    float block[BLOCK];
    float tones[3] = {1000.0f, 2000.0f, 3000.0f};

    for(uint32_t index = 0; index < BLOCK; index++)
    {
        block[index] = 0.0f;
        for(uint32_t which = 0; which < 3; which++)
        {
            block[index] += sinf((2.0f*PI*tones[which]*(float)index)/RATE);
        }
    }

    goertzel_t absent = goertzel_init(1500.0f, RATE, BLOCK);
    goertzel_process_block(&absent, block, BLOCK);
    float quiet = goertzel_magnitude(&absent);

    for(uint32_t which = 0; which < 3; which++)
    {
        goertzel_t detector = goertzel_init(tones[which], RATE, BLOCK);
        goertzel_process_block(&detector, block, BLOCK);
        TEST_ASSERT_TRUE(goertzel_magnitude(&detector) > (5.0f * quiet));
    }
}
