// Measure a vibration that sits on a baseline walking away underneath it.
//
// A strain gauge glued to a beam warms as current runs through it, and its
// zero walks while it warms. Over the first minutes that walk is far larger
// than the vibration riding on it. Ask stats_rms for the size of the vibration
// and it answers the size of the WALK, because the root mean square counts
// every part of the reading, and the walk is most of it.
//
// The walk is a straight line, or near enough over a block, and a straight
// line is exactly what detrend takes away. Take it away first, and the same
// call answers the vibration.
//
// TWO KINDS, AND THE HEADER SAYS TO TRY THE FIRST ONE FIRST. DETREND_CONSTANT
// takes away the mean, which is what a reading with an offset and no drift
// wants. DETREND_LINEAR takes away the best straight line, which is what a
// reading that DRIFTS wants. A constant taken from a drifting reading leaves
// the drift, and a line fitted to a reading that does not drift is fitting the
// noise. This one drifts, thus the line.
//
// THE DRIFT ITSELF IS A READING. detrend_trend gives the offset and the slope
// rather than taking them away, and the slope says how fast the gauge is still
// warming, thus when it is safe to trust a measurement at all.
//
// AND detrend_remove IS FOR WHEN THE DRIFT IS ALREADY KNOWN. Measure the slope
// once while the gauge warms, then take that same slope away from every block
// that follows without measuring it again.
//
// TO PORT THIS: replace fill_block with a read from your own gauge. The rate
// matters only for turning the slope into a drift for each second.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_DETREND_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/detrend.h>
#include <ffitt/util/stats.h>

#include <math.h>
#include <stdio.h>

#define BLOCK           512u
#define SAMPLE_RATE     REAL_C(100.0)
#define VIBRATION_HZ    REAL_C(12.0)
#define VIBRATION_SIZE  REAL_C(2.0)
#define DRIFT_PER_STEP  REAL_C(0.05)
#define OFFSET_AT_START REAL_C(100.0)
#define PI              REAL_C(3.14159265358979323846)

// ---------------------------------------------------------------------------
// Replace this function with a read from your own gauge.
//
// It stands for a gauge that reads 100 microstrain at rest, warms by 0.05 for
// each sample, and carries a vibration of 2 microstrain at 12 hertz.
// ---------------------------------------------------------------------------
static void fill_block(real_t* block)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        block[index] = OFFSET_AT_START
                       + (DRIFT_PER_STEP * (real_t)index)
                       + (VIBRATION_SIZE
                          * REAL_SIN(REAL_C(2.0) * PI * VIBRATION_HZ * time));
    }
}

int main(void)
{
    real_t block[BLOCK];
    real_t flat[BLOCK];
    real_t offset = REAL_C(0.0);
    real_t slope = REAL_C(0.0);

    fill_block(block);

    // The true size of a sine of this height, which is its height over the
    // root of two. Everything below is measured against it.
    real_t truth = VIBRATION_SIZE / REAL_SQRT(REAL_C(2.0));

    printf("A strain gauge, %u samples at %.0f a second.\n",
           BLOCK, (double)SAMPLE_RATE);
    printf("It carries a vibration of %.2f microstrain at %.0f hertz, and it\n",
           (double)VIBRATION_SIZE, (double)VIBRATION_HZ);
    printf("is warming. The vibration alone would measure %.3f.\n\n",
           (double)truth);

    // THE WRONG ANSWER FIRST, so that the right one can be set beside it.
    printf("  rms of the reading as it stands   %10.3f   <- the walk, not the\n",
           (double)stats_rms(block, BLOCK));
    printf("                                                 vibration\n");

    // Taking the MEAN away is not enough for a reading that drifts.
    (void)detrend_block(block, flat, BLOCK, DETREND_CONSTANT);
    printf("  rms with the mean taken away      %10.3f   <- the drift is\n",
           (double)stats_rms(flat, BLOCK));
    printf("                                                 still there\n");

    // The straight line is what a drifting reading wants.
    (void)detrend_block(block, flat, BLOCK, DETREND_LINEAR);
    printf("  rms with the line taken away      %10.3f   <- the vibration\n\n",
           (double)stats_rms(flat, BLOCK));

    // THE DRIFT ITSELF, WHICH IS A READING OF ITS OWN.
    (void)detrend_trend(block, BLOCK, DETREND_LINEAR, &offset, &slope);

    // THE OFFSET IS THE TREND AT THE MIDDLE OF THE BLOCK AND NOT AT ITS
    // START. The header says so. Read as a starting value it is wrong by half
    // the block's worth of drift, which here is more than twelve microstrain.
    printf("The line under the reading: %.2f at the MIDDLE of the block, and\n",
           (double)offset);
    printf("it climbs %.4f for each sample, thus %.2f where the block began.\n",
           (double)slope,
           (double)(offset - (slope * (real_t)(BLOCK / 2u))));
    printf("That is %.2f microstrain a second. The gauge is\n",
           (double)(slope * SAMPLE_RATE));
    printf("still warming, thus a measurement of the absolute strain now would\n");
    printf("be wrong however long it were averaged.\n\n");

    // AND ONCE THE DRIFT IS KNOWN, taking it away needs no measuring again.
    (void)detrend_remove(block, flat, BLOCK, offset, slope);
    printf("The same line taken away by detrend_remove: rms %.3f, which is\n",
           (double)stats_rms(flat, BLOCK));
    printf("what detrend_block worked out, without working it out again.\n");

    return 0;
}

#endif//RUN_EXAMPLE
