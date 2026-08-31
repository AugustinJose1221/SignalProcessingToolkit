// Find which way a sound came from, with two microphones.
//
// Two microphones stand 0.34 m apart. A sound reaches the nearer one first.
// Sound travels 340 m in a second, thus the largest delay between them is a
// millisecond, and at 48000 samples in a second that is 48 samples.
//
// Measuring that delay by looking at the two recordings is hopeless: a sound
// is not a single sharp event, and where exactly does one "start"? The
// question to ask instead is HOW ALIKE ARE THESE TWO WHEN ONE IS MOVED, and
// the lag where the answer is largest is the delay. That is a correlation.
//
// WHICH SCALING, AND WHY IT MATTERS HERE
//
// The raw sum of the products grows with how loud the sound is. A threshold on
// it would hold for a loud sound and not a quiet one. The coefficient does
// not: it is 1 for a perfect match, 0 for none, whatever the sound. Thus the
// same threshold says "I am sure" for a shout and for a whisper.
//
// This example prints the strength as well as the delay, because a delay
// without a strength is not an answer. A room with an echo, or two microphones
// that hear different things, gives a delay that means nothing, and only the
// strength says so.
//
// TO PORT THIS: replace fill_blocks with a read from your own two microphones.
// Set SAMPLE_RATE, SPACING and SPEED_OF_SOUND to your own.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CORRELATE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/correlate.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(48000.0)
#define SPACING         REAL_C(0.34)
#define SPEED_OF_SOUND  REAL_C(340.0)
#define SAMPLES         2048u
#define MAX_LAG         64u

static real_t near_microphone[SAMPLES];
static real_t far_microphone[SAMPLES];
static real_t likeness[MAX_LAG + 1u];

static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// REPLACE THIS with a read from your own two microphones.
//
// The sound is a rattle rather than a tone. A tone would be a poor test: a
// tone at 1000 Hz repeats every 48 samples, thus a delay of 10 and a delay of
// 58 would look exactly alike and the answer would mean nothing. Real sounds
// hold many frequencies, which is what makes the answer certain.
static void fill_blocks(uint32_t delay, real_t how_alike)
{
    static real_t source[SAMPLES + MAX_LAG];

    seed = 7u;
    for(uint32_t index = 0; index < (SAMPLES + MAX_LAG); index++)
    {
        source[index] = rough();
    }

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        near_microphone[index] = source[index + MAX_LAG];

        // The far one hears the same sound later, a little quieter, and with
        // some noise of its own that the near one does not hear.
        far_microphone[index] = (REAL_C(0.8) * source[(index + MAX_LAG) - delay])
                                + ((REAL_C(1.0) - how_alike) * REAL_C(3.0)
                                   * rough());
    }
}

// Give the delay in samples, and write how sure the answer is.
static uint32_t find_delay(real_t* strength)
{
    correlate_cross(near_microphone, far_microphone, SAMPLES, likeness,
                    MAX_LAG, CORRELATE_COEFFICIENT);

    uint32_t best = 0;
    for(uint32_t lag = 1; lag <= MAX_LAG; lag++)
    {
        if(likeness[lag] > likeness[best]) { best = lag; }
    }

    *strength = likeness[best];

    return best;
}

int main(void)
{
    real_t largest = (SPACING / SPEED_OF_SOUND) * SAMPLE_RATE;

    printf("Two microphones %.2f m apart, read at %.0f samples in a second.\n",
           (real_t)SPACING, (real_t)SAMPLE_RATE);
    printf("A sound from straight along the line between them arrives\n");
    printf("%.0f samples later at the far one. That is the largest delay\n",
           largest);
    printf("there can be, and it fixes how far the search must look.\n\n");

    printf("%-28s %8s %10s %10s\n", "what the microphones hear",
           "delay", "angle", "sure?");

    struct { uint32_t delay; real_t alike; const char* what; } cases[4] = {
        {24u, REAL_C(0.95), "a sound from one side"},
        {0u,  REAL_C(0.95), "a sound from straight ahead"},
        {40u, REAL_C(0.95), "a sound from further round"},
        {24u, REAL_C(0.10), "a room full of echo"}
    };

    for(uint32_t which = 0; which < 4u; which++)
    {
        real_t strength;

        fill_blocks(cases[which].delay, cases[which].alike);
        uint32_t found = find_delay(&strength);

        // The delay in samples becomes a time, and the time becomes an angle.
        real_t seconds = (real_t)found / SAMPLE_RATE;
        real_t part = (seconds * SPEED_OF_SOUND) / SPACING;
        if(part > REAL_C(1.0)) { part = REAL_C(1.0); }
        real_t angle = REAL_C(90.0) - ((REAL_C(180.0) / REAL_C(3.14159265))
                                       * REAL_ATAN2(REAL_SQRT(REAL_C(1.0)
                                                              - (part * part)),
                                                    part));

        printf("%-28s %5u    %6.0f deg %8.2f  %s\n",
               cases[which].what, found, angle, strength,
               (strength > REAL_C(0.5)) ? "yes" : "NO");
    }

    printf("\nThe last line is the one that matters. The delay it found is\n");
    printf("not nonsense, but the strength says the two microphones are not\n");
    printf("hearing the same sound, thus the angle means nothing. A delay\n");
    printf("without a strength beside it is not an answer.\n");

    return 0;
}

#endif//RUN_EXAMPLE
