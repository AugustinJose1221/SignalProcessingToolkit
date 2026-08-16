// Smooth a signal and keep the height of its peak.
//
// A peak carries its information in its height and its width. A plain mean of
// a window makes a peak lower and wider. The filter of Savitzky and Golay lays
// a polynomial through the window, thus it can follow a peak.
//
// The example also gives the first derivative of the signal, which says where
// the peak lies: the derivative goes through zero there.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_SAVGOL_EXAMPLE)

#include <sptk/filter/savgol.h>
#include <math.h>
#include <stdio.h>

#define SIZE        61u
#define WINDOW      11u
#define ORDER       3u

static float clean[SIZE];
static float noisy[SIZE];
static float smoothed[SIZE];
static float slope[SIZE];
static float mean[SIZE];

static float next_noise(uint32_t index)
{
    return 0.4f * sinf((float)index * 12.9898f) * cosf((float)index * 78.233f) * 4.0f;
}

int main(void)
{
    // A peak in the middle of the signal.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float position = ((float)index - 30.0f) / 8.0f;
        clean[index] = 10.0f * expf(-position*position);
        noisy[index] = clean[index] + next_noise(index);
    }

    // A plain mean of the same window, for the comparison.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float sum = 0.0f;
        for(uint32_t tap = 0; tap < WINDOW; tap++)
        {
            int32_t position = (int32_t)index + (int32_t)tap - (int32_t)(WINDOW/2);
            if(position < 0)
            {
                position = 0;
            }
            if(position >= (int32_t)SIZE)
            {
                position = (int32_t)SIZE - 1;
            }
            sum += noisy[position];
        }
        mean[index] = sum / (float)WINDOW;
    }

    savgol_t savgol = savgol_alloc(WINDOW);
    savgol_design(&savgol, ORDER, 0);
    savgol_process_block(&savgol, noisy, smoothed, SIZE);

    savgol_t derivative = savgol_alloc(WINDOW);
    savgol_design(&derivative, ORDER, 1);
    savgol_process_block(&derivative, noisy, slope, SIZE);

    printf("A peak with noise, smoothed with a window of %u and the order %u\n\n",
           WINDOW, ORDER);
    printf("%6s %9s %9s %9s %9s %9s\n",
           "SAMPLE", "CLEAN", "NOISY", "SAVGOL", "MEAN", "SLOPE");

    for(uint32_t index = 20; index <= 40; index += 2)
    {
        printf("%6u %9.2f %9.2f %9.2f %9.2f %9.3f\n", index,
               clean[index], noisy[index], smoothed[index], mean[index],
               slope[index]);
    }

    printf("\nThe height of the peak:\n");
    printf("  the clean signal:     %6.2f\n", clean[30]);
    printf("  the filter:           %6.2f\n", smoothed[30]);
    printf("  a plain mean:         %6.2f\n", mean[30]);
    printf("\nThe plain mean makes the peak lower. The filter keeps it.\n");
    printf("The slope goes through zero at the sample 30, where the peak lies.\n");

    savgol_free(&savgol);
    savgol_free(&derivative);

    return 0;
}

#endif
