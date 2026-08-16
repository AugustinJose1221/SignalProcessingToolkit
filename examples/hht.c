// Watch a motor start up, through the vibration of its housing.
//
// An accelerometer on the housing of a motor reads the shaking. While the
// motor runs at a fixed speed, that shaking holds one frequency, which is the
// speed of turning. While the motor starts up, the frequency rises.
//
// This is where a Fourier transform answers badly. It would give a wide band
// of frequencies, because the signal held every one of them at some moment,
// and it would say nothing about which moment. To watch a start up, or to find
// the moment when a bearing begins to fail, you need the frequency through the
// time.
//
// This transform gives that in two steps. The empirical mode decomposition
// splits the vibration into parts that each hold one frequency at a time. The
// Hilbert transform then reads the frequency of each part at each moment.
//
// TO PORT THIS: replace read_accelerometer with a read from your own sensor,
// and set SAMPLE_RATE to its rate. The size must be a power of two, because
// the Hilbert transform uses the fast Fourier transform.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_HHT_EXAMPLE)

#include <sptk/transform/hht.h>
#include <sptk/transform/hilbert.h>
#include <sptk/transform/fft.h>
#include <sptk/decompose/emd.h>
#include <sptk/decompose/imf.h>
#include <sptk/linalg/cnum.h>
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

// ---------------------------------------------------------------------------
// Replace this function with a read from your own sensor.
//
// It stands for a motor that turns at 10 turns in a second at the start and
// reaches about 40 by the end, thus its vibration rises with it.
// ---------------------------------------------------------------------------
static void read_accelerometer(void)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        float time = (float)index / SAMPLE_RATE;

        x[index] = (float)index;
        y[index] = cosf(2.0f*PI*((10.0f*time) + (15.0f*time*time)));
    }
}

int main(void)
{
    imf_t imf[NUMBER_OF_IMF];

    read_accelerometer();

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

    printf("The speed of the motor through the time, from the fastest part of\n");
    printf("the vibration:\n\n");
    printf("%10s %14s %12s\n", "TIME [s]", "SPEED [rev/s]", "STRENGTH");

    for(uint32_t index = 32; index < (SIZE - 32); index += 32)
    {
        printf("%10.3f %12.1f %12.2f\n", (float)index/SAMPLE_RATE,
               frequency[index], amplitude[index]);
    }

    printf("\nThe speed rises through the block, which is what a motor does\n");
    printf("while it starts up. The mean over the whole block is %.1f turns\n",
           hht_mean_frequency(amplitude, frequency, SIZE));
    printf("in a second, and that single number hides the whole start up.\n");
    printf("\nThe strength stays almost the same, thus nothing knocks. A bearing\n");
    printf("that begins to fail would show itself as a rise in the strength of\n");
    printf("one part while its frequency stays where it is.\n");

    fft_free(&fft);
    emd_free(emd);
    for(uint32_t which = 0; which < NUMBER_OF_IMF; which++)
    {
        imf_free(imf[which]);
    }

    return 0;
}

#endif
