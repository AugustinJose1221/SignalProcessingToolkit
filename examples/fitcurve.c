// Fit a curve through calibration readings, and the trap that ruins it.
//
// A pressure sensor is measured against a reference at twelve points. The
// device must turn a reading into a pressure, and a curve of the third order
// through those twelve points is the usual way.
//
// THE TRAP IS NOT THE ORDER. IT IS WHERE THE READINGS SIT.
//
// The sensor is read as a count from an analogue converter of 16 bits, thus the
// readings run from about 6000 to about 60000. A plain fit through those counts
// FAILS COMPLETELY, at either width, at any order, on data that lies on a
// perfect cubic. The reason is that 60000 to the sixth is a number near 10 to
// the twenty-eighth, and the arithmetic of the fit holds sums of such numbers
// beside sums of numbers near 1. Nothing is left of the small ones.
//
// The same twelve readings, brought to a range of about -1 to 1 first, fit
// without any trouble at all. That is the whole of the fix, and this example
// runs both so that the difference is on the screen rather than in a note.
//
// WHAT TO CARRY AWAY: use lstsq_polyfit_scaled unless the x of the readings
// already runs about -1 to 1, and keep the centre and the width beside the
// coefficients, because the coefficients mean nothing without them.
//
// TO PORT THIS: replace the two tables with your own readings against your own
// reference, keep the three numbers that the fit gives in the store of the
// device, and use lstsq_evaluate_scaled on each reading.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FITCURVE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/linalg/lstsq.h>
#include <math.h>
#include <stdio.h>

#define POINTS      12u
#define ORDER       3u

// The counts that the converter gave at each calibration point.
static real_t counts[POINTS] = {
    REAL_C(6100.0),  REAL_C(11000.0), REAL_C(16000.0), REAL_C(21000.0),
    REAL_C(26000.0), REAL_C(31000.0), REAL_C(36000.0), REAL_C(41000.0),
    REAL_C(46000.0), REAL_C(51000.0), REAL_C(56000.0), REAL_C(60000.0)
};

// The pressure that the reference showed, in kilopascal. These lie on a cubic
// in the count, thus a fit of the third order can answer exactly and any
// failure below is the arithmetic and not the data.
static real_t pressure[POINTS];

static void make_the_readings(void)
{
    for(uint32_t index = 0; index < POINTS; index++)
    {
        // A gentle bend, of the kind a real sensor has.
        real_t part = counts[index] / REAL_C(60000.0);

        pressure[index] = REAL_C(20.0) + (REAL_C(180.0) * part)
                          + (REAL_C(12.0) * part * part)
                          - (REAL_C(6.0) * part * part * part);
    }
}

// Give the worst error of a fit across all the calibration points, in the same
// unit as the pressure.
static real_t worst_error_of_a_plain_fit(const real_t* coefficients)
{
    real_t worst = REAL_C(0.0);

    for(uint32_t index = 0; index < POINTS; index++)
    {
        real_t error = REAL_ABS(lstsq_evaluate(coefficients, ORDER,
                                              counts[index])
                                - pressure[index]);

        if(error > worst) { worst = error; }
    }

    return worst;
}

static real_t worst_error_of_a_scaled_fit(const real_t* coefficients,
                                          real_t centre, real_t width)
{
    real_t worst = REAL_C(0.0);

    for(uint32_t index = 0; index < POINTS; index++)
    {
        real_t error = REAL_ABS(lstsq_evaluate_scaled(coefficients, ORDER,
                                                      centre, width,
                                                      counts[index])
                                - pressure[index]);

        if(error > worst) { worst = error; }
    }

    return worst;
}

int main(void)
{
    real_t coefficients[LSTSQ_COEFFICIENT_COUNT(ORDER)];
    real_t centre;
    real_t width;

    make_the_readings();

    printf("Fitting a curve of order %u through %u calibration points.\n",
           ORDER, POINTS);
    printf("The counts run from %.0f to %.0f.\n\n",
           (double)counts[0], (double)counts[POINTS - 1u]);

    // FIRST, THE WAY THAT LOOKS RIGHT AND IS NOT.
    printf("A plain fit, straight through the counts:\n");

    if(lstsq_polyfit(counts, pressure, POINTS, ORDER, coefficients))
    {
        printf("  it answered, worst error %.4f kPa\n",
               (double)worst_error_of_a_plain_fit(coefficients));
    }
    else
    {
        printf("  REFUSED. The counts sit too far from zero for the powers of\n"
               "  the count to stay apart at this width. The module says so\n"
               "  rather than giving back numbers that look like an answer.\n");
    }

    // NOW THE SAME READINGS, BROUGHT TO A RANGE THE ARITHMETIC CAN HOLD.
    printf("\nThe same readings, scaled first:\n");

    if(lstsq_polyfit_scaled(counts, pressure, POINTS, ORDER, coefficients,
                            &centre, &width))
    {
        printf("  it answered, worst error %.4f kPa\n",
               (double)worst_error_of_a_scaled_fit(coefficients, centre,
                                                  width));
        printf("  how much of the movement it accounts for: %.6f\n",
               (double)lstsq_fit_quality_scaled(counts, pressure, POINTS,
                                                coefficients, ORDER, centre,
                                                width));

        // THESE ARE THE NUMBERS THE DEVICE KEEPS. The centre and the width
        // belong with them; the coefficients alone are meaningless.
        printf("\n  keep in the store of the device:\n");
        printf("    centre %.6f  width %.6f\n", (double)centre, (double)width);

        for(uint32_t which = 0; which <= ORDER; which++)
        {
            printf("    coefficient of order %u: %.8f\n", which,
                   (double)coefficients[which]);
        }

        // AND THE WARNING, SHOWN RATHER THAN WRITTEN. Read the very same
        // coefficients as a plain polynomial in the count and the answer is
        // not merely a little off.
        printf("\n  read the wrong way, as a polynomial in the count:\n");
        printf("    at count %.0f it gives %.1f kPa where %.1f kPa is right\n",
               (double)counts[POINTS / 2u],
               (double)lstsq_evaluate(coefficients, ORDER,
                                      counts[POINTS / 2u]),
               (double)pressure[POINTS / 2u]);
    }
    else
    {
        printf("  refused, which should not happen for this data\n");
        return 1;
    }

    return 0;
}

#endif
