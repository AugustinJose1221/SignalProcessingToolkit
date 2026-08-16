// Measure a peak in a spectrometer reading.
//
// A spectrometer shines light through a sample and reads how much comes back
// at each wavelength. A substance in the sample shows itself as a peak, and
// two numbers about that peak carry the answer:
//
// - the HEIGHT says how much of the substance is there;
// - the PLACE says which substance it is.
//
// The reading holds noise from the detector. A plain mean of a window takes
// that noise away, but it also pulls the top of the peak down towards its
// neighbours, thus the height comes out too small and the answer is wrong.
//
// The filter of Savitzky and Golay lays a polynomial through the window
// instead. A polynomial can follow a peak, thus the height stays.
//
// The filter also gives the derivative of the reading. The derivative goes
// through zero exactly at the top of the peak, thus it names the place more
// finely than looking for the largest sample does.
//
// TO PORT THIS: replace read_spectrometer with a read from your own device.
// Choose the window from the width of your peaks: about the width of the
// narrowest peak that you must keep. A window that is too wide flattens it.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_SAVGOL_EXAMPLE)

#include <sptk/filter/savgol.h>
#include <math.h>
#include <stdio.h>

#define POINTS      61u     // One reading for each wavelength step
#define WINDOW      7u      // Must be odd, and not wider than a peak
#define ORDER       3u      // A polynomial of the third power

// The wavelength of the first point, and the step between two points.
#define FIRST_NM    400.0f
#define STEP_NM     5.0f

static float truth[POINTS];         // The peak with no noise
static float reading[POINTS];
static float smoothed[POINTS];
static float slope[POINTS];
static float plain_mean[POINTS];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own device.
//
// It stands for a sample that holds one substance, which shows a peak at
// 550 nanometres with the height 0.85 absorbance.
// ---------------------------------------------------------------------------
static void read_spectrometer(void)
{
    const float peak_at = 550.0f;
    const float peak_height = 0.85f;
    const float peak_width = 22.0f;

    for(uint32_t index = 0; index < POINTS; index++)
    {
        float wavelength = FIRST_NM + ((float)index * STEP_NM);
        float distance = (wavelength - peak_at) / peak_width;

        truth[index] = peak_height * expf(-distance*distance);

        float noise = 0.025f * sinf((float)index * 12.9898f)
                      * cosf((float)index * 78.233f);

        reading[index] = truth[index] + noise;
    }
}

static float wavelength_of(uint32_t index)
{
    return FIRST_NM + ((float)index * STEP_NM);
}

int main(void)
{
    read_spectrometer();

    // The filter that smooths.
    savgol_t smoother = savgol_alloc(WINDOW);
    if(!savgol_design(&smoother, ORDER, 0))
    {
        printf("The window and the order do not fit together.\n");
        return 1;
    }
    savgol_process_block(&smoother, reading, smoothed, POINTS);

    // The same window and order, but this one gives the first derivative.
    savgol_t derivative = savgol_alloc(WINDOW);
    savgol_design(&derivative, ORDER, 1);
    savgol_process_block(&derivative, reading, slope, POINTS);

    // A plain mean of the same window, for the comparison.
    for(uint32_t index = 0; index < POINTS; index++)
    {
        float sum = 0.0f;
        for(uint32_t tap = 0; tap < WINDOW; tap++)
        {
            int32_t position = (int32_t)index + (int32_t)tap - (int32_t)(WINDOW/2);
            if(position < 0)
            {
                position = 0;
            }
            if(position >= (int32_t)POINTS)
            {
                position = (int32_t)POINTS - 1;
            }
            sum += reading[position];
        }
        plain_mean[index] = sum / (float)WINDOW;
    }

    // Find the top of the peak in two steps.
    //
    // First, coarsely: the largest value of the smoothed reading. Looking for
    // the first place where the slope goes through zero would not do, because
    // noise near the flat ends makes the slope cross zero there as well.
    uint32_t top = 0;
    for(uint32_t index = 1; index < POINTS; index++)
    {
        if(smoothed[index] > smoothed[top])
        {
            top = index;
        }
    }

    // Then finely: the slope goes through zero at the true top, which usually
    // lies between two points. Draw a straight line between the slope on each
    // side of the top and find where that line crosses zero. This gives a
    // place finer than the step between two readings.
    float fine_top = wavelength_of(top);
    if((top > 0) && (top < (POINTS - 1)))
    {
        float before = slope[top - 1];
        float after = slope[top + 1];

        if((before - after) != 0.0f)
        {
            float part = before / (before - after);
            fine_top = wavelength_of(top - 1) + (part * 2.0f * STEP_NM);
        }
    }

    printf("A spectrometer reading, %u points from %.0f to %.0f nm\n",
           POINTS, FIRST_NM, wavelength_of(POINTS-1));
    printf("Window of %u points, polynomial of the order %u\n\n", WINDOW, ORDER);

    printf("%12s %10s %10s %10s %10s %10s\n",
           "WAVELENGTH", "TRUTH", "READING", "SAVGOL", "MEAN", "SLOPE");
    for(uint32_t index = 20; index <= 40; index += 2)
    {
        printf("%9.0f nm %10.3f %10.3f %10.3f %10.3f %10.4f%s\n",
               wavelength_of(index), truth[index], reading[index],
               smoothed[index], plain_mean[index], slope[index],
               (index == top) ? "  <- the top" : "");
    }

    printf("\nThe height of the peak:\n");
    printf("  the true height:     %6.3f\n", truth[30]);
    printf("  the filter:          %6.3f  (%+.1f%%)\n", smoothed[30],
           100.0f*(smoothed[30]-truth[30])/truth[30]);
    printf("  a plain mean:        %6.3f  (%+.1f%%)\n", plain_mean[30],
           100.0f*(plain_mean[30]-truth[30])/truth[30]);

    printf("\nThe place of the peak:\n");
    printf("  the largest reading: %.0f nm  (a step of %.0f nm)\n",
           wavelength_of(top), STEP_NM);
    printf("  where the slope crosses zero: %.1f nm\n", fine_top);
    printf("  the true place is:   550.0 nm\n");

    printf("\nThe plain mean pulls the top down, thus it would say that less of\n");
    printf("the substance is there than really is. The filter keeps the height.\n");

    savgol_free(&smoother);
    savgol_free(&derivative);

    return 0;
}

#endif
