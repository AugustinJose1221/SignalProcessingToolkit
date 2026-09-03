// Measure the harmonics of the mains from a block that is not a power of two.
//
// A power quality analyser reports each harmonic of the mains as a share of
// the fundamental. The standard that governs it asks for the measurement over
// a whole number of cycles: ten cycles of a 50 hertz mains, which is 200
// milliseconds. At 5000 samples a second that is exactly 1000 samples.
//
// AND 1000 IS NOT A POWER OF TWO, which is the whole reason this module
// exists. The fast transform beside it takes 512 or 1024 and nothing between,
// and neither will serve:
//
//   CUT TO 512 and the block is no longer a whole number of cycles. The
//   fundamental then falls between two bins and its energy smears across the
//   whole spectrum, which is called leakage. Every harmonic is measured
//   against a fundamental that is now spread out, and every answer is wrong.
//
//   PAD TO 1024 with zeros and the same thing happens: 1024 samples at this
//   rate is 10.24 cycles, not 10.
//
//   WINDOW THE BLOCK to soften the leakage and the harmonics are softened with
//   it, which is measuring the window and not the mains.
//
// This module transforms the 1000 directly. Every harmonic falls exactly on a
// bin, thus each one is read from a single bin with nothing spread around it.
//
// WHAT IT COSTS. The method needs a transform larger than the block, which
// bluestein_transform_size gives, and it does three of them where the plain
// fast transform does one. That is the price of a size the other cannot take.
//
// TO PORT THIS: replace fill_block with a read from your own input. Set
// SAMPLE_RATE and MAINS_HZ, and BLOCK to a whole number of cycles at that
// rate: CYCLES * SAMPLE_RATE / MAINS_HZ must come out whole.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_BLUESTEIN_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/bluestein.h>

#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(5000.0)
#define MAINS_HZ        REAL_C(50.0)
#define CYCLES          10u
#define BLOCK           1000u        // CYCLES * SAMPLE_RATE / MAINS_HZ
#define HARMONICS       9u
#define PI              REAL_C(3.14159265358979323846)

static cnum_t data[BLOCK];

// The share of the fundamental that each harmonic really holds, which the
// example puts in so that the answer can be checked against it. A real
// analyser has no such list.
static const real_t TRUTH[HARMONICS] = {
    REAL_C(1.000), REAL_C(0.000), REAL_C(0.050), REAL_C(0.000),
    REAL_C(0.030), REAL_C(0.000), REAL_C(0.015), REAL_C(0.000),
    REAL_C(0.008)
};

// ---------------------------------------------------------------------------
// Replace this function with a read from your own input.
//
// It stands for a mains carrying the odd harmonics that a switching load makes:
// the third at 5 percent, the fifth at 3, the seventh at 1.5, the ninth at 0.8.
// ---------------------------------------------------------------------------
static void fill_block(cnum_t* into)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;
        real_t value = REAL_C(0.0);

        for(uint32_t harmonic = 0; harmonic < HARMONICS; harmonic++)
        {
            if(TRUTH[harmonic] > REAL_C(0.0))
            {
                real_t frequency = MAINS_HZ * (real_t)(harmonic + 1u);

                value += TRUTH[harmonic]
                         * REAL_SIN(REAL_C(2.0) * PI * frequency * time);
            }
        }

        into[index] = cnum_make(value, REAL_C(0.0));
    }
}

int main(void)
{
    if(!bluestein_is_valid_size(BLOCK))
    {
        printf("The module cannot take a block of %u.\n", BLOCK);
        return 1;
    }

    bluestein_t bluestein = bluestein_alloc(BLOCK);

    if(bluestein.size == 0u)
    {
        printf("There was not memory for a transform of %u.\n", BLOCK);
        return 1;
    }

    fill_block(data);
    bluestein_forward(&bluestein, data);

    printf("Ten cycles of a %.0f hertz mains at %.0f samples a second,\n",
           (double)MAINS_HZ, (double)SAMPLE_RATE);
    printf("which is %u samples. That is no power of two, and the fast\n", BLOCK);
    printf("transform beside this one cannot take it. This one works inside a\n");
    printf("transform of %u.\n\n", bluestein_transform_size(BLOCK));

    // The bin of the fundamental. Because the block holds a whole number of
    // cycles, this comes out exactly whole, and so does every harmonic's.
    uint32_t first = CYCLES;
    real_t fundamental = cnum_magnitude(data[first]);

    printf("%10s %10s %10s %10s\n", "HARMONIC", "AT Hz", "MEASURED", "TRUTH");
    printf("--------------------------------------------\n");

    for(uint32_t harmonic = 0; harmonic < HARMONICS; harmonic++)
    {
        uint32_t bin = first * (harmonic + 1u);
        real_t share = cnum_magnitude(data[bin]) / fundamental;

        printf("%10u %10.0f %10.4f %10.3f\n",
               harmonic + 1u,
               (double)bluestein_bin_frequency(bin, BLOCK, SAMPLE_RATE),
               (double)share, (double)TRUTH[harmonic]);
    }

    printf("\nEvery harmonic falls on a bin of its own, thus each is read from\n");
    printf("one bin with nothing spread around it. The harmonics that are not\n");
    printf("there measure as nothing, which is the test that matters: an\n");
    printf("analyser that invents a harmonic is worse than one that misses it.\n");

    bluestein_free(&bluestein);

    return 0;
}

#endif//RUN_EXAMPLE
