// Take noise out of a signal and keep its edges.
//
// The signal holds two steps and a noise. A low pass filter would take the
// noise away but would also make the two edges round. The wavelet transform
// keeps them, because a step gives few large values of the detail while the
// noise spreads over every value.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_DWT_EXAMPLE)

#include <sptk/transform/dwt.h>
#include <math.h>
#include <stdio.h>

#define SIZE        64u
#define LEVELS      3u
#define LIMIT       1.0f

static float clean[SIZE];
static float noisy[SIZE];
static float work[SIZE];

// A simple source of numbers that look like noise. It gives the same numbers
// at each run, thus two runs of the example give the same picture. The values
// lie between -0.6 and 0.6, which is well below the steps of the signal.
static float next_noise(uint32_t index)
{
    return 0.6f * sinf((float)index * 12.9898f) * cosf((float)index * 78.233f);
}

int main(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        if(index < 20)
        {
            clean[index] = 0.0f;
        }
        else if(index < 45)
        {
            clean[index] = 5.0f;
        }
        else
        {
            clean[index] = 2.0f;
        }

        noisy[index] = clean[index] + next_noise(index);
    }

    float before = 0.0f;
    for(uint32_t index = 0; index < SIZE; index++)
    {
        before += fabsf(noisy[index] - clean[index]);
    }

    dwt_t dwt = dwt_init(DWT_HAAR);

    // Take the transform, clear the small values of every detail, and take the
    // inverse transform. The approximation of the last level stays as it is,
    // because it holds the signal itself and not the noise.
    dwt_forward_multi(&dwt, noisy, SIZE, LEVELS, work);

    uint32_t approximation_size = SIZE >> LEVELS;
    dwt_threshold(&noisy[approximation_size], SIZE - approximation_size, LIMIT);

    dwt_inverse_multi(&dwt, noisy, SIZE, LEVELS, work);

    float after = 0.0f;
    for(uint32_t index = 0; index < SIZE; index++)
    {
        after += fabsf(noisy[index] - clean[index]);
    }

    printf("A signal with two steps and a noise, cleaned with the wavelet of Haar\n");
    printf("%u levels, and every detail below %.1f set to zero\n\n", LEVELS, LIMIT);

    printf("%6s %10s %10s\n", "SAMPLE", "CLEAN", "CLEANED");
    for(uint32_t index = 16; index < 52; index += 2)
    {
        printf("%6u %10.2f %10.2f%s\n", index, clean[index], noisy[index],
               ((index == 20) || (index == 44)) ? "   <- an edge" : "");
    }

    printf("\nThe distance from the clean signal, added over every sample:\n");
    printf("  before: %8.2f\n", before);
    printf("  after:  %8.2f\n", after);
    printf("\nThe two edges at the samples 20 and 45 are still sharp.\n");

    return 0;
}

#endif
