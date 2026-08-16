// Find the frequencies that a signal holds.
//
// The signal holds two tones and a little noise. The transform shows both
// tones as peaks, and it says the frequency of each one in hertz.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FFT_EXAMPLE)

#include <fft/fft.h>
#include <cnum/cnum.h>
#include <math.h>
#include <stdio.h>

#define SIZE            256u
#define SAMPLE_RATE     1024.0f
#define PI              3.14159265358979323846f

int main(void)
{
    float signal[SIZE];
    cnum_t spectrum[SIZE];
    float magnitude[SIZE];

    // Two tones: 64 hertz with the amplitude 1, and 256 hertz with the
    // amplitude 0.5.
    //
    // One bin holds 4 hertz here, and both tones lie on a bin. A tone between
    // two bins gives its energy to many bins around it, which makes the peak
    // wide and lower. Choose the size of the window so that the tone that you
    // look for lies on a bin, or use a window function to make that spreading
    // smaller.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float time = (float)index / SAMPLE_RATE;
        signal[index] = sinf(2.0f*PI*64.0f*time)
                        + (0.5f*sinf(2.0f*PI*256.0f*time));
    }

    fft_t fft = fft_alloc(SIZE);

    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, SIZE);

    printf("The spectrum of a signal with two tones\n");
    printf("Sample rate %.0f hertz, %u points, thus one bin is %.2f hertz\n\n",
           SAMPLE_RATE, SIZE, SAMPLE_RATE/(float)SIZE);
    printf("%12s %12s %s\n", "FREQUENCY", "MAGNITUDE", "");

    // Only the bins below the middle hold new information. The bins above the
    // middle mirror them.
    for(uint32_t index = 0; index < (SIZE/2); index++)
    {
        if(magnitude[index] > 5.0f)
        {
            printf("%9.1f Hz %12.2f ", fft_bin_frequency(index, SIZE, SAMPLE_RATE),
                   magnitude[index]);
            for(uint32_t bar = 0; bar < (uint32_t)(magnitude[index]/4.0f); bar++)
            {
                printf("#");
            }
            printf("\n");
        }
    }

    fft_free(&fft);

    return 0;
}

#endif
