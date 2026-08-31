// Watch a pump for a bearing that is beginning to fail.
//
// A pump runs at 25 turns in a second. An accelerometer on its housing reads
// at 2000 samples in a second. A bearing that is wearing does not change how
// fast the pump turns; it adds a rattle high up in the band, and the rattle
// grows over weeks. The question is not "what does the vibration look like"
// but "how much energy is there in each band, and is the high band growing".
//
// WHY NOT ONE TRANSFORM OF THE WHOLE RECORDING
//
// A recording of 8192 samples transformed in one piece gives 4097 bins, and
// every one of them is as noisy as the last. Make the recording twice as long
// and there are twice as many bins, each still as noisy. The answer becomes
// finer and no more certain, which is the wrong trade for a measurement that
// must be compared against last week's.
//
// The method of Welch cuts the recording into overlapping blocks, transforms
// each, and takes the mean. Fewer bins, and each far steadier. This example
// prints both so that the difference can be seen.
//
// THE PART THAT IS USUALLY WRONG IS THE SCALING
//
// A density is power for each hertz. Its numbers must not change when the
// block, the window or the overlap changes, or two measurements taken with
// different settings cannot be compared at all. This example measures the same
// signal three ways and prints the band power each time.
//
// TO PORT THIS: replace fill_block with a read from your own accelerometer.
// Set SAMPLE_RATE to its rate and the two bands to what your machine does.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_PSD_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/psd.h>
#include <ffitt/transform/window.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(2000.0)
#define SAMPLES         8192u
#define PI              REAL_C(3.14159265358979323846)

#define SHAFT           REAL_C(25.0)
#define RATTLE          REAL_C(430.0)

static real_t reading[SAMPLES];
static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// REPLACE THIS with a read from your own accelerometer. The wear is 0 for a
// bearing that is sound and 1 for one that is failing.
static void fill_block(real_t* block, uint32_t size, real_t wear)
{
    seed = 1u;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        block[index] = REAL_SIN(REAL_C(2.0) * PI * SHAFT * time)
                       + (wear * REAL_C(0.3)
                          * REAL_SIN(REAL_C(2.0) * PI * RATTLE * time))
                       + (REAL_C(0.2) * rough());
    }
}

// The power in the band of the shaft and in the band of the rattle.
static void report(const char* what, psd_t* psd, const real_t* data)
{
    real_t density[513];

    psd_estimate(psd, data, SAMPLES, SAMPLE_RATE, density);

    real_t shaft = psd_band_power(psd, density, SAMPLE_RATE,
                                  REAL_C(15.0), REAL_C(35.0));
    real_t rattle = psd_band_power(psd, density, SAMPLE_RATE,
                                   REAL_C(400.0), REAL_C(460.0));

    printf("  %-34s shaft %7.4f   rattle %7.4f\n", what, shaft, rattle);
}

int main(void)
{
    printf("A pump at %.0f turns in a second, read at %.0f samples.\n\n",
           (real_t)SHAFT, (real_t)SAMPLE_RATE);

    // A block of 1024 gives bins about 2 Hz wide, and 8192 samples cut into
    // blocks of 1024 overlapping by half gives 15 of them.
    psd_t psd = psd_alloc(1024);
    printf("Blocks of %u, overlapping by %u, thus %u of them.\n",
           psd.block, psd.overlap, psd_block_count(&psd, SAMPLES));
    printf("Each bin is %.2f Hz wide.\n\n", psd_bin_width(&psd, SAMPLE_RATE));

    printf("A bearing that is sound, and one that is failing:\n");
    fill_block(reading, SAMPLES, REAL_C(0.0));
    report("sound", &psd, reading);
    fill_block(reading, SAMPLES, REAL_C(1.0));
    report("failing", &psd, reading);
    printf("\n  The shaft has not changed. The rattle has, and by enough to\n");
    printf("  set a threshold on.\n\n");

    // The same signal measured three ways. If the scaling were wrong these
    // three would not agree, and two measurements taken on different days with
    // different settings could not be compared.
    printf("The same failing bearing, measured three ways:\n");

    psd_design(&psd, 512, WINDOW_HANN, REAL_C(0.0));
    report("blocks of 1024, Hann, half over", &psd, reading);

    psd_design(&psd, 512, WINDOW_BLACKMAN, REAL_C(0.0));
    report("the same, but a Blackman window", &psd, reading);

    psd_free(&psd);
    psd_t small = psd_alloc(256);
    psd_design(&small, 192, WINDOW_HAMMING, REAL_C(0.0));
    report("blocks of 256, Hamming, 3/4 over", &small, reading);

    printf("\n  The three agree. That is what the scaling is for: a density is\n");
    printf("  power for each hertz, thus it must not follow the block, the\n");
    printf("  window or the overlap. Leave out any one of the three\n");
    printf("  corrections and these lines would disagree by a steady factor\n");
    printf("  that nobody would notice.\n");

    psd_free(&small);

    return 0;
}

#endif//RUN_EXAMPLE
