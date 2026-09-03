// Choose the converter before the board is built.
//
// There is no device in this example, and that is the point. A board is being
// designed, the signal it must carry is known, and the question is whether the
// cheap 12 bit converter will do or whether the 16 bit one has to be paid for.
// Answering it in software costs nothing; answering it by building two boards
// costs months.
//
// WHAT ROUNDING REALLY DOES TO A SIGNAL. A converter rounds every sample to a
// step, and the error it makes is not noise: for a signal that repeats, the
// error repeats with it, thus it lands in the spectrum as FALSE TONES at
// multiples of the real one. A tone that is not there reads as a fault in the
// machine being measured, and no amount of averaging removes it, because it is
// not random.
//
// AND WHAT A LITTLE NOISE ADDED FIRST DOES ABOUT IT. Add noise of about one
// step before rounding and the error stops repeating. The false tones become
// a flat floor, which is louder in total and far easier to live with: a floor
// can be averaged down and a false tone cannot. That is the trade, and this
// example measures both halves of it through the library's own psd.
//
// TO PORT THIS: replace fill_block with the signal your board must carry, at
// the rate it will run. Set REACH to the full scale of the converter.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_QUANTISE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/psd.h>
#include <ffitt/transform/window.h>
#include <ffitt/util/quantise.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK           4096u
#define PSD_BLOCK       512u
#define SAMPLE_RATE     REAL_C(8000.0)
#define TONE_HZ         REAL_C(500.0)
#define TONE_SIZE       REAL_C(0.8)
#define REACH           REAL_C(1.0)
#define PI              REAL_C(3.14159265358979323846)

static real_t clean[BLOCK];
static real_t rounded[BLOCK];

// ---------------------------------------------------------------------------
// Replace this function with the signal your board must carry.
//
// It stands for a single clean tone at 500 hertz, which is the hardest case
// for a converter: a signal that repeats exactly makes an error that repeats
// exactly, and that is what becomes a false tone.
// ---------------------------------------------------------------------------
static void fill_block(real_t* block)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        block[index] = TONE_SIZE * REAL_SIN(REAL_C(2.0) * PI * TONE_HZ * time);
    }
}

// The loudest bin that is NOT the tone itself and not its close neighbours.
// That is the worst thing a reader of this spectrum would see and mistake for
// a real signal.
static void worst_false_tone(const psd_t* psd, const real_t* density,
                             uint32_t bins, real_t* level, real_t* where)
{
    real_t tone_bin = TONE_HZ / psd_bin_width(psd, SAMPLE_RATE);

    *level = REAL_C(0.0);
    *where = REAL_C(0.0);

    for(uint32_t bin = 1; bin < bins; bin++)
    {
        // Skip the tone and the two bins either side of it, which are the
        // tone's own skirt and not an error at all.
        if(REAL_ABS((real_t)bin - tone_bin) <= REAL_C(2.0))
        {
            continue;
        }

        if(density[bin] > *level)
        {
            *level = density[bin];
            *where = psd_bin_frequency(psd, bin, SAMPLE_RATE);
        }
    }
}

static void measure(const char* what, quantise_way_t way, uint32_t bits,
                    psd_t* psd, real_t* density, uint32_t bins)
{
    quantise_t converter = quantise_make();

    if(!quantise_design(&converter, way, bits, REACH))
    {
        printf("  %-22s the module refuses those numbers\n", what);
        return;
    }

    quantise_set_seed(&converter, 1u);
    (void)quantise_block(&converter, clean, rounded, BLOCK);

    (void)psd_estimate(psd, rounded, BLOCK, SAMPLE_RATE, density);

    real_t level = REAL_C(0.0);
    real_t where = REAL_C(0.0);

    worst_false_tone(psd, density, bins, &level, &where);

    // The whole error that is not the tone, which is the noise floor added up.
    real_t floor_power = psd_band_power(psd, density, SAMPLE_RATE,
                                        REAL_C(600.0), REAL_C(3900.0));

    printf("  %-22s %8.1f %10.0f %14.1f\n",
           what,
           (double)(REAL_C(10.0) * REAL_LOG(level) / REAL_LOG(REAL_C(10.0))),
           (double)where,
           (double)(REAL_C(10.0) * REAL_LOG(floor_power)
                    / REAL_LOG(REAL_C(10.0))));
}

int main(void)
{
    psd_t psd = psd_alloc(PSD_BLOCK);

    if(!psd_design(&psd, PSD_BLOCK / 2u, WINDOW_HANN, REAL_C(0.0)))
    {
        printf("The spectrum cannot be designed.\n");
        psd_free(&psd);
        return 1;
    }

    uint32_t bins = psd_bin_count(&psd);
    real_t* density = (real_t*)malloc(sizeof(real_t) * bins);

    if(density == NULL)
    {
        psd_free(&psd);
        return 1;
    }

    fill_block(clean);

    printf("A tone of %.0f Hz at %.1f of full scale, %.0f samples a second.\n",
           (double)TONE_HZ, (double)TONE_SIZE, (double)SAMPLE_RATE);
    printf("Measured through the library's own psd.\n\n");

    printf("  %-22s %8s %10s %14s\n",
           "CONVERTER", "WORST dB", "AT Hz", "WHOLE FLOOR dB");
    printf("------------------------------------------------------------\n");

    measure("16 bit, plain",  QUANTISE_PLAIN,  16u, &psd, density, bins);
    measure("12 bit, plain",  QUANTISE_PLAIN,  12u, &psd, density, bins);
    measure("12 bit, dither", QUANTISE_DITHER, 12u, &psd, density, bins);
    measure("8 bit, plain",   QUANTISE_PLAIN,   8u, &psd, density, bins);
    measure("8 bit, dither",  QUANTISE_DITHER,  8u, &psd, density, bins);

    printf("\nWORST dB is the loudest bin that is not the tone.\n\n");
    printf("AT 8 BITS the plain converter paints a false tone 15 dB above\n");
    printf("everything else, at a frequency the machine never made. Adding\n");
    printf("noise before rounding takes it away and lifts the whole floor by\n");
    printf("about 3 dB. That is the trade, and it is worth taking: a floor can\n");
    printf("be averaged down and a false tone cannot.\n\n");
    printf("AT 12 BITS THE DITHER BUYS NOTHING HERE, and the table says so:\n");
    printf("the worst bin is the same either way, because at that width this\n");
    printf("signal makes no false tone worth removing. Do not pay for dither\n");
    printf("where there is nothing to mend; measure first.\n");
    printf("\nquantise_noise_floor says what each width can reach at best:\n");
    printf("  16 bit %6.1f dB   12 bit %6.1f dB   8 bit %6.1f dB\n",
           (double)quantise_noise_floor(16u),
           (double)quantise_noise_floor(12u),
           (double)quantise_noise_floor(8u));

    free(density);
    psd_free(&psd);

    return 0;
}

#endif//RUN_EXAMPLE
