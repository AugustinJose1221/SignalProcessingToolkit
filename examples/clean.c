// Clean a reading that has both a wandering level and bad samples.
//
// A load cell on a filling line reports the weight in the hopper. Three things
// are wrong with what it sends, and each one needs a different answer. Reaching
// for one filter and hoping is how a reading gets quietly ruined.
//
//   THE LEVEL WANDERS. The cell warms through the shift and its zero drifts by
//   more than a bag weighs. A filter of frequency could take it out, and at
//   this sample rate a high pass slow enough would spend most of its precision
//   on a number that carries nothing. dcblock follows the level and hands back
//   the difference.
//
//   SOME SAMPLES ARE SIMPLY WRONG. A forklift knocks the frame and one reading
//   in a thousand is nonsense. A mean SPREADS such a sample over its whole
//   window; a median removes it but changes every other sample as well. hampel
//   replaces only the samples it has a reason to replace, and says how many.
//
//   THE REST IS NOISE. What is left is ordinary noise, and a mean is the right
//   answer to it. movavg gives that mean in a fixed time however long the
//   window is.
//
// THE ORDER MATTERS AND IS NOT FREE TO CHOOSE. The bad samples must go FIRST.
// Any filter that runs before them spreads each one over its whole window, and
// from then on there is no single bad sample left to find: there is a bad
// stretch, and it looks like a reading.
//
// TO PORT THIS: replace fill_block with a read from your own cell.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CLEAN_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/filter/dcblock.h>
#include <sptk/filter/hampel.h>
#include <sptk/filter/medfilt.h>
#include <sptk/filter/movavg.h>
#include <sptk/util/stats.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(100.0)
#define SAMPLES         3000u
#define PI              REAL_C(3.14159265358979323846)

// The cell reads about 8 000 000 counts with nothing in the hopper, and a bag
// is worth about 2000 of them.
#define ZERO_READING    REAL_C(8000000.0)
#define BAG             REAL_C(2000.0)

static real_t raw[SAMPLES];
static real_t truth[SAMPLES];
static real_t work[SAMPLES];
static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// REPLACE THIS with a read from your own cell.
static void fill_block(void)
{
    seed = 11u;

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        // What is really in the hopper. It fills and empties, thus the weight
        // rises and falls smoothly rather than stepping.
        real_t weight = BAG * REAL_C(1.5)
                        * (REAL_C(1.0) + REAL_SIN(REAL_C(2.0) * PI
                                                  * REAL_C(0.20) * time));
        truth[index] = weight;

        // The zero of the cell drifting through the shift. Far slower than the
        // hopper fills, which is what lets one be told from the other.
        real_t drift = REAL_C(4000.0) * REAL_SIN(REAL_C(2.0) * PI
                                                 * REAL_C(0.008) * time);

        raw[index] = ZERO_READING + drift + weight + (REAL_C(60.0) * rough());
    }

    // The forklift, three times.
    raw[700] += REAL_C(90000.0);
    raw[1450] -= REAL_C(70000.0);
    raw[2310] += REAL_C(120000.0);
}

// How far a cleaned reading stands from the truth, on the mean.
//
// The level of both is taken away first. Taking the level away is what the
// first step of the chain is FOR, thus a cleaned reading no longer sits where
// the truth sits and comparing them straight would measure that on purpose.
// What is being measured here is the SHAPE: does the answer follow the hopper.
static real_t how_wrong(const real_t* cleaned)
{
    const uint32_t from = 300u;
    real_t cleaned_mean = stats_mean(&cleaned[from], SAMPLES - from);
    real_t truth_mean = stats_mean(&truth[from], SAMPLES - from);
    real_t total = REAL_C(0.0);

    for(uint32_t index = from; index < SAMPLES; index++)
    {
        total += REAL_ABS((cleaned[index] - cleaned_mean)
                          - (truth[index] - truth_mean));
    }

    return total / (real_t)(SAMPLES - from);
}

int main(void)
{
    fill_block();

    printf("A load cell reading about %.0f counts, where a bag is %.0f.\n",
           (real_t)ZERO_READING, (real_t)BAG);
    printf("The zero drifts, the forklift knocks it three times, and the\n");
    printf("rest is noise.\n\n");

    printf("The raw reading runs from %.0f to %.0f.\n",
           stats_min(raw, SAMPLES), stats_max(raw, SAMPLES));
    printf("A bag is %.0f of that, which is %.5f of the number. Six of the\n",
           (real_t)BAG, BAG / ZERO_READING);
    printf("seven digits a float holds would go on the part that carries\n");
    printf("nothing, which is why the level comes off first and in double.\n\n");

    // ---- 1. take the level away ----
    //
    // First, so that every step after it has the precision to work with. A
    // cutoff of 0.05 Hz follows the drift at 0.008 Hz and leaves the hopper,
    // which fills at 0.2 Hz, alone.
    dcblock_t level = dcblock_init(REAL_C(0.05) / SAMPLE_RATE);
    dcblock_process_block(&level, raw, work, SAMPLES);

    // ---- 2. take the bad samples away ----
    hampel_t knocks = hampel_alloc(11);
    uint32_t replaced = hampel_process_block(&knocks, work, work, SAMPLES);

    printf("hampel replaced %u samples of %u. That count is worth reading:\n",
           replaced, SAMPLES);
    printf("a count that climbs is a fault in the frame, not in the filter.\n\n");

    // ---- 3. take the noise away ----
    movavg_t smooth = movavg_alloc(25);
    movavg_process_block(&smooth, work, work, SAMPLES);

    real_t right_way = how_wrong(work);

    // Now the same three steps in the wrong order: smooth first, then look for
    // bad samples.
    dcblock_t level_again = dcblock_init(REAL_C(0.05) / SAMPLE_RATE);
    dcblock_process_block(&level_again, raw, work, SAMPLES);

    movavg_t smooth_first = movavg_alloc(25);
    movavg_process_block(&smooth_first, work, work, SAMPLES);

    hampel_t too_late = hampel_alloc(11);
    uint32_t missed = hampel_process_block(&too_late, work, work, SAMPLES);

    real_t wrong_way = how_wrong(work);

    printf("%-34s %12s %10s\n", "", "wrong by", "knocks found");
    printf("%-34s %10.1f   %10u\n", "bad samples out first, then smooth",
           right_way, replaced);
    printf("%-34s %10.1f   %10u\n", "smooth first, then bad samples",
           wrong_way, missed);

    printf("\nSmoothing first spread each knock over the whole window of the\n");
    printf("mean. There is no single bad sample left to find afterwards:\n");
    printf("there is a bad STRETCH, and a stretch looks like a reading. The\n");
    printf("filter that was meant to catch them now finds fewer, and what it\n");
    printf("misses is already spread through the answer.\n");

    dcblock_reset(&level);
    hampel_free(&knocks);
    hampel_free(&too_late);
    movavg_free(&smooth);
    movavg_free(&smooth_first);

    return 0;
}

#endif//RUN_EXAMPLE
