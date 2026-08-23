// Log a vibration sensor at a rate that a memory card can hold.
//
// A sensor on a bearing is read at 32000 samples in a second, because the
// sensor answers that fast and a fault shows itself as a rattle high up in the
// band. Nothing on the machine turns faster than 200 Hz, thus the log needs
// 500 samples in a second and no more. That is 64 samples in for one out.
//
// KEEPING EVERY 64TH SAMPLE IS NOT THE ANSWER, AND THE REASON IS THE WHOLE
// POINT OF THIS EXAMPLE.
//
// A log at 500 samples in a second can hold nothing above 250 Hz. The rattle
// at 4000 Hz does not disappear when the samples are thrown away. It comes
// back at a frequency it never had, it lands inside the band that the log
// keeps, and from that moment it is part of the signal: it cannot be told from
// a real reading, and no later step can find out.
//
// This example shows the same signal put through both ways, and prints where
// the rattle ends up.
//
// TO PORT THIS: replace fill_block with a read from your own sensor. Set
// INPUT_RATE to its rate and FACTOR to the rate you want divided into it.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_RESAMPLE_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/filter/resample.h>
#include <sptk/transform/psd.h>
#include <sptk/transform/window.h>
#include <math.h>
#include <stdio.h>

#define INPUT_RATE      REAL_C(32000.0)
#define FACTOR          64u
#define OUTPUT_RATE     (INPUT_RATE / (real_t)FACTOR)
#define SAMPLES         65536u
#define BLOCK           256u
#define PI              REAL_C(3.14159265358979323846)

// What the machine really does: a shaft at 50 Hz that must be logged, and a
// rattle at 4100 Hz from a bearing that must not reach the log.
//
// 4100 is chosen to show the danger plainly. Divided by the rate of the log it
// leaves 100, thus a rattle at 4100 Hz arrives in the log as a tone at 100 Hz,
// which is a frequency the machine could really turn at.
#define SHAFT           REAL_C(50.0)
#define RATTLE          REAL_C(4100.0)
#define ALIAS           REAL_C(100.0)

static real_t input[SAMPLES];
static real_t careful[SAMPLES / FACTOR];
static real_t careless[SAMPLES / FACTOR];

// REPLACE THIS with a read from your own sensor.
static void fill_block(real_t* block, uint32_t size)
{
    for(uint32_t index = 0; index < size; index++)
    {
        real_t time = (real_t)index / INPUT_RATE;

        block[index] = REAL_SIN(REAL_C(2.0) * PI * SHAFT * time)
                       + (REAL_C(1.5) * REAL_SIN(REAL_C(2.0) * PI * RATTLE
                                                 * time));
    }
}

// How much power a log holds near the shaft and near where the rattle would
// land if it came back.
static void look_at_the_log(const char* what, const real_t* data, uint32_t size)
{
    psd_t psd = psd_alloc(BLOCK);
    real_t density[(BLOCK / 2u) + 1u];

    psd_estimate(&psd, data, size, OUTPUT_RATE, density);

    // The power in a narrow band around each frequency. A band and not a bin,
    // because a tone that does not sit exactly on a bin is seen in both of the
    // bins beside it.
    real_t at_shaft = psd_band_power(&psd, density, OUTPUT_RATE,
                                     SHAFT - REAL_C(10.0),
                                     SHAFT + REAL_C(10.0));
    real_t at_alias = psd_band_power(&psd, density, OUTPUT_RATE,
                                     ALIAS - REAL_C(10.0),
                                     ALIAS + REAL_C(10.0));

    printf("%s\n", what);
    printf("   power near %3.0f Hz, the shaft   : %8.4f\n", (real_t)SHAFT,
           at_shaft);
    printf("   power near %3.0f Hz, nothing real: %8.4f\n", (real_t)ALIAS,
           at_alias);

    psd_free(&psd);
}

int main(void)
{
    printf("A bearing sensor at %.0f samples in a second, logged at %.0f.\n",
           (real_t)INPUT_RATE, (real_t)OUTPUT_RATE);
    printf("The shaft turns at %.0f Hz and must be logged.\n", (real_t)SHAFT);
    printf("A bearing rattles at %.0f Hz and must not reach the log.\n\n",
           (real_t)RATTLE);

    fill_block(input, SAMPLES);

    // The careless way: keep every 64th sample and hope.
    uint32_t kept = 0;
    for(uint32_t index = 0; index < SAMPLES; index += FACTOR)
    {
        careless[kept] = input[index];
        kept++;
    }

    // The careful way: filter first, then keep. The module does both, so that
    // they cannot be separated by accident.
    uint32_t length = resample_advised_length(FACTOR);
    resample_t resample = resample_alloc_decimator(FACTOR, length);
    uint32_t made = resample_decimate_block(&resample, input, careful, SAMPLES);

    printf("The filter is worked out only for the samples that are kept,\n");
    printf("thus it costs %u multiplications for each sample that goes in\n",
           length / FACTOR);
    printf("and not %u.\n\n", length);

    look_at_the_log("Keeping every 64th sample and hoping:", careless, kept);
    printf("\n");
    look_at_the_log("Filtering first, then keeping:", careful, made);

    printf("\nThe rattle was never wanted in the log. What matters is that\n");
    printf("the careless way did not LOSE it: it MOVED it, to 100 Hz, where\n");
    printf("it looks exactly like a reading of a machine turning at 100 Hz.\n");
    printf("Nothing later in the chain can tell the two apart.\n\n");

    printf("A note on the cost. This filter holds %u coefficients because it\n",
           length);
    printf("must turn from passing to stopping inside one 64th of the band.\n");
    printf("Two stages of 8 would need about %u each, and the two together\n",
           resample_advised_length(8u));
    printf("cost far less than this one. The guide of sptk/filter says so.\n");

    resample_free(&resample);

    return 0;
}

#endif//RUN_EXAMPLE
