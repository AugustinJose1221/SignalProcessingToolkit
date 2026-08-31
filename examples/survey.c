// Survey two sensors BEFORE building a canceller, and find out what it can do.
//
// The adaptive example shows a canceller running. This one shows the work that
// comes before that loop is written, because each part of it can tell you the
// loop is not worth writing, and each is one call.
//
//   WILL IT WORK AT ALL? The coherence between the two sensors is the ceiling
//   on cancellation. Nothing beats it, because it is the part of what the
//   first sensor hears that the second can account for at all. A filter cannot
//   subtract what its reference never saw.
//
//   HOW LONG MUST THE FILTER BE? Long enough to cover the delay between the
//   sensors and the spread of the path. Too short and it cannot describe the
//   path; too long and it wastes work and adds noise of its own. The
//   coefficients of a filter made deliberately too long show both, read off.
//
//   HOW FAST SHOULD IT LEARN? This is where most of the trouble lives, and the
//   usual advice to pick a rate for the speed of convergence is only half the
//   story. A filter that learns fast eats the signal it was meant to protect,
//   and the amount it eats is measurable.
//
//   AND THE FAULT THAT LOOKS LIKE NOTHING: a reference sensor that can hear
//   the signal. The filter is asked to make the error small, the signal is
//   part of the error, and so the signal goes. Everything degrades, and the
//   only warning available BEFORE the loop is written is in step one.
//
// TWO THINGS ARE MEASURED THROUGHOUT AND NOT ONE. How much noise went, and how
// much signal stayed. Neither means anything without the other: a canceller
// that removes everything scores perfectly on the first.
//
// TO PORT THIS: replace make_the_scene with two recordings of your own, taken
// AT THE SAME MOMENTS, and set WANTED_HERTZ to something your signal holds.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_SURVEY_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/adaptive.h>
#include <ffitt/filter/dcblock.h>
#include <ffitt/filter/rls.h>
#include <ffitt/transform/csd.h>
#include <ffitt/transform/window.h>
#include <ffitt/util/generate.h>
#include <ffitt/util/stats.h>
#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(8000.0)
#define SAMPLES         32768u

// The block for the coherence. Eight blocks is the fewest csd will answer for,
// and this gives well over a hundred.
#define BLOCK           256u
#define BINS            ((BLOCK / 2u) + 1u)

// THE SIGNAL SITS EXACTLY ON A BIN ON PURPOSE, AND THAT IS NOT A CONVENIENCE.
//
// A tone standing between two bins is spread across both of them and across
// their neighbours, and the dip it makes in the coherence is then spread as
// well: shallower, wider, and moving about as the leak changes. Measured at
// 300 Hz, which falls between bins here, the dip wandered from one bin to
// another and could not be read at all.
//
// 312.5 Hz is 10 bins of 31.25, thus it lands on one and the dip is a single
// clear place. When surveying a real signal, choose the block so that the
// frequency of interest lands near a bin, or use a longer one.
#define WANTED_HERTZ    REAL_C(312.5)
#define WANTED_BIN      10u

// The filter used to SURVEY the path, made deliberately too long: the point is
// to see where the path really lies, not to run cheaply.
#define PROBE_LENGTH    64u

static real_t wanted_signal[SAMPLES];
static real_t noise_alone[SAMPLES];
static real_t noise_at_primary[SAMPLES];
static real_t primary[SAMPLES];
static real_t reference[SAMPLES];
static real_t cleaned[SAMPLES];
static real_t left_over[SAMPLES];
static real_t coherence[BINS];

// The path the noise takes from the reference sensor to the primary one: seven
// samples of delay and a little colouring. NOTHING IN THE PROGRAM IS TOLD
// THIS. It is what the survey has to discover.
#define PATH_DELAY      7u
#define PATH_TAPS       4u

static const real_t path[PATH_TAPS] = {REAL_C(0.6), -REAL_C(0.3),
                                       REAL_C(0.2), REAL_C(0.1)};

// Build the scene. The leak says how much of the signal finds its way into the
// reference sensor, which is the fault this example is really about.
static void make_the_scene(real_t leak)
{
    generate_t hum = generate_make(GENERATE_PINK_NOISE);
    generate_t voice = generate_make(GENERATE_SINE);

    generate_design(&hum, REAL_C(0.0), RATE);
    generate_design(&voice, WANTED_HERTZ, RATE);

    // The same seed every time, so that runs differ ONLY in what is being
    // examined and not in the noise they were given.
    generate_set_seed(&hum, 20260827u);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        // THE NOISE MUST BE THE LOUD ONE. A canceller is for the case where
        // the noise buries the signal. Where it does not, the total loudness
        // barely moves however well the canceller works.
        noise_alone[index] = REAL_C(5.0) * generate_sample(&hum);
        wanted_signal[index] = REAL_C(0.25) * generate_sample(&voice);
    }

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t arrived = REAL_C(0.0);

        for(uint32_t tap = 0; tap < PATH_TAPS; tap++)
        {
            uint32_t back = PATH_DELAY + tap;

            if(index >= back)
            {
                arrived += path[tap] * noise_alone[index - back];
            }
        }

        noise_at_primary[index] = arrived;
        primary[index] = wanted_signal[index] + arrived;
        reference[index] = noise_alone[index] + (leak * wanted_signal[index]);
    }
}

// Draw a row of a plot as characters, so that the shape can be seen on a
// terminal that holds nothing else.
static void draw_bar(real_t value, real_t largest, uint32_t width)
{
    uint32_t filled = 0u;

    if(largest > REAL_SMALLEST)
    {
        real_t part = REAL_ABS(value) / largest;

        filled = (uint32_t)(part * (real_t)width);
    }

    for(uint32_t step = 0; step < width; step++)
    {
        printf("%c", (step < filled) ? '#' : ' ');
    }
}

// Draw one sample as a row of a plot: a mark at the place its value stands,
// with the middle of the row standing for nothing.
//
// A waveform is drawn down the page rather than across it because a terminal
// holds far more rows than it holds columns, and because the two traces can
// then stand side by side at the SAME SCALE, which is the whole point of the
// comparison. Drawn at scales of their own they would look alike.
static void draw_sample(real_t value, real_t reach, uint32_t width, char mark)
{
    uint32_t middle = width / 2u;
    uint32_t at = middle;

    if(reach > REAL_SMALLEST)
    {
        real_t part = value / reach;

        if(part > REAL_C(1.0)) { part = REAL_C(1.0); }
        if(part < -REAL_C(1.0)) { part = -REAL_C(1.0); }

        at = (uint32_t)((real_t)middle + (part * (real_t)(middle - 1u)));
    }

    for(uint32_t step = 0; step < width; step++)
    {
        if(step == at)
        {
            printf("%c", mark);
        }
        else if(step == middle)
        {
            printf(".");
        }
        else
        {
            printf(" ");
        }
    }
}

// Run a canceller and give BOTH numbers that matter.
//
// HOW MUCH SIGNAL SURVIVED is not how large the answer is, which says nothing:
// a canceller that removes everything gives a very small answer. It is how
// much of the signal that was wanted the answer still holds. A 1.0 means it
// came through untouched; a 0.5 means half of it went with the noise.
//
// HOW MUCH NOISE IS LEFT is not how much quieter the answer is, which is a
// different thing again. What remains after a perfect cancellation is the
// signal, which is not nothing, thus the total loudness cannot fall below it
// however well the filter works. So the part of the answer that the signal
// accounts for is taken out first, and what remains is the noise that got
// through.
static void run_it(uint32_t length, real_t rate, real_t* removed_out,
                   real_t* kept_out)
{
    adaptive_t filter = adaptive_alloc(length);
    dcblock_t dc_reference = dcblock_init(REAL_C(0.001));
    dcblock_t dc_primary = dcblock_init(REAL_C(0.001));

    adaptive_design(&filter, ADAPTIVE_NORMALISED, rate);
    adaptive_set_leak(&filter, REAL_C(0.0001));

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t x = dcblock_process_sample(&dc_reference, reference[index]);
        real_t d = dcblock_process_sample(&dc_primary, primary[index]);

        // THE ERROR IS THE ANSWER. The output of the filter is the noise as it
        // learned it; the error is what is left when that has been taken away.
        cleaned[index] = adaptive_error(&filter, x, d);
    }

    real_t together = REAL_C(0.0);
    real_t alone = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        together += cleaned[index] * wanted_signal[index];
        alone += wanted_signal[index] * wanted_signal[index];
    }

    real_t kept = (alone > REAL_SMALLEST) ? (together / alone) : REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        left_over[index] = cleaned[index] - (kept * wanted_signal[index]);
    }

    // The second half only, so that the settling at the start is not counted
    // as a fault of the filter.
    uint32_t from = SAMPLES / 2u;
    uint32_t count = SAMPLES - from;

    real_t was = stats_rms(&noise_at_primary[from], count);
    real_t is_now = stats_rms(&left_over[from], count);

    *removed_out = REAL_C(20.0) * REAL_LOG10((is_now + REAL_SMALLEST)
                                             / (was + REAL_SMALLEST));
    *kept_out = kept;

    adaptive_free(&filter);
}

// Give the coherence between the two sensors, and the value at the bin the
// signal stands on.
static real_t survey_the_coherence(csd_t* judge)
{
    if(!csd_coherence(judge, reference, primary, SAMPLES, coherence))
    {
        return REAL_C(0.0);
    }

    return coherence[WANTED_BIN];
}

int main(void)
{
    csd_t judge = csd_alloc(BLOCK);

    csd_design(&judge, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    printf("A signal of %.1f Hz buried under pink noise. The noise reaches the\n"
           "primary sensor %u samples late and coloured on the way, and\n"
           "nothing in this program is told that.\n\n",
           (double)WANTED_HERTZ, PATH_DELAY);

    make_the_scene(REAL_C(0.0));

    /* ---------------------------------------------------------------- */
    printf("=== STEP ONE: can it work at all? ===\n\n");

    survey_the_coherence(&judge);

    printf("  How much of what the primary sensor hears can the reference\n"
           "  account for, at each frequency, and what that allows:\n\n");
    printf("  %8s  %-32s %10s\n", "hertz", "coherence", "ceiling");

    for(uint32_t bin = 2; bin < BINS; bin += 4u)
    {
        real_t hertz = csd_bin_frequency(&judge, bin, RATE);

        if(hertz > REAL_C(1200.0))
        {
            break;
        }

        real_t left = REAL_C(1.0) - coherence[bin];

        if(left < REAL_C(0.000001))
        {
            left = REAL_C(0.000001);
        }

        printf("  %8.0f  ", (double)hertz);
        draw_bar(coherence[bin], REAL_C(1.0), 32u);
        printf(" %7.1f dB\n", (double)(REAL_C(10.0) * REAL_LOG10(left)));
    }

    printf("\n  Away from the signal the reference accounts for nearly all of\n"
           "  what the primary sensor hears, thus about 20 dB of cancellation\n"
           "  is there to be had.\n");
    printf("\n  THE DIP AT %.0f Hz IS THE SIGNAL, and it is what a good\n"
           "  reference looks like: it has never heard the thing being\n"
           "  measured. Step four shows what happens when it has.\n",
           (double)WANTED_HERTZ);

    /* ---------------------------------------------------------------- */
    printf("\n=== STEP TWO: how long must the filter be? ===\n");

    // THE PROBE USES rls AND NOT adaptive, and the reason is the noise itself.
    // Pink noise leans heavily on itself from one sample to the next, which is
    // exactly where a normalised filter learns slowly: measured, 163 samples
    // against 24. A probe that has not converged shows a smear and not a path,
    // and a delay cannot be read off a smear. The probe runs once and offline,
    // thus its memory does not matter.
    rls_t probe = rls_alloc(PROBE_LENGTH);

    rls_design(&probe, REAL_C(1.0), RLS_DEFAULT_DOUBT);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        rls_error(&probe, reference[index], primary[index]);
    }

    real_t largest = REAL_C(0.0);
    uint32_t at = 0u;
    uint32_t last = 0u;

    for(uint32_t tap = 0; tap < PROBE_LENGTH; tap++)
    {
        real_t size_of = REAL_ABS(rls_get_coefficient(&probe, tap));

        if(size_of > largest)
        {
            largest = size_of;
            at = tap;
        }
    }

    printf("\n  What the probe learned about the path, coefficient by\n"
           "  coefficient. The delay is where the weight begins and the\n"
           "  spread is how far it runs:\n\n");

    for(uint32_t tap = 0; tap < PROBE_LENGTH; tap++)
    {
        real_t value = rls_get_coefficient(&probe, tap);

        if(REAL_ABS(value) > (largest / REAL_C(20.0)))
        {
            last = tap;
        }

        if(tap < 14u)
        {
            printf("  %4u %+8.4f  ", tap, (double)value);
            draw_bar(value, largest, 28u);
            printf("\n");
        }
    }

    uint32_t suggested = last + 4u;

    printf("\n  The largest stands at %u, which IS the delay between the two\n"
           "  sensors in samples, and the weight has died away by %u.\n",
           at, last);
    printf("  A filter of %u taps therefore covers the whole path, where the\n"
           "  probe used %u to find that out.\n", suggested, PROBE_LENGTH);
    printf("\n  It also recovered the shape of the path itself, which nothing\n"
           "  in this program told it.\n");

    rls_free(&probe);

    /* ---------------------------------------------------------------- */
    printf("\n=== STEP THREE: how fast should it learn? ===\n\n");
    printf("  The usual advice is to pick the rate for how fast the filter\n"
           "  settles. THAT IS HALF THE STORY. A filter that learns fast\n"
           "  chases the error from sample to sample, and the signal is part\n"
           "  of that error:\n\n");

    printf("  %8s %16s %14s\n", "rate", "noise removed", "signal kept");

    const real_t rates[6] = {REAL_C(0.5), REAL_C(0.3), REAL_C(0.1),
                             REAL_C(0.05), REAL_C(0.02), REAL_C(0.01)};

    for(uint32_t which = 0; which < 6u; which++)
    {
        real_t removed;
        real_t kept;

        run_it(suggested, rates[which], &removed, &kept);

        printf("  %8.2f %13.1f dB %14.3f\n", (double)rates[which],
               (double)removed, (double)kept);
    }

    printf("\n  READ BOTH COLUMNS. At 0.5 the filter takes half the signal\n"
           "  away with the noise and cancels WORSE for it. At 0.01 it keeps\n"
           "  the signal whole and has not finished learning. The best of\n"
           "  both is near the middle, and neither column alone would have\n"
           "  found it.\n");

    /* ---------------------------------------------------------------- */
    real_t shown_removed;
    real_t shown_kept;

    run_it(suggested, REAL_C(0.05), &shown_removed, &shown_kept);

    printf("\n  AND HERE IS WHAT THAT LOOKS LIKE, at the rate the table\n"
           "  chose. Both traces are drawn to the SAME scale, which is what\n"
           "  makes the comparison honest:\n\n");

    printf("        %-29s %-29s\n", "raw, at the primary sensor",
           "filtered, what is left");

    uint32_t from = 24000u;
    real_t reach = REAL_C(2.0);

    for(uint32_t step = 0; step < 52u; step++)
    {
        uint32_t index = from + step;

        printf("  %5u ", step);
        draw_sample(primary[index], reach, 29u, '#');
        printf(" ");
        draw_sample(cleaned[index], reach, 29u, '#');
        printf("\n");
    }

    printf("\n  The noise buries everything on the left, and the middle of the\n"
           "  right hand trace hardly moves. NOTHING OF THE SIGNAL CAN BE SEEN\n"
           "  IN EITHER at this scale, because at this scale it is small.\n");

    printf("\n  So here is the right hand trace again, drawn four times\n"
           "  larger, against the signal that was wanted:\n\n");

    printf("        %-29s %-29s\n", "filtered, what is left",
           "the signal that was wanted");

    reach = REAL_C(0.5);

    for(uint32_t step = 0; step < 52u; step++)
    {
        uint32_t index = from + step;

        printf("  %5u ", step);
        draw_sample(cleaned[index], reach, 29u, '#');
        printf(" ");
        draw_sample(wanted_signal[index], reach, 29u, '#');
        printf("\n");
    }

    printf("\n  THE TWO SNAKE DOWN THE PAGE TOGETHER, turning at the same\n"
           "  places and leaning the same way. That is the canceller working:\n"
           "  what is left is the signal, which was nowhere to be seen in the\n"
           "  left hand trace of the figure before.\n");
    // What the two traces really reached across the rows that were drawn,
    // rather than a number written down beside a picture that shows another.
    real_t widest_left = REAL_C(0.0);
    real_t widest_right = REAL_C(0.0);

    for(uint32_t step = 0; step < 52u; step++)
    {
        uint32_t index = from + step;

        if(REAL_ABS(cleaned[index]) > widest_left)
        {
            widest_left = REAL_ABS(cleaned[index]);
        }

        if(REAL_ABS(wanted_signal[index]) > widest_right)
        {
            widest_right = REAL_ABS(wanted_signal[index]);
        }
    }

    printf("\n  IT IS NOT THE SIGNAL EXACTLY, and the plot says so: the left\n"
           "  hand trace swings wider, reaching %.2f across these rows where\n"
           "  the signal reaches %.2f. THAT EXTRA WIDTH IS THE NOISE THE\n"
           "  FILTER COULD NOT REACH, riding on top of the answer. It is the\n"
           "  %.1f dB of the table above, seen rather than read.\n",
           (double)widest_left, (double)widest_right, (double)-shown_removed);
    printf("\n  The %.0f parts in a hundred of signal that the filter ate\n"
           "  cannot be seen here at all: at this size it is less than the\n"
           "  width of one character. Some things are only ever numbers, and\n"
           "  that is what the table is for.\n",
           (double)((REAL_C(1.0) - shown_kept) * REAL_C(100.0)));

    /* ---------------------------------------------------------------- */
    printf("\n=== STEP FOUR: a reference that hears the signal ===\n\n");
    printf("  The reference sensor is moved nearer the thing being measured,\n"
           "  a little more each time. Nothing else changes.\n\n");

    printf("  %8s %14s %16s %14s\n", "leak", "coherence at", "noise removed",
           "signal kept");
    printf("  %8s %14.0f %16s %14s\n", "", (double)WANTED_HERTZ, "Hz", "");

    const real_t leaks[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0),
                             REAL_C(4.0), REAL_C(8.0)};

    real_t worst_removed = REAL_C(0.0);
    real_t worst_kept = REAL_C(0.0);

    for(uint32_t which = 0; which < 5u; which++)
    {
        make_the_scene(leaks[which]);

        real_t at_the_signal = survey_the_coherence(&judge);
        real_t removed;
        real_t kept;

        run_it(suggested, REAL_C(0.05), &removed, &kept);

        printf("  %8.1f %14.3f %13.1f dB %14.3f\n", (double)leaks[which],
               (double)at_the_signal, (double)removed, (double)kept);

        worst_removed = removed;
        worst_kept = kept;
    }

    printf("\n  THE FIRST TWO COLUMNS GO WRONG AT ONCE. The dip that marked a\n"
           "  good reference fills in, and the cancellation falls away with\n"
           "  it, from the very first step.\n");
    printf("\n  THE THIRD COLUMN LIES FOR A WHILE. At a leak of 1 the signal\n"
           "  reads BETTER than with no leak at all, because the filter is\n"
           "  now too busy with the reference to eat it. Only once the leak\n"
           "  is large does the signal go, and then it goes quickly.\n");
    printf("\n  That is why the coherence is the measurement to trust: it went\n"
           "  wrong first, it went wrong steadily, and it needed no canceller\n"
           "  to say so.\n");
    printf("\n  By the last row the filter removes %.1f dB of noise and keeps\n"
           "  %.0f parts in a hundred of the signal. It did nothing wrong: it\n"
           "  was asked to make the error small, and by then the signal was\n"
           "  the largest thing left in the error.\n",
           (double)-worst_removed, (double)(worst_kept * REAL_C(100.0)));
    printf("\n  NOTHING IN THE ARITHMETIC SAYS SO. The fix is not in the\n"
           "  filter, the rate or the length. It is to move the sensor, and\n"
           "  step one is how that is known before anything is built.\n");

    csd_free(&judge);

    return 0;
}

#endif
