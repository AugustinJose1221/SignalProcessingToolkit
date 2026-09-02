// Turn the resistance of a thermistor into a temperature.
//
// A thermistor is the cheapest temperature sensor there is, and its datasheet
// gives a TABLE and not a formula: resistance against temperature, every five
// or ten degrees. A reading falls between two rows, and the question is what
// to do there.
//
// THE CURVE BENDS HARD AT THE COLD END. A thermistor's resistance does not
// fall evenly: between -20 and -10 degrees it changes several times as much as
// between 30 and 40. That bend is why the choice between the two ways below
// matters at all. On a straight table either would do.
//
// A STRAIGHT LINE BETWEEN THE ROWS is the obvious answer and it is wrong by
// most where the curve bends most, which on this sensor is the cold end, which
// is often exactly where the reading matters.
//
// THE OTHER WAY IS SMOOTH AND STILL NEVER OVERSHOOTS. A cubic spline through
// the same rows is smooth, but it can swing ABOVE every row it passes through,
// and a temperature that never happened is worse than one that is a little
// off. The way of Fritsch and Carlson bends the curve while holding it between
// its neighbours, which is what a reading from a table wants.
//
// THE SLOPES ARE WORKED OUT ONCE. interp_pchip_slopes reads the whole table
// and gives one slope for each row; interp_pchip then reads a place from it
// with no further work. A device works the slopes out at start-up and keeps
// them, thus the cost at each reading is a search and a few multiplications.
//
// TO PORT THIS: replace the two tables with the ones from your own datasheet,
// and read_resistance with a read from your own input. The table must run in
// order, which interp_is_valid_table examines.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_INTERP_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/interpolate/interp.h>

#include <math.h>
#include <stdio.h>

#define ROWS    9u

// A 10k thermistor, from its datasheet.
//
// THE LIST THAT IS SEARCHED MUST RISE, and the reading in hand is a resistance,
// thus the resistance is the list that is searched and it is given rising. A
// thermistor's resistance rises as it COOLS, so the temperatures beside it run
// downward. That is not a mistake in the table; it is what the sensor does.
// interp_is_valid_table examines the rising list and nothing else.
static const real_t RESISTANCE[ROWS] = {
    REAL_C(3600.0),  REAL_C(5330.0),  REAL_C(8060.0),  REAL_C(10000.0),
    REAL_C(12490.0), REAL_C(19900.0), REAL_C(32650.0), REAL_C(55340.0),
    REAL_C(97070.0)
};
static const real_t TEMPERATURE[ROWS] = {
    REAL_C(50.0),  REAL_C(40.0),  REAL_C(30.0),  REAL_C(25.0),
    REAL_C(20.0),  REAL_C(10.0),  REAL_C(0.0),   REAL_C(-10.0),
    REAL_C(-20.0)
};

// The places to ask about: each sits between two rows, one at the warm end
// where the curve is nearly straight and one at the cold end where it bends.
#define ASKED   4u
static const real_t ASK[ASKED] = {
    REAL_C(4400.0),   // between 50 and 40, the curve is gentle here
    REAL_C(11200.0),  // between 25 and 20, near the middle
    REAL_C(42000.0),  // between 0 and -10, the bend is starting
    REAL_C(75000.0)   // between -10 and -20, the bend is worst
};

// ---------------------------------------------------------------------------
// Replace this function with a read from your own input.
// ---------------------------------------------------------------------------
static real_t read_resistance(uint32_t which)
{
    return ASK[which];
}

// The truth, for comparing against. A thermistor really follows the equation
// of Steinhart and Hart, and the table is that equation sampled. The example
// uses it only to say how far each way is off; a device has no such thing.
static real_t truth_at(real_t resistance)
{
    // The three coefficients of a common 10k thermistor.
    real_t log_r = REAL_LOG(resistance);
    real_t inverse = REAL_C(0.001129148)
                     + (REAL_C(0.000234125) * log_r)
                     + (REAL_C(0.0000000876741) * log_r * log_r * log_r);

    return (REAL_C(1.0) / inverse) - REAL_C(273.15);
}

int main(void)
{
    real_t slopes[ROWS];

    if(!interp_is_valid_table(RESISTANCE, ROWS))
    {
        printf("The table does not run in order.\n");
        return 1;
    }

    // Worked out once. A device does this at start-up and keeps the answer.
    if(!interp_pchip_slopes(RESISTANCE, TEMPERATURE, ROWS, slopes))
    {
        printf("The slopes cannot be worked out for that table.\n");
        return 1;
    }

    printf("A 10k thermistor read against a table of %u rows.\n\n", ROWS);
    printf("%12s %10s %10s %10s %10s %10s\n",
           "OHMS", "TRUTH", "LINE", "OFF BY", "PCHIP", "OFF BY");
    printf("--------------------------------------------------------"
           "----------------\n");

    for(uint32_t which = 0; which < ASKED; which++)
    {
        real_t resistance = read_resistance(which);
        real_t truth = truth_at(resistance);

        real_t line = interp_linear(RESISTANCE, TEMPERATURE, ROWS, resistance);
        real_t smooth = interp_pchip(RESISTANCE, TEMPERATURE, slopes, ROWS,
                                     resistance);

        printf("%12.0f %10.2f %10.2f %10.2f %10.2f %10.2f\n",
               (double)resistance, (double)truth,
               (double)line, (double)REAL_ABS(line - truth),
               (double)smooth, (double)REAL_ABS(smooth - truth));
    }

    printf("\nThe straight line is worst where the curve bends most, which on\n");
    printf("this sensor is the cold end. The smooth curve holds there, and it\n");
    printf("never leaves the range its neighbours set: no reading below can be\n");
    printf("colder than -20 or warmer than 50, whatever the arithmetic does.\n");

    return 0;
}

#endif//RUN_EXAMPLE
