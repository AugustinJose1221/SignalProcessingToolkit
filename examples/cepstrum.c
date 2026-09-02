// Find the pitch of a voice.
//
// A voiced sound is not one tone. The vocal folds open and close at the pitch,
// and every opening drives the throat and mouth, which ring at frequencies of
// their own. What comes out is a STACK of harmonics at multiples of the pitch,
// shaped by that ringing.
//
// WHY THE LOUDEST LINE IN THE SPECTRUM IS OFTEN NOT THE PITCH. The shaping is
// what makes a vowel sound like an 'ah' rather than an 'ee', and it can put far
// more energy into the third or fourth harmonic than into the first. A peak
// search on the spectrum then answers three or four times the truth. This
// example builds exactly that case and shows the wrong answer first.
//
// WHAT THE CEPSTRUM DOES ABOUT IT. Take the spectrum, take its logarithm, and
// transform it again. A stack of evenly spaced harmonics is itself a kind of
// ripple, and transforming the ripple folds the whole stack back into ONE peak
// at the spacing, which is the pitch. The shaping, which changes slowly across
// frequency, lands somewhere else entirely and does not compete.
//
// THE ANSWER COMES BACK AS A QUEFRENCY, WHICH IS A TIME. The peak stands at
// the PERIOD of the pitch measured in samples, thus the frequency is the
// sample rate divided by it.
//
// THE RANGE MUST BE GIVEN AND IT MATTERS. cepstrum_best_quefrency searches
// between two quefrencies, and they are what keep it from answering nonsense:
// too low and it finds the shaping, too high and it finds a fraction of the
// pitch. A human voice runs from about 70 hertz to about 400, and that range
// turns into the two numbers below.
//
// TO PORT THIS: replace fill_block with a read from your own microphone, and
// set the range from the pitches you expect.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CEPSTRUM_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/cepstrum.h>
#include <ffitt/transform/fft.h>
#include <ffitt/linalg/cnum.h>

#include <math.h>
#include <stdio.h>

#define BLOCK           1024u
#define SAMPLE_RATE     REAL_C(8000.0)
#define TRUE_PITCH      REAL_C(125.0)
#define LOWEST_HZ       REAL_C(70.0)
#define HIGHEST_HZ      REAL_C(400.0)
#define PI              REAL_C(3.14159265358979323846)

static real_t block[BLOCK];
static real_t answer[BLOCK];
static cnum_t spectrum[BLOCK];
static real_t magnitude[BLOCK];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own microphone.
//
// It stands for a voice at 125 hertz saying a vowel whose throat rings near
// 500 hertz. That ringing lands on the FOURTH harmonic, thus the fourth is the
// loudest line in the spectrum and the first is not.
// ---------------------------------------------------------------------------
static void fill_block(real_t* into)
{
    static const real_t WEIGHT[8] = {
        REAL_C(0.30), REAL_C(0.45), REAL_C(0.70), REAL_C(1.00),
        REAL_C(0.65), REAL_C(0.35), REAL_C(0.20), REAL_C(0.10)
    };

    for(uint32_t index = 0; index < BLOCK; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;
        real_t value = REAL_C(0.0);

        for(uint32_t harmonic = 0; harmonic < 8u; harmonic++)
        {
            real_t frequency = TRUE_PITCH * (real_t)(harmonic + 1u);

            value += WEIGHT[harmonic]
                     * REAL_SIN(REAL_C(2.0) * PI * frequency * time);
        }

        into[index] = value
                      + (REAL_C(0.02) * REAL_SIN((real_t)index * REAL_C(2.399)));
    }
}

// What a peak search on the spectrum answers, which is the wrong road.
static real_t loudest_line(void)
{
    fft_t fft = fft_alloc(BLOCK);
    uint32_t bins = FFT_REAL_BIN_COUNT(BLOCK);
    uint32_t loudest = 1u;

    fft_forward_real(&fft, block, spectrum);
    fft_magnitude(spectrum, magnitude, BLOCK);

    for(uint32_t bin = 1; bin < bins; bin++)
    {
        if(magnitude[bin] > magnitude[loudest])
        {
            loudest = bin;
        }
    }

    real_t frequency = fft_bin_frequency(loudest, BLOCK, SAMPLE_RATE);

    fft_free(&fft);

    return frequency;
}

int main(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(BLOCK);
    real_t strength = REAL_C(0.0);

    fill_block(block);

    // The range, turned from hertz into the quefrencies the module searches.
    uint32_t lowest = (uint32_t)(SAMPLE_RATE / HIGHEST_HZ);
    uint32_t highest = (uint32_t)(SAMPLE_RATE / LOWEST_HZ);

    if(!cepstrum_real(&cepstrum, block, answer))
    {
        printf("The cepstrum refused a block of %u.\n", BLOCK);
        cepstrum_free(&cepstrum);
        return 1;
    }

    uint32_t period = cepstrum_best_quefrency(answer, BLOCK, lowest, highest,
                                              &strength);

    printf("A voice of %.0f hertz, %u samples at %.0f a second.\n",
           (double)TRUE_PITCH, BLOCK, (double)SAMPLE_RATE);
    printf("Searched between %.0f and %.0f hertz, which is quefrency %u to %u.\n\n",
           (double)LOWEST_HZ, (double)HIGHEST_HZ, lowest, highest);

    real_t line = loudest_line();

    printf("  the loudest line of the spectrum   %8.1f Hz   <- %.0f times the\n",
           (double)line, (double)(line / TRUE_PITCH));
    printf("                                                    pitch, not the\n");
    printf("                                                    pitch\n");

    if(period == 0u)
    {
        printf("  the cepstrum                        found nothing in range\n");
    }
    else
    {
        printf("  the cepstrum                       %8.1f Hz   <- the pitch\n",
               (double)(SAMPLE_RATE / (real_t)period));
        printf("\nThe peak stands at quefrency %u, which is a period of %.2f\n",
               period, (double)((real_t)period / SAMPLE_RATE * REAL_C(1000.0)));
        printf("milliseconds, and its strength is %.2f.\n", (double)strength);

        // THE ANSWER IS AS FINE AS ONE SAMPLE OF PERIOD AND NO FINER, and
        // this block lands one step out. That is worth printing rather than
        // hiding: a reader who takes the answer as exact will be surprised by
        // two hertz, and two hertz matters to anything that tracks a voice.
        printf("\nA quefrency is a whole number of samples, thus the pitches\n");
        printf("it can report step: %u samples gives %.1f Hz and %u gives\n",
               period, (double)(SAMPLE_RATE / (real_t)period), period + 1u);
        printf("%.1f. The truth of %.0f is exactly what %u would give, and the\n",
               (double)(SAMPLE_RATE / (real_t)(period + 1u)),
               (double)TRUE_PITCH, period + 1u);
        printf("peak was found at %u: one step out, which is %.1f Hz here.\n",
               period,
               (double)REAL_ABS((SAMPLE_RATE / (real_t)period) - TRUE_PITCH));
        printf("A caller who needs finer refines the peak against its two\n");
        printf("neighbours, as delay_refine_peak does for a lag.\n");
    }

    printf("\nREAD THE STRENGTH BEFORE THE PITCH. A block of noise, or one that\n");
    printf("holds no voice at all, still gives back a quefrency. The strength\n");
    printf("is what says whether a stack of harmonics was really there.\n");

    cepstrum_free(&cepstrum);

    return 0;
}

#endif//RUN_EXAMPLE
