// Turn a resistance into a temperature, from a table of calibration points.
//
// A thermistor is not linear. Its maker gives a table: at these temperatures
// the resistance is that. The device reads a resistance and must give a
// temperature, and the reading almost never falls on a point of the table.
//
// STRAIGHT LINES BETWEEN THE POINTS ARE THE OBVIOUS ANSWER AND ARE NOT GOOD
// ENOUGH. A thermistor bends sharply. Between two points a straight line cuts
// the corner, and the error is worst exactly where the curve bends most, which
// for a thermistor is at the cold end where the resistance changes fastest.
// The answer also has corners: the temperature reported jumps in slope at
// every point of the table, and a controller that acts on the rate of change
// sees a step that is not there.
//
// A cubic spline lays a smooth curve through every point. It passes through
// each one exactly, and its slope and its bend match on both sides of every
// point, thus the answer has no corners anywhere.
//
// FINDING WHICH TWO POINTS THE READING FALLS BETWEEN is a search, and a table
// is in order, thus a binary search finds the place in a handful of steps
// rather than by walking the table. For a table of 12 that is 4 steps rather
// than 12; for a table of 1000 it is 10 rather than 1000. The spline uses that
// search inside itself.
//
// TO PORT THIS: replace the two tables with your own maker's figures and
// read_sensor with your own reading.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CALIBRATE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/interpolate/cspline.h>
#include <ffitt/util/binarysearch.h>
#include <ffitt/util/stats.h>
#include <math.h>
#include <stdio.h>

#define POINTS      12u

// The thermistor itself.
//
// The rule of Steinhart in its simplest form: the logarithm of the resistance
// against the reciprocal of the temperature is nearly straight. A device
// cannot know this rule, which is why it is given a table instead. It stands
// here so that the example can build a table that is really this thermistor's,
// and can then measure how far each way of reading between the points falls
// from the truth.
#define AT_25       REAL_C(12500.0)
#define BETA        REAL_C(3950.0)
#define KELVIN_25   REAL_C(298.15)
#define ZERO_C      REAL_C(273.15)

static real_t resistance_at(real_t celsius)
{
    return AT_25 * REAL_EXP(BETA * ((REAL_C(1.0) / (celsius + ZERO_C))
                                    - (REAL_C(1.0) / KELVIN_25)));
}

static real_t true_temperature(real_t ohms)
{
    real_t inverse = (REAL_C(1.0) / KELVIN_25) + (REAL_LOG(ohms / AT_25) / BETA);

    return (REAL_C(1.0) / inverse) - ZERO_C;
}

// The maker's table. The resistance must RISE through it, because that is what
// a search of this kind needs, thus the temperatures run from hot to cold.
static real_t resistance[POINTS];
static real_t temperature[POINTS];

static void build_the_table(void)
{
    for(uint32_t index = 0; index < POINTS; index++)
    {
        temperature[index] = REAL_C(100.0) - (REAL_C(10.0) * (real_t)index);
        resistance[index] = resistance_at(temperature[index]);
    }
}

// The obvious answer: a straight line between the two points it falls between.
static real_t straight_lines(real_t ohms)
{
    uint32_t place = binarysearch_get_index(resistance, ohms, POINTS);

    if(place == 0u) { place = 1u; }

    real_t low = resistance[place - 1u];
    real_t high = resistance[place];
    real_t part = (ohms - low) / (high - low);

    return temperature[place - 1u]
           + (part * (temperature[place] - temperature[place - 1u]));
}

int main(void)
{
    build_the_table();

    cspline_t curve = cspline_alloc(POINTS);
    cspline_mempool_t pool = cspline_alloc_mempool(POINTS);

    cspline_init(&curve, pool, resistance, temperature);

    printf("A thermistor with a table of %u calibration points, from\n", POINTS);
    printf("%.0f ohms at %.0f C to %.0f ohms at %.0f C.\n\n",
           resistance[0], temperature[0],
           resistance[POINTS - 1u], temperature[POINTS - 1u]);

    printf("The table is in order, thus a binary search finds the place in\n");
    printf("about %u steps rather than by walking all %u of it.\n\n", 4u, POINTS);

    printf("%10s %12s %14s %14s\n", "ohms", "truth", "straight lines",
           "cubic spline");

    real_t straight_worst = REAL_C(0.0);
    real_t spline_worst = REAL_C(0.0);
    real_t straight_errors[9];
    real_t spline_errors[9];
    uint32_t counted = 0;

    // Readings that fall half way between the points of the table, which is
    // where the two ways differ most.
    for(uint32_t which = 0; which < 9u; which++)
    {
        real_t ohms = resistance_at(REAL_C(95.0) - (REAL_C(10.0) * (real_t)which));
        real_t truth = true_temperature(ohms);
        real_t straight = straight_lines(ohms);
        real_t smooth = cspline_get_interpolated_point(&curve, ohms);

        printf("%10.0f %11.3f C %11.3f C %11.3f C\n", ohms, truth, straight,
               smooth);

        straight_errors[counted] = REAL_ABS(straight - truth);
        spline_errors[counted] = REAL_ABS(smooth - truth);

        if(straight_errors[counted] > straight_worst)
        {
            straight_worst = straight_errors[counted];
        }
        if(spline_errors[counted] > spline_worst)
        {
            spline_worst = spline_errors[counted];
        }
        counted++;
    }

    printf("\n%-24s %12s %12s\n", "", "on the mean", "at worst");
    printf("%-24s %10.3f C %10.3f C\n", "straight lines",
           stats_mean(straight_errors, counted), straight_worst);
    printf("%-24s %10.3f C %10.3f C\n", "cubic spline",
           stats_mean(spline_errors, counted), spline_worst);

    printf("\nBoth pass through every point of the table exactly. They differ\n");
    printf("only BETWEEN the points, and that is where every reading falls.\n\n");

    printf("The spline is also smooth in its slope. A straight line changes\n");
    printf("slope at every point of the table, thus a controller acting on\n");
    printf("the rate of change of temperature would see a step at each one\n");
    printf("that the thermistor never made.\n");

    cspline_free(curve);
    cspline_free_mempool(pool);

    return 0;
}

#endif//RUN_EXAMPLE
