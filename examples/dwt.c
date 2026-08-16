// Read the weight from a scale that stands on a shaking table.
//
// A load cell gives a reading that is almost flat while nothing changes, and
// that steps up when an item goes onto the scale. The table shakes, thus the
// reading holds noise on top of those steps.
//
// A low pass filter would take the noise away, but it would also make each
// step round, thus the moment when the item arrived would become unclear and
// the weight would settle slowly.
//
// A wavelet transform keeps the step sharp. The reason: a step gives a few
// large values of the detail at the place where it stands, while the noise
// spreads a small value over every place. Clear the small values and the step
// stays.
//
// TO PORT THIS: replace read_load_cell with a read from your own scale. Set
// LIMIT from the noise of your own device: read it while nothing stands on the
// scale, and take about three times the size of what you see.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_DWT_EXAMPLE)

#include <sptk/transform/dwt.h>
#include <math.h>
#include <stdio.h>

#define SIZE        64u     // A power of two, as the transform asks
#define LEVELS      3u
#define LIMIT       1.0f    // In grams, from the noise of the device

static float truth[SIZE];   // What the scale would read with no shaking
static float reading[SIZE];
static float work[SIZE];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own scale.
//
// It stands for a scale that holds nothing, then takes an item of 50 grams,
// then a second item of 30 grams.
// ---------------------------------------------------------------------------
static void read_load_cell(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        if(index < 20)
        {
            truth[index] = 0.0f;
        }
        else if(index < 44)
        {
            truth[index] = 50.0f;
        }
        else
        {
            truth[index] = 80.0f;
        }

        // The table shakes, thus the reading moves about half a gram.
        float shaking = 0.6f * sinf((float)index * 12.9898f)
                        * cosf((float)index * 78.233f);

        reading[index] = truth[index] + shaking;
    }
}

// Give how far a reading lies from the truth, added over every sample.
static float distance_from_the_truth(const float* signal)
{
    float total = 0.0f;

    for(uint32_t index = 0; index < SIZE; index++)
    {
        total += fabsf(signal[index] - truth[index]);
    }

    return total;
}

int main(void)
{
    read_load_cell();

    float before = distance_from_the_truth(reading);

    dwt_t dwt = dwt_init(DWT_HAAR);

    // 1. Take the transform. After this call the front of the list holds the
    //    approximation of the last level, and the rest holds the details.
    dwt_forward_multi(&dwt, reading, SIZE, LEVELS, work);

    // 2. Clear the small values of every detail. Leave the approximation as it
    //    is: it holds the weight itself and not the shaking.
    uint32_t approximation_size = SIZE >> LEVELS;
    dwt_threshold(&reading[approximation_size], SIZE - approximation_size, LIMIT);

    // 3. Take the inverse transform.
    dwt_inverse_multi(&dwt, reading, SIZE, LEVELS, work);

    float after = distance_from_the_truth(reading);

    printf("A scale on a shaking table, %u readings\n", SIZE);
    printf("Two items arrive: 50 grams at the reading 20, 30 more at 44\n\n");

    printf("%8s %10s %10s %s\n", "READING", "TRUTH", "CLEANED", "");
    for(uint32_t index = 16; index < 52; index += 2)
    {
        const char* note = "";
        if(index == 20)
        {
            note = "  <- the first item arrives";
        }
        if(index == 44)
        {
            note = "  <- the second item arrives";
        }
        printf("%8u %10.1f %10.1f%s\n", index, truth[index], reading[index], note);
    }

    printf("\nHow far the reading lies from the truth, added over every sample:\n");
    printf("  before: %8.1f grams\n", before);
    printf("  after:  %8.1f grams\n", after);
    printf("\nBoth steps are still sharp: the weight is right on the very reading\n");
    printf("where the item arrives, thus the moment of arrival stays clear.\n");

    return 0;
}

#endif
