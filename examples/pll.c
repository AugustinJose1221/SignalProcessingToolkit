// Follow the frequency of the mains as it drifts.
//
// A grid runs at 50 hertz and never at exactly 50. As load comes on and off,
// the whole grid slows and quickens, and that drift is not a fault to filter
// away: it is the reading. A grid under strain runs slow, and equipment that
// must stay in step with the mains has to know the frequency all the time.
//
// WHY NOT COUNT THE CROSSINGS OF ZERO. That is the first idea and it does not
// hold up. Noise near a crossing moves it, thus each measurement jitters, and
// averaging the jitter away over enough cycles is to be slow to see a real
// change. A phase locked loop holds an oscillator of its own against the
// incoming wave, and that oscillator carries the memory of every cycle it has
// already seen. Noise on one cycle hardly moves it.
//
// TWO NUMBERS THAT ARE PARTS OF THE SAMPLE RATE AND NOT HERTZ, and both are
// easy to read past. The bandwidth given to pll_design is a part of the sample
// rate: 1 hertz at 1000 samples a second is 0.001, and passing the 1 gives a
// design the module refuses. pll_pull_range answers in the same unit, thus it
// is multiplied by the rate before it is printed as hertz.
//
// READ THE LOCK QUALITY BEFORE THE FREQUENCY. A loop that has not yet caught
// the wave still reports a frequency, and that number means nothing. The
// quality says whether to believe it, and pll_settle_samples says how long it
// takes to get there.
//
// THE FREQUENCY OF ONE SAMPLE IS NOISY AND THE EXAMPLE SHOWS IT. The raw
// reading swings more than a hertz either side while the true frequency stands
// still. That is not the loop failing; it is what an instantaneous frequency
// is. A monitor averages it, and the moving mean over half a second turns a
// column that swings by a hertz into one that is right to a hundredth.
//
// TO PORT THIS: replace fill_block with a read from your own input. Set
// SAMPLE_RATE to its rate and NOMINAL to the mains frequency where you are,
// which is 50 hertz in most of the world and 60 in the Americas.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_PLL_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/estimate/pll.h>
#include <ffitt/filter/movavg.h>

#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(1000.0)
#define NOMINAL         REAL_C(50.0)

// A hertz either side, given as the part of the sample rate that the module
// asks for.
#define BANDWIDTH_HZ    REAL_C(1.0)
#define BANDWIDTH       (BANDWIDTH_HZ / SAMPLE_RATE)

#define DAMPING         REAL_C(0.707)
#define BLOCK           500u
#define BLOCK_COUNT     14u
#define SMOOTH_WINDOW   500u
#define SAG_FROM        7u
#define SAG_UNTIL       11u
#define SAG_TO          REAL_C(49.6)
#define PI              REAL_C(3.14159265358979323846)

// ---------------------------------------------------------------------------
// Replace this function with a read from your own input.
//
// It stands for a mains that runs at 50.0 hertz and sags to 49.6 for two
// seconds as a large load comes on, with a third harmonic and a little noise
// on top, which is what an outlet really carries. A sag of 0.4 hertz is a
// serious event on a real grid.
// ---------------------------------------------------------------------------
static void fill_block(real_t* block, uint32_t which, real_t* truth)
{
    static real_t phase = REAL_C(0.0);

    *truth = ((which >= SAG_FROM) && (which < SAG_UNTIL)) ? SAG_TO : NOMINAL;

    for(uint32_t index = 0; index < BLOCK; index++)
    {
        phase += (REAL_C(2.0) * PI * (*truth)) / SAMPLE_RATE;

        block[index] = REAL_SIN(phase)
                       + (REAL_C(0.05) * REAL_SIN(REAL_C(3.0) * phase))
                       + (REAL_C(0.03) * REAL_SIN((real_t)index * REAL_C(12.9898)));
    }
}

int main(void)
{
    real_t block[BLOCK];
    pll_t pll = pll_make();
    movavg_t smooth = movavg_alloc(SMOOTH_WINDOW);

    if(!pll_design(&pll, NOMINAL, SAMPLE_RATE, BANDWIDTH, DAMPING))
    {
        printf("The loop cannot be designed for those numbers.\n");
        movavg_free(&smooth);
        return 1;
    }

    printf("A loop on the mains at %.0f hertz, %.0f samples a second.\n",
           (double)NOMINAL, (double)SAMPLE_RATE);
    printf("Bandwidth %.0f Hz, which is %.4f of the sample rate.\n",
           (double)BANDWIDTH_HZ, (double)BANDWIDTH);
    printf("It catches on in about %u samples, which is %.1f seconds,\n",
           pll_settle_samples(&pll),
           (double)pll_settle_samples(&pll) / (double)SAMPLE_RATE);
    printf("and it can follow the wave %.2f Hz either side of %.0f.\n\n",
           (double)(pll_pull_range(&pll) * SAMPLE_RATE), (double)NOMINAL);

    printf("%6s %9s %9s %10s %7s   %s\n",
           "BLOCK", "TRUTH", "RAW", "SMOOTHED", "LOCK", "");
    printf("--------------------------------------------------------------"
           "--------\n");

    for(uint32_t which = 0; which < BLOCK_COUNT; which++)
    {
        real_t truth = REAL_C(0.0);
        real_t smoothed = REAL_C(0.0);

        fill_block(block, which, &truth);

        for(uint32_t index = 0; index < BLOCK; index++)
        {
            (void)pll_process_sample(&pll, block[index]);

            // The frequency of one sample is noisy. The mean over half a
            // second is not.
            smoothed = movavg_process_sample(&smooth,
                                             pll_get_frequency(&pll,
                                                               SAMPLE_RATE));
        }

        real_t quality = pll_lock_quality(&pll);
        const char* note = "";

        if(quality < REAL_C(0.9))
        {
            note = "still catching on";
        }
        else if(truth < NOMINAL)
        {
            note = "the grid is sagging";
        }

        printf("%6u %9.2f %9.2f %10.2f %7.2f   %s\n",
               which, (double)truth,
               (double)pll_get_frequency(&pll, SAMPLE_RATE),
               (double)smoothed, (double)quality, note);
    }

    printf("\nThe RAW column swings more than a hertz while the truth stands\n");
    printf("still. The SMOOTHED column follows the sag of %.1f Hz and comes\n",
           (double)(NOMINAL - SAG_TO));
    printf("back, and it is right to about a hundredth of a hertz.\n");

    movavg_free(&smooth);

    return 0;
}

#endif//RUN_EXAMPLE
