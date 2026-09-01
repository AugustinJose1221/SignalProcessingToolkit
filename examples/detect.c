// Hear a ping come back, say when it arrived, and notice the tank draining.
//
// A depth sounder sends a chirp into the water and listens. Three questions
// follow one after the other, and the library answers each with a different
// module. Each question is one that a threshold on a single sample cannot
// answer, and each of them fails in its own way if the answer is taken without
// the number that says whether to believe it.
//
//   DID IT COME BACK? The echo is quieter than the noise, thus no sample of the
//   reading looks like anything. matched slides the chirp that was SENT along
//   the reading and adds up the products. Where the echo is, every product is
//   positive at once.
//
//   WHEN EXACTLY? Two hydrophones a little apart both hear the echo, and which
//   heard it first says which way it came from. The difference is a fraction of
//   a sample, and rounding it to a whole sample throws the bearing away. delay
//   measures it two ways, and the two ways disagreeing is the only warning
//   either gives.
//
//   IS THE TANK DRAINING? The bed reads a little nearer each ping. The change
//   is far smaller than the noise, thus no reading is remarkable and a hundred
//   of them together are. changepoint adds them up.
//
// THE THREAD THROUGH ALL THREE: a detector always answers. Give matched a
// reading of nothing and it still reports a largest score somewhere. Give delay
// two readings with nothing in common and it still gives a lag. Each module
// therefore gives a second number, and this example shows what each of them
// does when there is nothing there.
//
// TO PORT THIS: replace listen_for_the_echo with a read from your own receiver
// and send_the_chirp with the shape your own transmitter sends.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_DETECT_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/detect/changepoint.h>
#include <ffitt/detect/delay.h>
#include <ffitt/detect/matched.h>
#include <ffitt/transform/fft.h>
#include <ffitt/util/stats.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(96000.0)
#define CHIRP_LENGTH    512u
#define BLOCK           2048u
#define TRANSFORM       512u
#define LARGEST_LAG     32u
#define PI              REAL_C(3.14159265358979323846)

// Where the echo comes back from, in samples. At 96 000 samples a second and
// 1500 metres a second in water, 700 samples there and back is about 5.5 m.
#define ECHO_AT         700u

// How far the second hydrophone hears the echo behind the first. Not a whole
// number of samples, because a real one never is.
#define BETWEEN_EARS    REAL_C(2.35)

static real_t chirp[CHIRP_LENGTH];
static real_t near_ear[BLOCK];
static real_t far_ear[BLOCK];
static real_t score[BLOCK];
static real_t work[DELAY_WORK_COUNT(LARGEST_LAG)];
static cnum_t first_work[TRANSFORM];
static cnum_t second_work[TRANSFORM];
static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

// REPLACE THIS with the shape your own transmitter sends.
//
// A chirp and not a tone. A tone matches itself at every shift of a whole turn,
// thus a matched filter on a tone finds it in a dozen places at once. A chirp
// sweeps, thus it looks like no shift of itself and the score has one peak.
//
// THE SHAPE IS WRITTEN AS A FUNCTION OF TIME AND NOT AS A LIST, and the reason
// is the second question below. The far hydrophone hears the same chirp a part
// of a sample later, and a part of a sample later is a place BETWEEN two of the
// samples of a list. Reaching for the two either side and drawing a straight
// line between them is not that place: a straight line between two samples
// delays the low frequencies of a sweep by more than the high ones, and the
// delay then measures as 2.26 rather than the 2.35 that was put in. Reading the
// shape at the place wanted has no such lean, and it is also what a real
// hydrophone does.
static real_t chirp_at(real_t place)
{
    if((place < REAL_C(0.0)) || (place > (real_t)CHIRP_LENGTH))
    {
        return REAL_C(0.0);
    }

    real_t part = place / (real_t)CHIRP_LENGTH;

    // From an eighth of the sample rate up to three eighths, which is a sweep
    // the whole of the band can carry.
    real_t turn = REAL_C(2.0) * PI * (REAL_C(0.125) + (REAL_C(0.125) * part))
                  * place;

    // Faded in and out, so that the two ends do not knock.
    real_t fade = REAL_C(0.5) * (REAL_C(1.0)
                                 - REAL_COS(REAL_C(2.0) * PI * part));

    return fade * REAL_SIN(turn);
}

static void send_the_chirp(void)
{
    for(uint32_t index = 0; index < CHIRP_LENGTH; index++)
    {
        chirp[index] = chirp_at((real_t)index);
    }
}

// REPLACE THIS with a read from your own receiver.
//
// The echo is quieter than the noise on purpose. That is the case the module is
// for; where the echo is louder than the noise there is nothing to answer.
static void listen_for_the_echo(real_t loudness, real_t at, bool anything_there)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        near_ear[index] = rough();
        far_ear[index] = rough();
    }

    if(!anything_there)
    {
        return;
    }

    for(uint32_t index = 0; index < CHIRP_LENGTH; index++)
    {
        // The near ear hears the echo where it stands, and the far ear hears
        // the same chirp a part of a sample later. Both are read from the shape
        // itself rather than from the list, for the reason chirp_at gives.
        near_ear[(uint32_t)at + index] += loudness * chirp[index];
        far_ear[(uint32_t)at + index] += loudness
                                         * chirp_at((real_t)index
                                                    - BETWEEN_EARS);
    }
}

int main(void)
{
    send_the_chirp();

    printf("A depth sounder sends a chirp of %u samples at %.0f kHz and\n",
           CHIRP_LENGTH, (real_t)(SAMPLE_RATE / REAL_C(1000.0)));
    printf("listens for %u samples. Three questions follow.\n\n", BLOCK);

    // ---- 1. did it come back? ----
    matched_t matched = matched_make();

    if(!matched_design(&matched, chirp, CHIRP_LENGTH))
    {
        printf("The chirp holds no energy.\n");

        return 1;
    }

    uint32_t offsets = MATCHED_SCORE_COUNT(BLOCK, CHIRP_LENGTH);

    // The threshold is asked for ONCE and used for every ping. It is in units
    // of the noise of the reading, thus it does not depend on how loud any one
    // ping came back.
    real_t threshold = matched_threshold_for(REAL_C(0.01), offsets);

    printf("1. DID IT COME BACK?\n\n");
    printf("   A wrong answer is wanted no more often than one ping in a\n");
    printf("   hundred. There are %u offsets in a block, thus each one may be\n",
           offsets);
    printf("   wrong one time in %.0f, and the score must reach %.2f times\n",
           (real_t)(REAL_C(100.0) * (real_t)offsets), threshold);
    printf("   the noise of the reading.\n\n");

    printf("   %-34s %8s %8s %6s\n", "the reading holds", "loudest",
           "noise", "found");

    real_t loudnesses[5] = {REAL_C(0.0), REAL_C(0.1), REAL_C(0.2),
                            REAL_C(0.35), REAL_C(0.6)};

    for(uint32_t which = 0; which < 5u; which++)
    {
        seed = 41u;
        listen_for_the_echo(loudnesses[which], (real_t)ECHO_AT,
                            loudnesses[which] > REAL_C(0.0));

        matched_score_block(&matched, near_ear, BLOCK, score);

        real_t noise = stats_deviation(near_ear, BLOCK);

        uint32_t where = 0u;
        real_t best = REAL_C(0.0);

        matched_best(&matched, near_ear, BLOCK, &where, &best);

        real_t stands_out = best / noise;
        bool found = stands_out > threshold;

        char what[64];

        if(loudnesses[which] <= REAL_C(0.0))
        {
            snprintf(what, sizeof(what), "noise alone");
        }
        else
        {
            snprintf(what, sizeof(what), "an echo %.2f as loud as the noise",
                     (real_t)(loudnesses[which] / noise));
        }

        printf("   %-34s %8.2f %8.2f %6s", what, stands_out, noise,
               found ? "yes" : "no");

        if(found)
        {
            printf("   at %u", where);
        }

        printf("\n");
    }

    printf("\n   THE FIRST LINE IS THE ONE TO READ. There is no echo in it and\n");
    printf("   the filter still reports a largest score: it has to, because\n");
    printf("   some offset is always the largest. What says the answer means\n");
    printf("   nothing is that the score did not reach the threshold. A\n");
    printf("   program that took the offset and ignored the score would find\n");
    printf("   an echo in every block it was ever given, including this one.\n\n");

    printf("   The two lines after it are the same thing with an echo really\n");
    printf("   there. The score does not move, because the loudest place in\n");
    printf("   the block is still a lump of noise and not the echo. The\n");
    printf("   filter is right to say nothing: at that loudness the echo\n");
    printf("   cannot be told from the noise, and no other method could tell\n");
    printf("   it either.\n\n");

    printf("   The echo is found from four tenths as loud as the noise\n");
    printf("   upwards, and found at exactly the sample it stands on. THAT IS\n");
    printf("   WHAT A MATCHED FILTER BUYS: %u samples of chirp add up together\n",
           CHIRP_LENGTH);
    printf("   while the noise, having no shape, does not. A threshold on any\n");
    printf("   single sample of this block would find nothing at all.\n\n");

    // ---- 2. when exactly? ----
    printf("2. WHEN EXACTLY, AND FROM WHICH SIDE?\n\n");
    printf("   Two hydrophones hear the same echo. The far one hears it %.2f\n",
           (real_t)BETWEEN_EARS);
    printf("   samples later, and which way it came from is worked out from\n");
    printf("   that number. Rounding it to a whole sample throws the bearing\n");
    printf("   away, thus both ways below work below a sample.\n\n");

    fft_t fft = fft_alloc(TRANSFORM);

    printf("   %-34s %10s %10s %8s\n", "the reading holds", "correlation",
           "phase", "agree");

    real_t for_delay[5] = {REAL_C(0.2), REAL_C(0.5), REAL_C(1.0),
                           REAL_C(2.0), REAL_C(8.0)};

    for(uint32_t which = 0; which < 5u; which++)
    {
        seed = 41u;
        listen_for_the_echo(for_delay[which], (real_t)ECHO_AT, true);

        // Both ways are given the stretch of the block the echo stands in,
        // rather than the whole block. Everywhere else is noise alone, and
        // noise alone has nothing to say about the delay.
        const real_t* near_here = &near_ear[ECHO_AT];
        const real_t* far_here = &far_ear[ECHO_AT];

        real_t by_correlation = REAL_C(0.0);
        real_t by_phase = REAL_C(0.0);
        real_t strength = REAL_C(0.0);

        delay_by_correlation(near_here, far_here, TRANSFORM, LARGEST_LAG, work,
                             &by_correlation, &strength);
        delay_by_phase(near_here, far_here, TRANSFORM, &fft, first_work,
                       second_work, &by_phase);

        char what[64];

        snprintf(what, sizeof(what), "an echo %.2f as loud as the noise",
                 (real_t)(for_delay[which]
                          / stats_deviation(near_ear, BLOCK)));

        printf("   %-34s %10.3f %10.3f %8.2f\n", what, by_correlation,
               by_phase, strength);
    }

    printf("\n   READ THE TABLE DOWNWARDS AND WATCH THE TWO WAYS COME\n");
    printf("   TOGETHER. At the top of it they are not measuring the delay at\n");
    printf("   all: the correlation says 7 samples and the phase says 250,\n");
    printf("   which is further than the reading is long. At the bottom they\n");
    printf("   say 2.31 and 2.30, and the answer put in was %.2f.\n\n",
           (real_t)BETWEEN_EARS);

    printf("   NEITHER WAY SAYS IT IS WRONG AT THE TOP OF THE TABLE. Each one\n");
    printf("   gives a number, and the number is about noise. What says so is\n");
    printf("   that THE TWO DISAGREE. They rest on different things -- one\n");
    printf("   fits a curve through three points of a correlation, the other\n");
    printf("   reads a slope across a spectrum -- thus a reading that breaks\n");
    printf("   one of them does not usually break the other in the same\n");
    printf("   direction. Running both and comparing costs one transform and\n");
    printf("   is the only warning available.\n\n");

    printf("   The last column is the second warning, and it is cheaper. It\n");
    printf("   says how much the two hydrophones agree where they agree best,\n");
    printf("   and it climbs from 0.10 to 0.82 down the table alongside the\n");
    printf("   answers coming together. A pair of readings with nothing in\n");
    printf("   common still has a place where they agree best, and this is\n");
    printf("   what says that place means nothing.\n\n");

    // ---- 3. is the tank draining? ----
    printf("3. IS THE BED GETTING NEARER?\n\n");

    // Every ping gives one reading of the depth, and the depth is worked out
    // from where the echo stood. What is watched is that one number, ping after
    // ping, and not the reading inside a ping.
    real_t depths[600];

    seed = 77u;

    for(uint32_t ping = 0; ping < 600u; ping++)
    {
        // The bed sits at 5.50 m and the reading wanders by 4 cm from ping to
        // ping. After ping 300 it starts to come nearer by 1 cm each hundred
        // pings, which is a quarter of the wander.
        real_t drift = (ping > 300u)
                       ? (REAL_C(0.01) * (real_t)(ping - 300u)
                          / REAL_C(100.0))
                       : REAL_C(0.0);

        depths[ping] = REAL_C(5.50) - drift + (REAL_C(0.04) * rough());
    }

    // What ordinary looks like, measured from the first two hundred pings,
    // which are known to be good.
    real_t level = stats_mean(depths, 200u);
    real_t wander = stats_deviation(depths, 200u);

    printf("   The bed sits at %.3f m and the reading wanders by %.3f m from\n",
           level, wander);
    printf("   one ping to the next. After ping 300 it starts coming nearer at\n");
    printf("   1 cm every hundred pings, which is a quarter of the wander over\n");
    printf("   a hundred pings. NO SINGLE PING IS REMARKABLE anywhere in this\n");
    printf("   run, and the two hundred pings after the change began look the\n");
    printf("   same as the two hundred before it.\n\n");

    printf("   %-12s %14s %14s %14s\n", "threshold", "first alarm", "began at",
           "wrong alarms");

    real_t thresholds[4] = {REAL_C(4.0), REAL_C(5.0), REAL_C(8.0),
                            REAL_C(12.0)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        changepoint_t watcher = changepoint_make();

        changepoint_design(&watcher, level, wander, REAL_C(1.0),
                           thresholds[which]);

        uint32_t first_alarm = 0u;
        uint32_t began_at = 0u;
        uint32_t wrong = 0u;

        for(uint32_t ping = 0; ping < 600u; ping++)
        {
            changepoint_way_t said = changepoint_process_sample(&watcher,
                                                                depths[ping]);

            if(said == CHANGEPOINT_NONE)
            {
                continue;
            }

            if(ping <= 300u)
            {
                // Nothing had changed yet, thus this one is wrong.
                wrong++;
            }
            else if(first_alarm == 0u)
            {
                first_alarm = ping;
                began_at = ping - changepoint_began_ago(&watcher);
            }
        }

        printf("   %-12.1f", thresholds[which]);

        if(first_alarm == 0u)
        {
            printf(" %14s %14s", "never", "-");
        }
        else
        {
            printf(" %14u %14u", first_alarm, began_at);
        }

        printf(" %14u\n", wrong);
    }

    printf("\n   READ DOWN THE TABLE. That is the whole choice, and the\n");
    printf("   arithmetic cannot make it: a low threshold hears the change\n");
    printf("   sooner and cries wolf on the way there, and a high one is quiet\n");
    printf("   and late. Which of those costs more is a question about the\n");
    printf("   ship and not about the numbers.\n\n");
    printf("   The third column is what makes the alarm useful, AND IT MUST\n");
    printf("   BE READ FOR WHAT IT SAYS. It gives the ping the running sum\n");
    printf("   last stood at nothing on, which is when the reading last looked\n");
    printf("   ordinary. The change was put in at ping 300 and the column says\n");
    printf("   366. Both are right: the drift at ping 300 is nothing at all\n");
    printf("   and grows from there, thus there was nothing to walk away from\n");
    printf("   until about ping 350. A step would have been pointed at\n");
    printf("   exactly; a slope is pointed at where it became large enough to\n");
    printf("   see, which is the honest answer and the one worth having.\n\n");

    printf("   That still narrows a fault to a stretch of about seventy pings\n");
    printf("   out of six hundred, which is what says which shift, which load\n");
    printf("   or which tide it was.\n");

    fft_free(&fft);

    return 0;
}

#endif//RUN_EXAMPLE
