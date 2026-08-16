// The Hilbert-Huang transform: which frequency at which time.
//
// The signal gets faster with the time. A Fourier transform would give a wide
// band of frequencies and would say nothing about the time. This transform
// takes the signal apart with the empirical mode decomposition and then reads
// the frequency at each point of time.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_HHT_EXAMPLE)

#include <hht/hht.h>
#include <hilbert/hilbert.h>
#include <fft/fft.h>
#include <emd/emd.h>
#include <imf/imf.h>
#include <cnum/cnum.h>
#include <math.h>
#include <stdio.h>

#define SIZE            256u
#define NUMBER_OF_IMF   4u
#define SAMPLE_RATE     256.0f
#define PI              3.14159265358979323846f

static float x[SIZE];
static float y[SIZE];
static float residue[SIZE];
static float working_buffer[SIZE];
static float peak_index_buffer[SIZE];
static float valley_index_buffer[SIZE];

static cnum_t work[SIZE];
static float amplitude[SIZE];
static float frequency[SIZE];

int main(void)
{
    imf_t imf[NUMBER_OF_IMF];

    // A signal whose frequency rises from about 10 hertz to about 40 hertz.
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float time = (float)index / SAMPLE_RATE;
        x[index] = (float)index;
        y[index] = cosf(2.0f*PI*((10.0f*time) + (15.0f*time*time)));
    }

    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf[which] = imf_alloc(SIZE);
    }

    emd_t emd = emd_alloc(SIZE);
    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer,
                   peak_index_buffer, valley_index_buffer);

    uint32_t count = emd_sift(&emd, 5);
    printf("The decomposition found %u intrinsic mode functions.\n\n", count);

    fft_t fft = fft_alloc(SIZE);

    // The first function holds the fastest part of the signal, which is the
    // part that gets faster.
    hht_transform_imf(&fft, &imf[0], work, amplitude, frequency, SAMPLE_RATE);

    printf("The frequency of the first function through the time:\n\n");
    printf("%10s %12s %12s\n", "TIME [s]", "FREQ [Hz]", "AMPLITUDE");

    for(uint32_t index = 32; index < (SIZE - 32); index += 32)
    {
        printf("%10.3f %12.1f %12.2f\n", (float)index/SAMPLE_RATE,
               frequency[index], amplitude[index]);
    }

    printf("\nThe frequency rises with the time, which is what the signal does.\n");
    printf("The mean frequency of the whole function is %.1f hertz.\n",
           hht_mean_frequency(amplitude, frequency, SIZE));

    fft_free(&fft);
    emd_free(emd);
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf_free(imf[which]);
    }

    return 0;
}

#endif
