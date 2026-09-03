// Line up two recorders whose clocks do not quite agree.
//
// Two devices record the same event from different places: two microphones on
// a machine, two loggers on a pipeline, two receivers of one radio. Each has a
// crystal of its own, and no two crystals agree. One samples at 8000.0 a
// second and the other at 8000.3, and nothing about either is faulty.
//
// WHY A WHOLE SAMPLE SHIFT DOES NOT MEND IT. After a minute the two streams
// stand about two samples apart, and after ten minutes about twenty. Shifting
// by a whole number of samples takes out the whole part and leaves the
// FRACTION, and the fraction is what ruins the work that follows: a
// correlation between two streams half a sample apart reports a delay that is
// half a sample wrong, and a difference between them never cancels.
//
// A FRACTIONAL DELAY IS A FILTER. The sample that would have arrived a third
// of a sample earlier was never measured, thus it must be worked out from the
// ones either side. This module does that with a filter whose delay can be set
// to any fraction and changed at any moment, which is what a drifting clock
// asks for: the offset grows, and the filter follows it.
//
// MEASURE FIRST, THEN CORRECT. delay_by_phase gives the offset between two
// blocks to a fraction of a sample, and farrow_set_delay takes it out. The two
// modules are meant to be used together, and the example shows the residual
// after the correction, which is the number that says whether it worked.
//
// THE FILTER ADDS A WHOLE DELAY OF ITS OWN AND IT CANNOT NOT, which decides
// how the example is arranged. To work out a value between samples it must
// have the samples either side, thus it must wait for them. The delay it can
// apply runs from half its order to half its order plus one, and for an order
// of 3 that is 1.5 to 2.5 samples. A delay of 0.37 on its own is NOT in that
// range and the filter refuses it.
//
// The way round is to run BOTH streams through a filter: the early one delayed
// by 1.5 plus the offset, the late one by 1.5. Both delays are in range, both
// streams come out with the same whole delay, and the fraction between them is
// gone. The whole 1.5 samples that both now carry is a delay of the
// measurement and not an error in it.
//
// THE ORDER IS A TRADE. A higher order follows the shape between samples more
// closely, and it needs more samples of history, thus more delay of its own.
// Three is the usual choice.
//
// TO PORT THIS: replace fill_streams with reads from your own two devices.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FARROW_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/detect/delay.h>
#include <ffitt/filter/farrow.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/fft.h>

#include <math.h>
#include <stdio.h>

#define BLOCK           1024u
#define ORDER           3u
#define SAMPLE_RATE     REAL_C(8000.0)
#define TONE_HZ         REAL_C(300.0)
#define TRUE_OFFSET     REAL_C(0.37)
#define PI              REAL_C(3.14159265358979323846)

static real_t first[BLOCK];
static real_t second[BLOCK];
static real_t lined_up[BLOCK];
static real_t second_out[BLOCK];
static cnum_t first_work[BLOCK];
static cnum_t second_work[BLOCK];

// ---------------------------------------------------------------------------
// Replace this function with reads from your own two devices.
//
// It stands for two recorders of the same sound, the second one sampling
// 0.37 of a sample later than the first because its crystal has drifted.
// ---------------------------------------------------------------------------
static void fill_streams(real_t* a, real_t* b)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t at = (real_t)index;

        a[index] = REAL_SIN(REAL_C(2.0) * PI * TONE_HZ * at / SAMPLE_RATE)
                   + (REAL_C(0.4) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(1100.0)
                                             * at / SAMPLE_RATE));

        real_t later = at + TRUE_OFFSET;

        b[index] = REAL_SIN(REAL_C(2.0) * PI * TONE_HZ * later / SAMPLE_RATE)
                   + (REAL_C(0.4) * REAL_SIN(REAL_C(2.0) * PI * REAL_C(1100.0)
                                             * later / SAMPLE_RATE));
    }
}

// How far apart two streams stand, measured as what is left when one is taken
// from the other. Two streams that line up leave nothing.
static real_t what_is_left(const real_t* a, const real_t* b, uint32_t skip)
{
    real_t total = REAL_C(0.0);
    uint32_t counted = 0;

    for(uint32_t index = skip; index < BLOCK; index++)
    {
        real_t error = a[index] - b[index];

        total += error * error;
        counted++;
    }

    return REAL_SQRT(total / (real_t)counted);
}

int main(void)
{
    fft_t fft = fft_alloc(BLOCK);
    farrow_t early = farrow_alloc(ORDER);
    farrow_t late = farrow_alloc(ORDER);
    real_t measured = REAL_C(0.0);

    fill_streams(first, second);

    if(!delay_by_phase(first, second, BLOCK, &fft, first_work, second_work,
                       &measured))
    {
        printf("The offset could not be measured.\n");
        return 1;
    }

    printf("Two recorders of one sound, %u samples at %.0f a second.\n",
           BLOCK, (double)SAMPLE_RATE);
    printf("The second samples %.2f of a sample later than the first,\n",
           (double)TRUE_OFFSET);
    printf("because its crystal has drifted.\n\n");

    printf("delay_by_phase measures the offset as %.4f samples.\n\n",
           (double)measured);

    real_t base = farrow_smallest_delay(ORDER);

    // WHICHEVER STREAM IS AHEAD IS THE ONE THAT WAITS. The sign of what was
    // measured says which, and both delays stay inside the range this way.
    real_t hold_first = base + ((measured > REAL_C(0.0)) ? measured
                                                        : REAL_C(0.0));
    real_t hold_second = base + ((measured > REAL_C(0.0)) ? REAL_C(0.0)
                                                          : -measured);

    printf("An order of %u can delay between %.2f and %.2f samples, thus a\n",
           ORDER, (double)farrow_smallest_delay(ORDER),
           (double)farrow_largest_delay(ORDER));
    printf("shift of %.4f on its own is not something it can do. Both streams\n",
           (double)measured);
    printf("go through a filter instead: the first by %.4f, the second by\n",
           (double)hold_first);
    printf("%.4f. Both are in range, both come out with the same whole delay,\n",
           (double)hold_second);
    printf("and the fraction between them is gone.\n\n");

    if(!farrow_is_valid_delay(&early, hold_first)
       || !farrow_is_valid_delay(&late, hold_second))
    {
        printf("Those delays are outside what this order can give.\n");
        farrow_free(&early);
        farrow_free(&late);
        fft_free(&fft);
        return 1;
    }

    (void)farrow_set_delay(&early, hold_first);
    (void)farrow_set_delay(&late, hold_second);

    (void)farrow_process_block(&early, first, lined_up, BLOCK);
    (void)farrow_process_block(&late, second, second_out, BLOCK);

    // Both filters work from ORDER+1 samples, thus the first few out of each
    // are still filling and are left out of the comparison.
    uint32_t settled = FARROW_TAP_COUNT(ORDER) + 2u;

    printf("%34s %12s\n", "", "WHAT IS LEFT");
    printf("-----------------------------------------------\n");
    printf("%34s %12.5f\n", "the two streams as they stand",
           (double)what_is_left(first, second, settled));
    printf("%34s %12.5f\n", "after the fractional delay",
           (double)what_is_left(lined_up, second_out, settled));

    printf("\nA whole sample shift could not have done this: the offset was\n");
    printf("less than one sample to begin with, thus the nearest whole shift\n");
    printf("is no shift at all, and no shift leaves the streams as they were.\n\n");
    printf("THE CORRECTION IS ONLY AS GOOD AS THE MEASUREMENT. What is left is\n");
    printf("not the filter failing: the filter applied exactly the shift it was\n");
    printf("given. It is the difference between the offset that was measured\n");
    printf("and the one that was really there. A longer block, or a quieter\n");
    printf("one, measures the offset more closely and leaves less behind.\n");

    farrow_free(&early);
    farrow_free(&late);
    fft_free(&fft);

    return 0;
}

#endif//RUN_EXAMPLE
