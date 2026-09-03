// Ask whether a reading holds a pattern, before any pattern can be named.
//
// A bearing that is beginning to wear does not announce itself. Long before a
// line appears in the spectrum, the noise it makes stops being quite noise: a
// faint structure creeps in, because the same flaw passes the same place every
// turn. The question a monitor wants answered is not "which frequency" but
// something cruder and earlier: IS THERE ANY PATTERN IN THIS AT ALL?
//
// PREDICTABILITY IS THE ANSWER. Try to guess each sample from the few before
// it. Noise cannot be guessed, and whatever is left after the best guess is as
// large as the reading itself. A reading with structure CAN be guessed in
// part, and what is left is smaller. The ratio of the two is a number between
// nothing and one, and it needs no model of the machine and no list of the
// frequencies a fault would make.
//
// WHY THIS MODULE AND NOT A PLAIN ADAPTIVE FILTER. Both learn to predict. A
// lattice learns each stage against the one before it rather than all of them
// together, thus it settles at nearly the same speed whatever the reading
// looks like. A plain filter given a reading whose energy is bunched at one
// frequency can take many times as long, and a bearing's noise is exactly that
// sort of reading.
//
// THE ERROR BEFORE AND THE ERROR AFTER are what lattice_error_before and
// lattice_error_after give, and their ratio is the number wanted. Read them
// after the filter has settled and not before: an unsettled filter predicts
// nothing and reports every reading as noise.
//
// THE REFLECTION COEFFICIENTS ARE THE FINGERPRINT. Beyond the single ratio,
// lattice_get_reflection gives one number for each stage, and those numbers
// together describe the structure that was found. Two recordings can be
// compared by them without any spectrum being kept.
//
// TO PORT THIS: replace fill_reading with a read from your own sensor.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_LATTICE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/lattice.h>
#include <ffitt/util/generate.h>

#include <math.h>
#include <stdio.h>

#define SAMPLES         8000u
#define STAGES          8u
#define RATE            REAL_C(0.02)
#define FORGETTING      REAL_C(0.995)
#define SETTLE          2000u
#define PI              REAL_C(3.14159265358979323846)

static real_t reading[SAMPLES];
static real_t wanted[SAMPLES];
static real_t error[SAMPLES];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own sensor.
//
// A healthy bearing gives noise and nothing else. A wearing one gives the same
// noise with a faint resonance in it, far too small to see as a line.
// ---------------------------------------------------------------------------
static void fill_reading(real_t* into, bool wearing)
{
    real_t held = REAL_C(0.0);

    // The library's own noise, seeded, thus two runs of this example compare
    // with each other and a reader can repeat what it printed.
    generate_t noise = generate_make(GENERATE_WHITE_NOISE);

    (void)generate_design(&noise, REAL_C(1000.0), REAL_C(8000.0));
    generate_set_seed(&noise, 1u);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t value = generate_sample(&noise);

        if(wearing)
        {
            // A resonance rung by the flaw: noise fed through a narrow
            // resonance is still noise to look at, and it is no longer
            // unpredictable.
            held = (REAL_C(0.93) * held) + (REAL_C(0.20) * value);
            value += held;
        }

        into[index] = value;
    }
}

// How much of the reading the filter managed to predict, from 0 to 1.
static real_t how_predictable(const real_t* block)
{
    lattice_t lattice = lattice_alloc(STAGES);

    if(!lattice_design(&lattice, RATE, FORGETTING))
    {
        lattice_free(&lattice);
        return REAL_C(0.0);
    }

    // The filter predicts each sample from the ones before it, thus what it is
    // given as the wanted value is the reading itself, one step on.
    for(uint32_t index = 0; index + 1u < SAMPLES; index++)
    {
        wanted[index] = block[index + 1u];
    }
    wanted[SAMPLES - 1u] = REAL_C(0.0);

    (void)lattice_process_block(&lattice, block, wanted, error, SAMPLES);

    lattice_free(&lattice);

    // THE TWO error FUNCTIONS GIVE THE LAST SAMPLE AND NOT A RUNNING TOTAL.
    // The header says so, and it was read as a total here first: the answer
    // then rested on one sample out of eight thousand and told the healthy
    // reading from the worn one not at all. What is wanted is the ENERGY the
    // ladder could not explain, over the whole settled part, against the
    // energy that was there to explain.
    real_t left_over = REAL_C(0.0);
    real_t there_was = REAL_C(0.0);

    for(uint32_t index = SETTLE; index + 1u < SAMPLES; index++)
    {
        left_over += error[index] * error[index];
        there_was += wanted[index] * wanted[index];
    }

    if(there_was <= REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    return REAL_C(1.0) - (left_over / there_was);
}

int main(void)
{
    printf("A vibration sensor on a bearing, %u samples, %u stages.\n\n",
           SAMPLES, STAGES);

    fill_reading(reading, false);
    real_t healthy = how_predictable(reading);

    fill_reading(reading, true);
    real_t wearing = how_predictable(reading);

    printf("%24s %14s\n", "", "PREDICTABLE");
    printf("---------------------------------------\n");
    printf("%24s %13.3f\n", "a healthy bearing", (double)healthy);
    printf("%24s %13.3f\n", "one beginning to wear", (double)wearing);

    printf("\nThe healthy reading sits at nothing, which is what noise means:\n");
    printf("the filter learned all it could and still predicted none of it. A\n");
    printf("value at or a little below zero is that answer, and not a fault.\n\n");
    printf("The worn one does not sit at nothing. The fault here is faint - far\n");
    printf("too small to stand up as a line in a spectrum - and the number\n");
    printf("still moves clearly, without anyone naming a frequency, choosing a\n");
    printf("band, or keeping a spectrum from when the machine was new.\n\n");
    printf("THIS SAYS THERE IS SOMETHING AND NEVER WHAT IT IS. A monitor uses\n");
    printf("it to decide when a closer look is worth the trouble, and the psd\n");
    printf("or the cepstrum takes that closer look.\n");

    return 0;
}

#endif//RUN_EXAMPLE
