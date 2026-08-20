// Clean an ECG signal from a heart monitor.
//
// An electrode on the skin picks up the heart, and it also picks up two things
// that are not the heart:
//
// - BASELINE WANDER. The chest moves with each breath, thus the whole signal
//   drifts up and down at about 0.3 hertz. It is far below the heart.
// - MAINS HUM. The wiring of the room radiates at 50 hertz, and the electrode
//   picks it up. It is far above the useful part of the heart.
//
// The useful band of an ECG lies between about 0.5 and 40 hertz. Two filters
// in a row keep that band and throw the rest away.
//
// WHY TWO KINDS OF FILTER. The cutoff of 0.5 hertz is very low against a
// sample rate of 250, which is 0.002 of the rate. A filter with a finite
// impulse response would need about 2000 coefficients for such a low cutoff,
// which no small device can hold. A filter with an infinite impulse response
// needs four. For the upper cutoff the finite filter is the better choice,
// because it moves every frequency by the same time and thus does not deform
// the sharp peak of a heartbeat.
//
// TO PORT THIS: replace read_electrode with a read from your own device, and
// set SAMPLE_RATE to its rate. If your mains runs at 60 hertz, nothing needs
// to change here, because 60 also lies above the upper cutoff.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FILTER_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/filter/fir.h>
#include <sptk/filter/iir.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(250.0)
#define SIZE            1000u       // 4 seconds
#define PI              REAL_C(3.14159265358979323846)

// The two cutoffs, as a part of the sample rate.
#define LOW_CUTOFF      (REAL_C(0.5) / SAMPLE_RATE)
#define HIGH_CUTOFF     (REAL_C(40.0) / SAMPLE_RATE)

static real_t raw[SIZE];
static real_t heart_only[SIZE];      // What the heart alone would give
static real_t without_wander[SIZE];
static real_t clean[SIZE];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own device.
//
// It stands for an electrode that sees a heart at 75 beats in a minute, a
// chest that breathes at 0.3 hertz, and a room whose wiring runs at 50 hertz.
// ---------------------------------------------------------------------------
static void read_electrode(void)
{
    const real_t beats_in_a_second = REAL_C(75.0) / REAL_C(60.0);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        // The heart. A beat is a sharp peak, thus it needs several frequencies
        // to describe it.
        real_t phase = beats_in_a_second * time;
        real_t into_the_beat = phase - REAL_FLOOR(phase);
        real_t beat = REAL_EXP(-REAL_C(200.0) * (into_the_beat - REAL_C(0.2)) * (into_the_beat - REAL_C(0.2)));
        heart_only[index] = beat;

        real_t wander = REAL_C(0.6) * REAL_SIN(REAL_C(2.0)*PI*REAL_C(0.3)*time);
        real_t hum = REAL_C(0.25) * REAL_SIN(REAL_C(2.0)*PI*REAL_C(50.0)*time);

        raw[index] = heart_only[index] + wander + hum;
    }
}

// Give how far a signal lies from the heart alone, added over every sample.
//
// Two things must be taken into account for the comparison to be fair. The low
// pass filter moves the signal later in time, thus the comparison must move
// with it. And a high pass filter takes the mean of a signal away, which is
// what it is there for, thus the comparison must take the mean away from both
// sides.
static real_t distance_from_the_heart(const real_t* signal, uint32_t delay)
{
    // A filter needs time to settle after its first sample. Leave the first
    // second out, so that the comparison sees the settled filter and not its
    // start.
    uint32_t first = (uint32_t)SAMPLE_RATE + delay;

    real_t signal_mean = REAL_C(0.0);
    real_t heart_mean = REAL_C(0.0);
    uint32_t count = SIZE - first;

    for(uint32_t index = first; index < SIZE; index++)
    {
        signal_mean += signal[index];
        heart_mean += heart_only[index - delay];
    }
    signal_mean /= (real_t)count;
    heart_mean /= (real_t)count;

    real_t total = REAL_C(0.0);
    for(uint32_t index = first; index < SIZE; index++)
    {
        total += REAL_ABS((signal[index] - signal_mean)
                       - (heart_only[index - delay] - heart_mean));
    }

    return total;
}

int main(void)
{
    read_electrode();

    // Step one: an IIR high pass filter takes the baseline wander away. Two
    // sections give an order of four, which is enough for a cutoff this low.
    iir_t high_pass = iir_alloc(2);
    // Every design says whether it could hold what was asked of it. A cutoff
    // that is too low against the sample rate cannot be held, and a design
    // that is not examined gives back a filter that looks right and is not.
    if(!iir_design_high_pass(&high_pass, LOW_CUTOFF))
    {
        printf("the high pass cannot hold a cutoff of %.4f\n", LOW_CUTOFF);
        return 1;
    }
    iir_process_block(&high_pass, raw, without_wander, SIZE);

    // Step two: an FIR low pass filter takes the mains hum away and keeps the
    // shape of each beat.
    fir_t low_pass = fir_alloc(101);
    if(!fir_design_low_pass(&low_pass, HIGH_CUTOFF))
    {
        printf("the low pass is too short for a cutoff of %.4f\n", HIGH_CUTOFF);
        return 1;
    }
    fir_process_block(&low_pass, without_wander, clean, SIZE);

    uint32_t delay = low_pass.length / 2;

    printf("An ECG at %.0f samples in a second, %u samples, %.1f seconds\n\n",
           SAMPLE_RATE, SIZE, (real_t)SIZE/SAMPLE_RATE);

    printf("What each filter lets through:\n\n");
    printf("%14s %12s %12s %12s\n", "FREQUENCY", "HIGH PASS", "LOW PASS", "BOTH");
    real_t points[6] = {REAL_C(0.3), REAL_C(0.5), REAL_C(5.0), REAL_C(25.0), REAL_C(40.0), REAL_C(50.0)};
    const char* what[6] = {"breathing", "cutoff", "heart", "heart", "cutoff", "mains hum"};

    for(uint32_t index = 0; index < 6; index++)
    {
        real_t part = points[index] / SAMPLE_RATE;
        real_t first = iir_get_gain(&high_pass, part);
        real_t second = fir_get_gain(&low_pass, part);

        printf("%9.1f Hz  %12.3f %12.3f %12.3f   %s\n",
               points[index], first, second, first*second, what[index]);
    }

    // Send the clean heart alone through the high pass filter. Whatever
    // distance that gives is not noise: it is what the filter itself does to
    // the shape of the signal.
    static real_t heart_through_high_pass[SIZE];
    static real_t heart_through_low_pass[SIZE];

    iir_reset(&high_pass);
    iir_process_block(&high_pass, heart_only, heart_through_high_pass, SIZE);
    iir_reset(&high_pass);

    fir_reset(&low_pass);
    fir_process_block(&low_pass, heart_only, heart_through_low_pass, SIZE);
    fir_reset(&low_pass);

    printf("\nHow far each signal lies from the heart alone, over the settled part:\n");
    printf("  the raw signal:            %8.1f\n", distance_from_the_heart(raw, 0));
    printf("  after the high pass:       %8.1f\n",
           distance_from_the_heart(without_wander, 0));
    printf("  after both filters:        %8.1f\n",
           distance_from_the_heart(clean, delay));

    printf("\nWhat is left is not noise. Send the clean heart alone through each\n");
    printf("filter and see what the filter itself does to it:\n");
    printf("  the high pass alone:       %8.1f\n",
           distance_from_the_heart(heart_through_high_pass, 0));
    printf("  the low pass alone:        %8.1f\n",
           distance_from_the_heart(heart_through_low_pass, delay));

    printf("\nThe low pass changes the heart by almost nothing, because it delays\n");
    printf("every frequency by the same time. The high pass changes it much more,\n");
    printf("because a filter with feedback does not. That is the price of reaching\n");
    printf("0.5 hertz with ten coefficients instead of two thousand.\n");
    printf("\nFor an ECG this matters: too sharp a high pass bends the flat part\n");
    printf("between two beats, which a reader of the signal looks at. Move the\n");
    printf("cutoff lower, or use fewer sections, when that part must stay flat.\n");

    printf("\nThe two filters together hold %u coefficients:\n",
           (high_pass.sections * IIR_COEFFICIENT_COUNT) + low_pass.length);
    printf("  %u for the high pass, which is the cheap way to reach 0.5 hertz,\n",
           high_pass.sections * IIR_COEFFICIENT_COUNT);
    printf("  %u for the low pass, which keeps the shape of each beat.\n",
           low_pass.length);
    printf("\nThe low pass moves the signal %u samples later, which is %.1f ms.\n",
           delay, ((real_t)delay/SAMPLE_RATE)*REAL_C(1000.0));
    printf("Take that delay into account when you measure the time of a beat.\n");

    iir_free(&high_pass);
    fir_free(&low_pass);

    return 0;
}

#endif
