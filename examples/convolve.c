// Ask what a probe will report, before the probe is in the water.
//
// A temperature probe does not read the truth; it reads the truth smeared. Put
// it into water that jumps twenty degrees and it climbs over seconds, because
// the sheath and the mass of the sensor must warm first. That smearing is the
// probe's impulse response, and it is measured once on a bench: drop the probe
// into a bath and record how it climbs.
//
// ONCE THAT RESPONSE IS KNOWN, THE PROBE CAN BE ASKED IN ADVANCE. Slide the
// response along the temperature the process will really do, and the answer is
// what the log will hold. That is a convolution, and it answers the question
// that matters before any hardware is wet: WOULD THE EXCURSION EVEN BE VISIBLE?
//
// A brief excursion is the case that catches people out. A spike that lasts
// less time than the probe takes to respond does not appear as a spike in the
// log. It appears as a small bump, and a threshold set on the true height will
// never fire, no matter how the alarm is tuned.
//
// TWO MODES, AND THE ONE TO TAKE HERE IS THE MIDDLE. CONVOLVE_SAME gives an
// answer as long as the input and lined up with it, which is what a log wants.
// CONVOLVE_FULL gives every place the two touch, which is longer than the
// input and runs past the end of the process.
//
// AND THE FAST WAY IS FOR A LONG SHAPE. This response is short, thus the plain
// way wins and the header says so: below a shape of about 60 the transform's
// fixed cost is not paid back. convolve_by_transform is there for the long
// shape, and the cost tests hold both sides of that crossover.
//
// TO PORT THIS: replace the response with your own probe's, measured on a
// bench, and fill_process with the temperature your process will really do.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CONVOLVE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/convolve.h>

#include <math.h>
#include <stdio.h>

#define SAMPLES         200u
#define RESPONSE_LEN    24u
#define SAMPLE_RATE     REAL_C(4.0)
#define TIME_CONSTANT   REAL_C(2.0)

static real_t process[SAMPLES];
static real_t response[RESPONSE_LEN];
static real_t logged[SAMPLES + RESPONSE_LEN];

// The probe, measured once on a bench: it answers a step by climbing towards
// it, and the climb is a falling exponential. The response is scaled so that
// it adds to one, thus a steady temperature is reported as itself and not
// larger or smaller.
static void build_response(void)
{
    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; index < RESPONSE_LEN; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        response[index] = REAL_EXP(-time / TIME_CONSTANT);
        total += response[index];
    }

    for(uint32_t index = 0; index < RESPONSE_LEN; index++)
    {
        response[index] /= total;
    }
}

// ---------------------------------------------------------------------------
// Replace this function with the temperature your process will really do.
//
// It stands for a bath at 20 degrees with two excursions to 40: one that lasts
// twelve seconds, and one that lasts a single second.
// ---------------------------------------------------------------------------
static void fill_process(real_t* block)
{
    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t degrees = REAL_C(20.0);

        if((index >= 40u) && (index < 88u))          // twelve seconds
        {
            degrees = REAL_C(40.0);
        }
        if((index >= 150u) && (index < 154u))        // one second
        {
            degrees = REAL_C(40.0);
        }

        block[index] = degrees;
    }
}

static real_t highest(const real_t* block, uint32_t from, uint32_t until)
{
    real_t largest = block[from];

    for(uint32_t index = from; index < until; index++)
    {
        if(block[index] > largest)
        {
            largest = block[index];
        }
    }

    return largest;
}

int main(void)
{
    uint32_t room = convolve_output_size(SAMPLES, RESPONSE_LEN, CONVOLVE_SAME);

    build_response();
    fill_process(process);

    if(!convolve_direct(process, SAMPLES, response, RESPONSE_LEN, logged,
                        CONVOLVE_SAME))
    {
        printf("The convolution was refused.\n");
        return 1;
    }

    printf("A probe with a time constant of %.0f seconds, at %.0f samples a\n",
           (double)TIME_CONSTANT, (double)SAMPLE_RATE);
    printf("second. Its response is %u samples long, and the log it writes is\n",
           RESPONSE_LEN);
    printf("%u values, as long as the process itself.\n\n", room);

    real_t long_truth = highest(process, 40u, 88u);
    real_t long_seen = highest(logged, 40u, 100u);
    real_t brief_truth = highest(process, 150u, 154u);
    real_t brief_seen = highest(logged, 150u, 175u);

    printf("%22s %10s %10s %10s\n", "", "TRUTH", "LOGGED", "SEEN AS");
    printf("------------------------------------------------------------\n");
    printf("%22s %10.1f %10.1f %9.0f%%\n", "the long excursion",
           (double)long_truth, (double)long_seen,
           (double)(REAL_C(100.0) * (long_seen - REAL_C(20.0))
                    / (long_truth - REAL_C(20.0))));
    printf("%22s %10.1f %10.1f %9.0f%%\n", "the one second spike",
           (double)brief_truth, (double)brief_seen,
           (double)(REAL_C(100.0) * (brief_seen - REAL_C(20.0))
                    / (brief_truth - REAL_C(20.0))));

    printf("\nThe long excursion arrives whole: the probe has time to catch up.\n");
    printf("The one second spike does not: the probe cannot climb that fast,\n");
    printf("and the log holds a small bump where the truth held twenty\n");
    printf("degrees above the bath.\n\n");
    printf("AN ALARM SET AT 35 DEGREES WOULD FIRE ON THE FIRST AND NEVER ON\n");
    printf("THE SECOND, however it were tuned, because the height it needs\n");
    printf("never reaches the log. That is a fact about the probe and no\n");
    printf("arrangement of software mends it. A faster probe would.\n");

    return 0;
}

#endif//RUN_EXAMPLE
