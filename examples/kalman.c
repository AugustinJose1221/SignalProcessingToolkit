// Follow a trolley with a distance sensor that only measures distance.
//
// A trolley runs along a rail. An ultrasonic sensor at one end reports how far
// away it is, ten times a second, and the reading is noisy by about 5 cm. The
// controller needs two things: where the trolley is, more accurately than one
// reading gives, and HOW FAST IT IS GOING, which no sensor on the rail
// measures at all.
//
// WHY A FILTER OF FREQUENCY IS THE WRONG TOOL HERE
//
// Smoothing the readings would give a better position and no speed. Taking the
// difference between two readings would give a speed, and the noise would
// swamp it: two readings 0.1 s apart, each wrong by 5 cm, give a speed wrong
// by about as much as the trolley ever travels.
//
// A Kalman filter is a different kind of answer. It is told what the trolley
// CAN do, which is that a position changes by the speed times the time and a
// speed changes slowly, and it is told how much to trust the sensor. It then
// keeps both numbers, and every reading corrects both. The speed comes out
// even though nothing measures it, because the model says the two are linked.
//
// WHAT THE TWO NOISES REALLY MEAN, AND THIS IS THE PART THAT IS TUNED
//
//   THE MEASUREMENT NOISE is how much the sensor is wrong by. It is a fact
//   about the sensor and can be measured: leave the trolley still and look at
//   the spread of the readings.
//
//   THE PROCESS NOISE is how much the trolley can do that the model does not
//   describe. It is not a fact about anything; it is a statement of how much
//   the model is trusted. Make it small and the filter believes the model and
//   is slow to notice a real change. Make it large and the filter believes
//   each reading and gives back the noise.
//
// This example runs the same trolley through three settings so that the trade
// can be seen rather than argued about.
//
// TO PORT THIS: replace read_sensor with your own sensor, set STEP to the time
// between readings, and set the two noises as above.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_KALMAN_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/estimate/kalman.h>
#include <sptk/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define STEP            REAL_C(0.1)
#define SAMPLES         200u
#define SENSOR_NOISE    REAL_C(0.05)

static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// Where the trolley really is, and how fast it is really going.
static void truth_at(uint32_t index, real_t* position, real_t* speed)
{
    real_t time = (real_t)index * STEP;

    // It stands still, then moves off at a steady speed, then stops.
    if(time < REAL_C(5.0))
    {
        *position = REAL_C(0.0);
        *speed = REAL_C(0.0);
    }
    else if(time < REAL_C(15.0))
    {
        *position = REAL_C(0.4) * (time - REAL_C(5.0));
        *speed = REAL_C(0.4);
    }
    else
    {
        *position = REAL_C(4.0);
        *speed = REAL_C(0.0);
    }
}

// REPLACE THIS with a read from your own sensor.
static real_t read_sensor(uint32_t index)
{
    real_t position;
    real_t speed;

    truth_at(index, &position, &speed);

    return position + (SENSOR_NOISE * rough());
}

// Run the whole thing with one setting of the process noise, and give back how
// far the two answers stood from the truth on the mean.
static void follow(real_t process_noise, real_t* position_error,
                   real_t* speed_error)
{
    // Two numbers in the state: where it is, and how fast it goes. One number
    // measured: where it is.
    kalman_t kalman = kalman_alloc(1, 2, 1);

    // A position changes by the speed times the time; a speed stays as it was.
    matrix_t a = matrix_create_unit_matrix(2);
    matrix_add_element(&a, 0, 1, STEP);

    // Nothing drives the trolley that the filter is told about.
    matrix_t b = matrix_create_zero_matrix(2, 1);
    matrix_t u = matrix_create_zero_matrix(1, 1);

    // The sensor sees the position and not the speed.
    matrix_t c = matrix_create_zero_matrix(1, 2);
    matrix_add_element(&c, 0, 0, REAL_C(1.0));

    // How much the sensor is wrong by, squared. This is a fact about the
    // sensor.
    matrix_t r = matrix_create_zero_matrix(1, 1);
    matrix_add_element(&r, 0, 0, SENSOR_NOISE * SENSOR_NOISE);

    // How much the trolley can do that the model does not describe. This is
    // the number that is tuned.
    matrix_t q = matrix_create_zero_matrix(2, 2);
    matrix_add_element(&q, 0, 0, process_noise * process_noise * REAL_C(0.25));
    matrix_add_element(&q, 1, 1, process_noise * process_noise);

    // Where it starts, and how little that is trusted.
    matrix_t x = matrix_create_zero_matrix(2, 1);
    matrix_t p = matrix_create_unit_matrix(2);
    matrix_t y = matrix_create_zero_matrix(1, 1);

    kalman_set_state_matrix(&kalman, &x);
    kalman_set_state_transition_matrix(&kalman, &a);
    kalman_set_control_matrix(&kalman, &b);
    kalman_set_input_matrix(&kalman, &u);
    kalman_set_covariance_matrix(&kalman, &p);
    kalman_set_process_noise_covariance_matrix(&kalman, &q);
    kalman_set_measurement_covariance_matrix(&kalman, &r);
    kalman_set_observation_matrix(&kalman, &c);

    seed = 9u;
    real_t position_total = REAL_C(0.0);
    real_t speed_total = REAL_C(0.0);
    uint32_t counted = 0;

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        matrix_add_element(&y, 0, 0, read_sensor(index));
        kalman_step(&kalman, NULL, &y);

        real_t position;
        real_t speed;
        truth_at(index, &position, &speed);

        if(index > 20u)
        {
            position_total += REAL_ABS(matrix_get_element(&kalman.x, 0, 0)
                                       - position);
            speed_total += REAL_ABS(matrix_get_element(&kalman.x, 1, 0) - speed);
            counted++;
        }
    }

    *position_error = position_total / (real_t)counted;
    *speed_error = speed_total / (real_t)counted;

    matrix_free(&a);
    matrix_free(&b);
    matrix_free(&u);
    matrix_free(&c);
    matrix_free(&r);
    matrix_free(&q);
    matrix_free(&x);
    matrix_free(&p);
    matrix_free(&y);
    kalman_free(&kalman);
}

int main(void)
{
    printf("A trolley on a rail, seen by a sensor that is wrong by %.2f m.\n",
           (real_t)SENSOR_NOISE);
    printf("It stands still, moves off at 0.40 m/s, then stops.\n\n");

    // What the plain answers give, for comparison.
    seed = 9u;
    real_t raw_total = REAL_C(0.0);
    real_t difference_total = REAL_C(0.0);
    real_t last = read_sensor(0);

    for(uint32_t index = 1; index < SAMPLES; index++)
    {
        real_t reading = read_sensor(index);
        real_t position;
        real_t speed;
        truth_at(index, &position, &speed);

        raw_total += REAL_ABS(reading - position);
        difference_total += REAL_ABS(((reading - last) / STEP) - speed);
        last = reading;
    }

    printf("Without a filter:\n");
    printf("  the reading itself is wrong by      %6.3f m\n",
           raw_total / (real_t)(SAMPLES - 1u));
    printf("  the speed from two readings is off  %6.3f m/s\n",
           difference_total / (real_t)(SAMPLES - 1u));
    printf("  which is nearly as much as the trolley ever travels.\n\n");

    printf("With the filter, at three settings of the process noise:\n");
    printf("  %-14s %14s %14s\n", "process noise", "position off", "speed off");

    real_t settings[3] = {REAL_C(0.001), REAL_C(0.05), REAL_C(2.0)};
    const char* what[3] = {"trusts the model", "balanced", "trusts the sensor"};

    for(uint32_t which = 0; which < 3u; which++)
    {
        real_t position_error;
        real_t speed_error;

        follow(settings[which], &position_error, &speed_error);

        printf("  %-14.3f %11.3f m %11.3f m/s   %s\n", settings[which],
               position_error, speed_error, what[which]);
    }

    printf("\nThe middle row is the answer: the position is better than one\n");
    printf("reading gives, and there is a speed at all, which no sensor on\n");
    printf("the rail measures.\n\n");

    printf("The first row trusts the model too far. It is smooth and it is\n");
    printf("late: when the trolley moves off, the filter does not believe it\n");
    printf("for several seconds, and both answers suffer for it. The last row\n");
    printf("trusts each reading, thus it follows a change at once and gives\n");
    printf("back more of the noise with it.\n\n");

    printf("There is no right value. There is a trade, and it must be made\n");
    printf("with the machine in front of you.\n");

    return 0;
}

#endif//RUN_EXAMPLE
