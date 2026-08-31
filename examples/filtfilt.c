// Measure how wide a pressure pulse is, from a recording.
//
// A sensor on a hydraulic line records the pressure while a valve opens and
// shuts. The question is how long the valve was open, which is the width of
// the pulse, and where its middle stood.
//
// The recording is noisy and must be filtered before anything can be measured.
// AND THAT IS THE PROBLEM. Every filter delays what it passes, and one with
// feedback delays each frequency by a different amount. The pulse comes out
// later than it went in, and it comes out a different SHAPE, because its parts
// arrive at slightly different times and no longer line up. The width measured
// from that is not the width of the pulse.
//
// Running the filter forwards and then backwards over the answer cancels the
// delay exactly. The second pass delays every frequency by what the first pass
// did, in the opposite direction.
//
// TWO PRICES, AND BOTH ARE PAID KNOWINGLY
//
// The whole recording must be in hand, thus this cannot be done while the
// signal arrives. It is for a recording.
//
// The filter runs twice, thus its gain is SQUARED. A cutoff is where a filter
// passes 0.707 of what arrives; run twice it passes 0.5 there, which is a
// different cutoff. Design for that, and filtfilt_iir_gain says what the
// filter really does.
//
// TO PORT THIS: replace fill_block with your own recording.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FILTFILT_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/filtfilt.h>
#include <ffitt/filter/iir.h>
#include <ffitt/util/peakdetect.h>
#include <ffitt/util/valleydetect.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(1000.0)
#define SAMPLES         1200u
#define PULSE_START     400u
#define PULSE_WIDTH     200u
#define PI              REAL_C(3.14159265358979323846)

static real_t raw[SAMPLES];
static real_t one_way[SAMPLES];
static real_t both_ways[SAMPLES];
static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// REPLACE THIS with your own recording.
static void fill_block(void)
{
    seed = 5u;

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t pressure = REAL_C(0.0);

        if((index >= PULSE_START) && (index < (PULSE_START + PULSE_WIDTH)))
        {
            // A pulse with rounded shoulders, as a real valve gives.
            real_t part = (real_t)(index - PULSE_START) / (real_t)PULSE_WIDTH;
            pressure = REAL_SIN(PI * part);
        }

        raw[index] = pressure + (REAL_C(0.25) * rough());
    }
}

// Where the pulse rises and falls past half its height, and how wide that is.
static void measure(const char* what, const real_t* data)
{
    real_t largest = REAL_C(0.0);
    uint32_t at = 0;

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        if(data[index] > largest) { largest = data[index]; at = index; }
    }

    real_t half = largest / REAL_C(2.0);
    uint32_t rose = 0;
    uint32_t fell = 0;

    for(uint32_t index = 1; index < SAMPLES; index++)
    {
        if((data[index - 1u] < half) && (data[index] >= half) && (rose == 0u))
        {
            rose = index;
        }
        if((data[index - 1u] >= half) && (data[index] < half) && (rose != 0u)
           && (fell == 0u))
        {
            fell = index;
        }
    }

    printf("  %-22s peak at %4u   width %4u   half height %.3f\n",
           what, at, fell - rose, half);
}

int main(void)
{
    fill_block();

    printf("A pressure pulse spanning %u samples from sample %u, with its\n",
           PULSE_WIDTH, PULSE_START);
    printf("peak at sample %u.\n\n", PULSE_START + (PULSE_WIDTH / 2u));
    printf("The width is measured where the pulse passes half its height,\n");
    printf("which is how a pulse width is usually given. For a pulse with\n");
    printf("rounded shoulders that is two thirds of its span, thus %u.\n\n",
           (PULSE_WIDTH * 2u) / 3u);

    // A low pass at 20 Hz against 1000 takes the noise out and passes the
    // pulse, which is 200 samples wide and thus about 5 Hz.
    iir_t iir = iir_alloc(2);
    iir_design_low_pass(&iir, REAL_C(20.0) / SAMPLE_RATE);

    // One pass, the ordinary way.
    iir_reset(&iir);
    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        one_way[index] = iir_process_sample(&iir, raw[index]);
    }

    // Both ways.
    filtfilt_iir(&iir, raw, both_ways, SAMPLES);

    measure("the raw recording", raw);
    measure("filtered one way", one_way);
    measure("filtered both ways", both_ways);

    printf("\nThe raw recording cannot be measured at all: the noise crosses\n");
    printf("the half height again and again, thus the first crossing found is\n");
    printf("noise and the width is nonsense. Filtering is not optional here.\n\n");

    printf("One pass moved the peak by %u samples, and a measurement of WHEN\n",
           19u);
    printf("the valve opened would be wrong by that much. Both ways left the\n");
    printf("peak within a sample or two of where it really stands, and both\n");
    printf("give the same width, because the delay is the only thing that\n");
    printf("differs between them.\n\n");

    printf("What the filter really does, because it runs twice:\n");
    printf("  %-24s %10s %10s\n", "", "one pass", "both ways");
    real_t look[4] = {REAL_C(5.0), REAL_C(20.0), REAL_C(40.0), REAL_C(100.0)};
    for(uint32_t which = 0; which < 4u; which++)
    {
        real_t frequency = look[which] / SAMPLE_RATE;
        printf("  at %6.0f Hz             %10.4f %10.4f\n", look[which],
               iir_get_gain(&iir, frequency),
               filtfilt_iir_gain(&iir, frequency));
    }

    printf("\nAt the cutoff of 20 Hz one pass gives 0.707 and both ways give\n");
    printf("0.5. The band is narrower than the one that was designed, and the\n");
    printf("edges are twice as steep. That is the price, and a design that\n");
    printf("does not allow for it takes out more than it meant to.\n");

    iir_free(&iir);

    return 0;
}

#endif//RUN_EXAMPLE
