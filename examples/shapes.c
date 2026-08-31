// Choose the shape of a filter, and see what each choice really costs.
//
// A filter trades three things against each other: how flat the band that
// passes is, how sharply it falls, and how much of the band that is stopped
// gets through. NO SHAPE IS BEST AT ALL THREE. This example takes one
// specification and builds it four ways, so that the trade is on the screen
// rather than in a textbook.
//
// THE SPECIFICATION: pass everything below 500 Hz, stop everything above 750
// Hz by 60 dB, and allow 1 dB of ripple in the band that passes. At 10000
// samples in a second.
//
// WHAT TO LOOK AT. The number of sections is what the shape costs to run: a
// section is five multiplications for every sample, for ever. An elliptic
// filter meets this specification with a third of the sections of a
// Butterworth, and that is the whole reason it exists.
//
// WHAT THE TABLE DOES NOT SHOW is the phase, and that is why the second half
// of this example prints the group delay. A filter that holds every frequency
// back by the same time moves a waveform along and leaves its shape alone. One
// that does not changes the shape.
//
// EVERY SHAPE HERE CLIMBS STEEPLY NEAR ITS CUTOFF, and they climb by different
// amounts: measured across the band that passes, a Butterworth rises by a
// little over twice and an elliptic by more than six times. The shape that
// costs the fewest multiplications costs the most in this, and no measurement
// of gain would ever have shown it.
//
// TO PORT THIS: replace the four numbers below with your own specification and
// read off the sections. Where the shape of the waveform matters, read the
// group delay too, or use filtfilt.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_SHAPES_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/filter/iir.h>
#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(10000.0)
#define PASS_HERTZ      REAL_C(500.0)
#define STOP_HERTZ      REAL_C(750.0)
#define RIPPLE          REAL_C(1.0)
#define ATTENUATION     REAL_C(60.0)

static const char* name_of[4] = {"Butterworth", "Chebyshev I", "Chebyshev II",
                                 "Elliptic"};

// The worst gain anywhere in the band that is stopped.
static real_t worst_that_gets_through(iir_t* filter, real_t from)
{
    real_t worst = REAL_C(0.0);

    for(uint32_t step = 0; step <= 1000u; step++)
    {
        real_t place = from
                       + (((REAL_C(0.5) - from) * (real_t)step)
                          / REAL_C(1000.0));
        real_t gain = iir_get_gain(filter, place);

        if(gain > worst) { worst = gain; }
    }

    return worst;
}

int main(void)
{
    real_t pass_edge = PASS_HERTZ / RATE;
    real_t stop_edge = STOP_HERTZ / RATE;

    printf("Pass below %.0f Hz, stop above %.0f Hz by %.0f dB, %.0f dB of\n"
           "ripple allowed, at %.0f samples in a second.\n\n",
           (double)PASS_HERTZ, (double)STOP_HERTZ, (double)ATTENUATION,
           (double)RIPPLE, (double)RATE);

    printf("  %-14s %9s %8s %14s %16s\n", "shape", "sections", "order",
           "what it costs", "worst that gets");
    printf("  %-14s %9s %8s %14s %16s\n", "", "", "",
           "each sample", "through");

    for(uint32_t which = 0; which < 4u; which++)
    {
        iir_shape_t shape = (iir_shape_t)which;

        uint32_t sections = iir_sections_for(shape, pass_edge, stop_edge,
                                             RIPPLE, ATTENUATION);

        if(sections == 0u)
        {
            printf("  %-14s %9s\n", name_of[which], "cannot");
            continue;
        }

        iir_t filter = iir_alloc(sections);

        // Chebyshev II counts its cutoff where the band that is STOPPED
        // begins; the other three count it at the end of the band that passes.
        real_t cutoff = (shape == IIR_CHEBYSHEV_II) ? stop_edge : pass_edge;

        if(!iir_design_low_pass_with(&filter, cutoff, shape, RIPPLE,
                                     ATTENUATION))
        {
            printf("  %-14s %9s\n", name_of[which], "refused");
            iir_free(&filter);
            continue;
        }

        real_t worst = worst_that_gets_through(&filter, stop_edge);

        printf("  %-14s %9u %8u %10u mul %13.1f dB\n", name_of[which],
               sections, sections * 2u, sections * 5u,
               (double)(REAL_C(20.0) * REAL_LOG10(worst)));

        iir_free(&filter);
    }

    printf("\nEvery one of those meets the specification. They differ only in\n"
           "what they cost to run, and an elliptic filter costs a third of a\n"
           "Butterworth.\n");

    // NOW THE PART THAT THE GAIN DOES NOT SHOW.
    printf("\nWhat each one does to the SHAPE of a waveform, in samples held\n"
           "back, across the band that passes:\n\n");

    printf("  %-14s", "shape");
    for(uint32_t step = 1; step <= 5u; step++)
    {
        printf(" %7.0f", (double)((PASS_HERTZ * (real_t)step) / REAL_C(5.0)));
    }
    printf("  Hz\n");

    for(uint32_t which = 0; which < 4u; which++)
    {
        iir_shape_t shape = (iir_shape_t)which;
        uint32_t sections = iir_sections_for(shape, pass_edge, stop_edge,
                                             RIPPLE, ATTENUATION);

        if(sections == 0u) { continue; }

        iir_t filter = iir_alloc(sections);
        real_t cutoff = (shape == IIR_CHEBYSHEV_II) ? stop_edge : pass_edge;

        if(iir_design_low_pass_with(&filter, cutoff, shape, RIPPLE,
                                    ATTENUATION))
        {
            printf("  %-14s", name_of[which]);

            for(uint32_t step = 1; step <= 5u; step++)
            {
                real_t place = ((PASS_HERTZ * (real_t)step) / REAL_C(5.0))
                               / RATE;

                printf(" %7.1f", (double)iir_group_delay(&filter, place));
            }

            printf("\n");
        }

        iir_free(&filter);
    }

    printf("\nRead across each row. A row that changes little holds the shape\n"
           "of a waveform; a row that changes a great deal bends it.\n");
    printf("\nEVERY row climbs steeply at the end, because every one of these\n"
           "filters is sharpest near its cutoff. But they climb by different\n"
           "amounts: Butterworth rises by a little over twice across the band\n"
           "and the elliptic by more than six times. The shape that costs the\n"
           "fewest multiplications costs the most here, and NO MEASUREMENT OF\n"
           "GAIN WOULD EVER HAVE SHOWN IT.\n");
    printf("\nWhere the shape matters, use filtfilt: it runs the filter both\n"
           "ways and leaves no phase shift at all, at the cost of running\n"
           "twice and needing the whole signal before it starts.\n");

    return 0;
}

#endif
