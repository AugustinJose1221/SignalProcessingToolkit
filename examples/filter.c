// Take the noise out of a signal with two kinds of filter.
//
// The signal holds a slow wave and a fast noise. Both filters must keep the
// slow wave and take the fast noise away. The example prints how much of each
// frequency every filter lets through.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FILTER_EXAMPLE)

#include <fir/fir.h>
#include <iir/iir.h>
#include <math.h>
#include <stdio.h>

#define SIZE            200u
#define CUTOFF          0.05f
#define PI              3.14159265358979323846f

int main(void)
{
    float noisy[SIZE];
    float clean[SIZE];
    float wanted[SIZE];

    // The wanted signal turns 4 times over the window. The noise turns 60
    // times, thus a low pass filter can take it away.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        wanted[index] = sinf((2.0f*PI*4.0f*(float)index)/(float)SIZE);
        noisy[index] = wanted[index]
                       + (0.4f*sinf((2.0f*PI*60.0f*(float)index)/(float)SIZE));
    }

    fir_t fir = fir_alloc(41);
    fir_design_low_pass(&fir, CUTOFF);

    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, CUTOFF);

    printf("Two low pass filters with the cutoff %.2f of the sample rate\n\n",
           CUTOFF);
    printf("%12s %10s %10s\n", "FREQUENCY", "FIR", "IIR");
    for(float frequency = 0.0f; frequency < 0.5f; frequency += 0.05f)
    {
        printf("%12.2f %10.3f %10.3f\n", frequency,
               fir_get_gain(&fir, frequency), iir_get_gain(&iir, frequency));
    }

    // A filter with a finite impulse response needs many more coefficients
    // than a filter with an infinite one, but it moves every frequency by the
    // same time.
    printf("\nThe FIR filter holds %u coefficients.\n", fir.length);
    printf("The IIR filter holds %u sections, thus %u coefficients.\n\n",
           iir.sections, iir.sections * IIR_COEFFICIENT_COUNT);

    fir_process_block(&fir, noisy, clean, SIZE);

    // Measure how much noise is left. The filter moves the signal by half of
    // its length, thus the comparison must move with it.
    uint32_t delay = fir.length / 2;
    float before = 0.0f;
    float after = 0.0f;
    for(uint32_t index = delay; index < SIZE; index++)
    {
        before += fabsf(noisy[index] - wanted[index]);
        after += fabsf(clean[index] - wanted[index - delay]);
    }

    printf("The distance from the wanted signal, added over every sample:\n");
    printf("  before the filter: %8.2f\n", before);
    printf("  after the filter:  %8.2f\n", after);

    fir_free(&fir);
    iir_free(&iir);

    return 0;
}

#endif
