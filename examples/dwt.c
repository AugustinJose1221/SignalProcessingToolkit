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

#include <ffitt/core/real.h>
#include <ffitt/transform/dwt.h>
#include <math.h>
#include <stdio.h>

#define SIZE        64u     // A power of two, as the transform asks
#define LEVELS      3u
#define LIMIT       REAL_C(1.0)    // In grams, from the noise of the device

static real_t truth[SIZE];   // What the scale would read with no shaking
static real_t reading[SIZE];
static real_t work[SIZE];

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
            truth[index] = REAL_C(0.0);
        }
        else if(index < 44)
        {
            truth[index] = REAL_C(50.0);
        }
        else
        {
            truth[index] = REAL_C(80.0);
        }

        // The table shakes, thus the reading moves about half a gram.
        real_t shaking = REAL_C(0.6) * REAL_SIN((real_t)index * REAL_C(12.9898))
                        * REAL_COS((real_t)index * REAL_C(78.233));

        reading[index] = truth[index] + shaking;
    }
}

// Give how far a reading lies from the truth, added over every sample.
static real_t distance_from_the_truth(const real_t* signal)
{
    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        total += REAL_ABS(signal[index] - truth[index]);
    }

    return total;
}

int main(void)
{
    read_load_cell();

    real_t before = distance_from_the_truth(reading);

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

    real_t after = distance_from_the_truth(reading);

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
