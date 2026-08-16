// Watch for a few known tones.
//
// A telephone keypad sends two tones for each key. This example takes a block
// of samples that holds the two tones of one key, and it asks each detector
// how much of its own tone the block holds. The detector with the largest
// answer in each group names the key.
//
// The algorithm holds three float values for each tone. A fast Fourier
// transform would need memory for the whole block and would give every
// frequency, where this program needs eight.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_GOERTZEL_EXAMPLE)

#include <sptk/transform/goertzel.h>
#include <math.h>
#include <stdio.h>

#define BLOCK           205u
#define SAMPLE_RATE     8000.0f
#define PI              3.14159265358979323846f

static const float ROW_TONES[4] = {697.0f, 770.0f, 852.0f, 941.0f};
static const float COLUMN_TONES[4] = {1209.0f, 1336.0f, 1477.0f, 1633.0f};
static const char KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// Give the index of the tone of the group that the block holds most of.
static uint32_t find_strongest(const float* block, const float* tones)
{
    uint32_t strongest = 0;
    float largest = -1.0f;

    for(uint32_t index = 0; index < 4; index++)
    {
        goertzel_t detector = goertzel_init(tones[index], SAMPLE_RATE, BLOCK);
        goertzel_process_block(&detector, block, BLOCK);

        float answer = goertzel_magnitude_squared(&detector);
        printf("  %6.0f Hz : %12.0f\n", tones[index], answer);

        if(answer > largest)
        {
            largest = answer;
            strongest = index;
        }
    }

    return strongest;
}

int main(void)
{
    float block[BLOCK];

    // The key 5 sends 770 hertz and 1336 hertz together.
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        float time = (float)index / SAMPLE_RATE;
        block[index] = sinf(2.0f*PI*770.0f*time) + sinf(2.0f*PI*1336.0f*time);
    }

    printf("A block of %u samples at %.0f hertz\n\n", BLOCK, SAMPLE_RATE);

    printf("The four tones of the rows:\n");
    uint32_t row = find_strongest(block, ROW_TONES);

    printf("\nThe four tones of the columns:\n");
    uint32_t column = find_strongest(block, COLUMN_TONES);

    printf("\nThe key is %c.\n", KEYS[row][column]);

    return 0;
}

#endif
