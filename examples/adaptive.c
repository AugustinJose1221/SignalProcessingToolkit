// Hear someone speak next to a running fan.
//
// A microphone in a workshop hears a voice and a fan together. A second
// microphone sits at the fan and hears the fan alone. The fan reaches the
// first microphone changed: quieter, later, and coloured by the room. Nobody
// knows by how much, and it changes when a door opens.
//
// NO FILTER OF FREQUENCY CAN DO THIS. A fan is not one tone; it is a rush of
// noise that covers the whole band the voice lives in. Any filter that took
// the fan out would take the voice with it.
//
// What parts them is not frequency. It is that the second microphone hears the
// fan and NOT the voice. An adaptive filter learns whatever the room does to
// the fan between the two microphones and takes it away.
//
// TAKE THE ERROR AND NOT THE OUTPUT. This is the one thing to get right: the
// output of the filter is the fan as it learned it, and the error is what is
// left when that has been taken away. The error is the voice.
//
// THE ONE WAY THIS FAILS QUIETLY: if the second microphone can hear the voice
// as well, the filter learns to remove the voice too, because that also makes
// the error smaller. Nothing in the numbers says so. Put the reference
// microphone where it hears the noise and nothing else.
//
// TO PORT THIS: replace fill_blocks with reads from your own two microphones.
// The filter must be at least as long as the delay between them, in samples.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_ADAPTIVE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/adaptive.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(16000.0)
#define SAMPLES         48000u
#define FILTER_LENGTH   32u
#define PI              REAL_C(3.14159265358979323846)

static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// Run the whole thing and give back what was measured.
//
// REPLACE THE TWO LINES MARKED BELOW with reads from your own microphones.
static void listen(bool reference_hears_the_voice, real_t rate,
                   real_t* voice_kept, real_t* fan_left, real_t* fan_before,
                   uint32_t* where_the_delay_is)
{
    adaptive_t filter = adaptive_alloc(FILTER_LENGTH);
    real_t fan_history[FILTER_LENGTH];

    adaptive_design(&filter, ADAPTIVE_NORMALISED, rate);
    for(uint32_t index = 0; index < FILTER_LENGTH; index++)
    {
        fan_history[index] = REAL_C(0.0);
    }

    seed = 3u;

    real_t together = REAL_C(0.0);
    real_t voice_energy = REAL_C(0.0);
    real_t left_over = REAL_C(0.0);
    real_t arrived = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        // The voice. A real one would be speech; this is two tones so that the
        // example can measure how much of it survived.
        real_t voice = REAL_SIN(REAL_C(2.0) * PI * REAL_C(220.0) * time)
                       + (REAL_C(0.6) * REAL_SIN(REAL_C(2.0) * PI
                                                 * REAL_C(440.0) * time));

        // ---- REPLACE: the microphone at the fan ----
        real_t at_the_fan = REAL_C(2.0) * rough();

        for(uint32_t k = FILTER_LENGTH - 1u; k > 0u; k--)
        {
            fan_history[k] = fan_history[k - 1u];
        }
        fan_history[0] = at_the_fan;

        // The room delays the fan by 6 samples and halves it on the way.
        real_t fan_in_the_room = REAL_C(0.5) * fan_history[6];

        // ---- REPLACE: the microphone by the speaker ----
        real_t at_the_speaker = voice + fan_in_the_room;

        real_t reference = reference_hears_the_voice
                           ? (at_the_fan + voice) : at_the_fan;

        // THE ERROR IS THE ANSWER.
        real_t cleaned = adaptive_error(&filter, reference, at_the_speaker);

        if(index > ((SAMPLES * 3u) / 4u))
        {
            together += cleaned * voice;
            voice_energy += voice * voice;
            left_over += (cleaned - voice) * (cleaned - voice);
            arrived += fan_in_the_room * fan_in_the_room;
        }
    }

    *voice_kept = together / voice_energy;
    *fan_left = left_over;
    *fan_before = arrived;

    // Where the largest coefficient stands is the delay between the two
    // microphones, in samples. The filter found it without being told.
    uint32_t largest = 0;
    for(uint32_t index = 1; index < FILTER_LENGTH; index++)
    {
        if(REAL_ABS(adaptive_get_coefficient(&filter, index))
           > REAL_ABS(adaptive_get_coefficient(&filter, largest)))
        {
            largest = index;
        }
    }
    *where_the_delay_is = largest;

    adaptive_free(&filter);
}

int main(void)
{
    real_t kept;
    real_t left;
    real_t before;
    uint32_t delay;

    printf("A voice and a fan on one microphone, the fan alone on another.\n");
    printf("The room delays the fan by 6 samples and halves it.\n\n");

    printf("%-38s %10s %10s %8s\n", "", "fan left", "voice kept", "delay");

    listen(false, REAL_C(0.02), &kept, &left, &before, &delay);
    printf("%-38s %9.1f%% %9.2f %7u\n",
           "the reference hears the fan alone",
           (real_t)(REAL_C(100.0) * left / before), kept, delay);

    listen(true, REAL_C(0.02), &kept, &left, &before, &delay);
    printf("%-38s %9.1f%% %9.2f %7u\n",
           "the reference can hear the voice too",
           (real_t)(REAL_C(100.0) * left / before), kept, delay);

    printf("\nThe first line is the answer: under a hundredth of the fan is\n");
    printf("left and the voice came through whole. The filter also found the\n");
    printf("delay of 6 samples by itself, which is what its largest\n");
    printf("coefficient says.\n\n");

    printf("The second line is the trap. The reference microphone could hear\n");
    printf("the voice, thus the filter learned to remove the voice as well,\n");
    printf("because that also made the error smaller. Look at what is left of\n");
    printf("the voice. NOTHING IN THE NUMBERS THE FILTER GIVES SAYS THIS HAS\n");
    printf("HAPPENED: the error fell, and the answer is empty.\n\n");

    printf("How fast to learn is a trade and not a setting to get right:\n");
    printf("%-38s %10s %10s\n", "", "fan left", "voice kept");
    real_t rates[3] = {REAL_C(0.5), REAL_C(0.1), REAL_C(0.02)};
    for(uint32_t which = 0; which < 3u; which++)
    {
        listen(false, rates[which], &kept, &left, &before, &delay);
        printf("  rate %.2f                              %9.1f%% %9.2f\n",
               rates[which], (real_t)(REAL_C(100.0) * left / before), kept);
    }
    printf("\nA high rate follows a change quickly and rattles about the\n");
    printf("answer. A low rate settles closer and takes longer to get there.\n");

    return 0;
}

#endif//RUN_EXAMPLE
