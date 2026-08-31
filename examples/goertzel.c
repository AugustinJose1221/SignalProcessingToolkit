// Decode a key press from an audio stream.
//
// A telephone keypad sends two tones together for each key: one from a group
// of four low tones, which says the row, and one from a group of four high
// tones, which says the column. A device that answers a call must find which
// key the caller pressed.
//
// The device knows the eight frequencies before it starts, thus it does not
// need the whole spectrum. It needs eight questions of the form "how much of
// this one frequency does this block hold". That is what this algorithm
// answers, and it holds three float values for each question.
//
// The cost matters here. A fast Fourier transform of a block of 205 samples
// needs memory for 205 complex numbers, which is 1640 bytes, and it gives 205
// answers of which the device wants eight. The eight detectors together hold
// under 300 bytes and give exactly the eight.
//
// TO PORT THIS: replace fill_block with a read from your own audio input. Set
// SAMPLE_RATE to its rate. A telephone line runs at 8000 samples in a second,
// and a block of 205 samples is the size that the standard asks for.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_GOERTZEL_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/goertzel.h>
#include <math.h>
#include <stdio.h>

#define BLOCK           205u
#define SAMPLE_RATE     REAL_C(8000.0)
#define PI              REAL_C(3.14159265358979323846)

static const real_t ROW_TONES[4] = {REAL_C(697.0), REAL_C(770.0), REAL_C(852.0), REAL_C(941.0)};
static const real_t COLUMN_TONES[4] = {REAL_C(1209.0), REAL_C(1336.0), REAL_C(1477.0), REAL_C(1633.0)};
static const char KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// Give the index of the tone of the group that the block holds most of.
static uint32_t find_strongest(const real_t* block, const real_t* tones)
{
    uint32_t strongest = 0;
    real_t largest = -REAL_C(1.0);

    for(uint32_t index = 0; index < 4; index++)
    {
        goertzel_t detector = goertzel_init(tones[index], SAMPLE_RATE, BLOCK);
        goertzel_process_block(&detector, block, BLOCK);

        real_t answer = goertzel_magnitude_squared(&detector);
        printf("  %6.0f Hz : %12.0f\n", tones[index], answer);

        if(answer > largest)
        {
            largest = answer;
            strongest = index;
        }
    }

    return strongest;
}

// ---------------------------------------------------------------------------
// Replace this function with a read from your own audio input.
//
// It stands for a caller who presses the key 5, which sends 770 hertz and
// 1336 hertz together, over a line that holds a little noise.
// ---------------------------------------------------------------------------
static void fill_block(real_t* block)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        block[index] = REAL_SIN(REAL_C(2.0)*PI*REAL_C(770.0)*time)
                       + REAL_SIN(REAL_C(2.0)*PI*REAL_C(1336.0)*time)
                       + (REAL_C(0.1) * REAL_SIN((real_t)index * REAL_C(12.9898))
                          * REAL_COS((real_t)index * REAL_C(78.233)));
    }
}

int main(void)
{
    real_t block[BLOCK];

    fill_block(block);

    printf("A block of %u samples at %.0f hertz, which is %.1f ms of audio\n\n",
           BLOCK, SAMPLE_RATE, (REAL_C(1000.0)*(real_t)BLOCK)/SAMPLE_RATE);

    printf("The four tones of the rows:\n");
    uint32_t row = find_strongest(block, ROW_TONES);

    printf("\nThe four tones of the columns:\n");
    uint32_t column = find_strongest(block, COLUMN_TONES);

    printf("\nThe key is %c.\n", KEYS[row][column]);

    printf("\nThe answer of the tone that is there stands far above the others,\n");
    printf("thus a simple comparison names the key. A real decoder also examines\n");
    printf("that the two answers are near each other in size, and that the rest\n");
    printf("stay small, so that speech does not look like a key press.\n");

    return 0;
}

#endif
