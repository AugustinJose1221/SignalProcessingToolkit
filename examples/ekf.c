// Sensor fusion: find a tilt angle from a gyroscope and an accelerometer.
//
// This is the problem that almost every device with a motion sensor must
// solve. Two sensors both say something about the tilt, and each one is wrong
// in its own way:
//
// - The GYROSCOPE gives the speed of turning. Add it up through the time and
//   you get the angle. It is smooth and it reacts at once, but every small
//   error adds up, thus the angle drifts away over minutes. The gyroscope also
//   holds a bias: a reading that is not zero while the device lies still.
// - The ACCELEROMETER feels gravity, thus it says which way is down. It never
//   drifts, but every knock and every movement of the device disturbs it.
//
// Together they hold the whole answer: the gyroscope for the short time and
// the accelerometer for the long time. The filter finds the gyroscope bias
// while it runs, which no simple mixing of the two can do.
//
// The state holds two values:
//   x[0] the tilt angle in radians
//   x[1] the bias of the gyroscope in radians for each second
//
// The measurement is the raw reading of the accelerometer, which is not a
// linear function of the angle:
//   ax = -g * sin(angle)
//   az =  g * cos(angle)
// A plain Kalman filter cannot take that. The extended filter can.
//
// TO PORT THIS: replace read_gyroscope and read_accelerometer with reads from
// your own sensors. Set DT to the time between two reads. Set the two noise
// matrices from the data sheets of your sensors, and then make them larger or
// smaller until the filter behaves as you want.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_EKF_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/estimate/ekf.h>
#include <ffitt/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define DT              REAL_C(0.01)       // 100 reads in a second
#define GRAVITY         REAL_C(9.81)       // metres in a second, in a second
#define STEPS           500u        // 5 seconds
#define PI              REAL_C(3.14159265358979323846)

// The true bias of the gyroscope. A real device does not know this value; the
// filter must find it. Here it stands only to make the readings.
#define TRUE_BIAS       REAL_C(0.02)

// ---------------------------------------------------------------------------
// The model.
//
// f says how the state moves in one step of DT. The angle grows by the turning
// speed, which is the gyroscope reading less the bias. The bias itself does
// not change.
// ---------------------------------------------------------------------------
static void move_state(const matrix_t* state, const matrix_t* input,
                       matrix_t* result)
{
    real_t angle = matrix_get_element((matrix_t*)state, 0, 0);
    real_t bias = matrix_get_element((matrix_t*)state, 1, 0);
    real_t gyroscope = matrix_get_element((matrix_t*)input, 0, 0);

    matrix_add_element(result, 0, 0, angle + ((gyroscope - bias) * DT));
    matrix_add_element(result, 1, 0, bias);
}

// h says what the accelerometer would read for a given state.
static void expected_accelerometer(const matrix_t* state, matrix_t* result)
{
    real_t angle = matrix_get_element((matrix_t*)state, 0, 0);

    matrix_add_element(result, 0, 0, -GRAVITY * REAL_SIN(angle));
    matrix_add_element(result, 1, 0, GRAVITY * REAL_COS(angle));
}

// ---------------------------------------------------------------------------
// Replace these two functions with reads from your own sensors.
//
// They stand for a device that tilts slowly to and fro. The gyroscope holds
// the true turning speed plus its bias plus noise. The accelerometer holds
// gravity through the true angle plus noise.
// ---------------------------------------------------------------------------
static real_t true_angle_at(uint32_t step)
{
    return REAL_C(0.4) * REAL_SIN(REAL_C(2.0)*PI*REAL_C(0.2)*(real_t)step*DT);
}

static real_t true_speed_at(uint32_t step)
{
    return REAL_C(0.4) * REAL_C(2.0)*PI*REAL_C(0.2) * REAL_COS(REAL_C(2.0)*PI*REAL_C(0.2)*(real_t)step*DT);
}

static real_t sensor_noise(uint32_t step, real_t size)
{
    return size * REAL_SIN((real_t)step * REAL_C(12.9898)) * REAL_COS((real_t)step * REAL_C(78.233));
}

static real_t read_gyroscope(uint32_t step)
{
    return true_speed_at(step) + TRUE_BIAS + sensor_noise(step, REAL_C(0.01));
}

static void read_accelerometer(uint32_t step, real_t* ax, real_t* az)
{
    real_t angle = true_angle_at(step);

    *ax = (-GRAVITY * REAL_SIN(angle)) + sensor_noise(step + 7, REAL_C(0.3));
    *az = (GRAVITY * REAL_COS(angle)) + sensor_noise(step + 13, REAL_C(0.3));
}

// ---------------------------------------------------------------------------

static matrix_t make(uint32_t m, uint32_t n, real_t* values)
{
    matrix_t matrix = matrix_alloc(m, n);
    for(uint32_t i = 0; i < m; i++)
    {
        for(uint32_t j = 0; j < n; j++)
        {
            matrix_add_element(&matrix, i, j, values[(i*n)+j]);
        }
    }
    return matrix;
}

int main(void)
{
    // One input, which is the gyroscope. Two state values. Two measurements,
    // which are the two axes of the accelerometer.
    ekf_t ekf = ekf_alloc(1, 2, 2);

    ekf_set_state_function(&ekf, move_state);
    ekf_set_measurement_function(&ekf, expected_accelerometer);

    // The filter starts with an angle of zero and a bias of zero. It knows
    // neither, thus the doubt in both is large.
    real_t start_state[2] = {REAL_C(0.0), REAL_C(0.0)};
    real_t start_covariance[4] = {REAL_C(1.0), REAL_C(0.0),
                                 REAL_C(0.0), REAL_C(1.0)};
    matrix_t x = make(2, 1, start_state);
    matrix_t p = make(2, 2, start_covariance);
    ekf_set_state_matrix(&ekf, &x);
    ekf_set_covariance_matrix(&ekf, &p);

    // Q says how much the model itself is wrong at each step. The angle
    // follows the gyroscope closely, thus its part is small. The bias moves
    // very slowly, thus its part is smaller still. Make these larger if the
    // filter follows the accelerometer too slowly.
    real_t process[4] = {REAL_C(0.0001), REAL_C(0.0),
                        REAL_C(0.0),    REAL_C(0.000001)};
    matrix_t q = make(2, 2, process);
    ekf_set_process_noise_covariance_matrix(&ekf, &q);

    // R says how much noise the accelerometer holds. It comes from the data
    // sheet of the sensor, or from a measurement of the sensor lying still.
    real_t measurement_noise[4] = {REAL_C(0.3), REAL_C(0.0),
                                  REAL_C(0.0), REAL_C(0.3)};
    matrix_t r = make(2, 2, measurement_noise);
    ekf_set_measurement_covariance_matrix(&ekf, &r);

    matrix_t gyroscope = matrix_alloc(1, 1);
    matrix_t accelerometer = matrix_alloc(2, 1);

    printf("Sensor fusion of a gyroscope and an accelerometer\n");
    printf("%.0f reads in a second, %u steps, thus %.1f seconds\n\n",
           REAL_C(1.0)/DT, STEPS, (real_t)STEPS*DT);
    printf("The true bias of the gyroscope is %.3f rad/s. The filter does not\n",
           TRUE_BIAS);
    printf("know it and must find it.\n\n");

    printf("%8s %12s %12s %12s %12s\n",
           "TIME [s]", "TRUE [deg]", "FOUND [deg]", "ERROR [deg]", "BIAS FOUND");

    for(uint32_t step = 0; step < STEPS; step++)
    {
        real_t ax, az;

        matrix_add_element(&gyroscope, 0, 0, read_gyroscope(step));
        read_accelerometer(step, &ax, &az);
        matrix_add_element(&accelerometer, 0, 0, ax);
        matrix_add_element(&accelerometer, 1, 0, az);

        // One call does the whole step: it takes the input and the
        // measurement, moves the state forward, and corrects it.
        if(!ekf_step(&ekf, &gyroscope, &accelerometer))
        {
            printf("The filter could not invert its innovation covariance.\n");
            break;
        }

        if((step % 100) == 0)
        {
            real_t found = matrix_get_element(&ekf.x, 0, 0);
            real_t truth = true_angle_at(step);

            printf("%8.2f %12.2f %12.2f %12.2f %12.4f\n",
                   (real_t)step*DT,
                   truth * REAL_C(180.0)/PI,
                   found * REAL_C(180.0)/PI,
                   (found - truth) * REAL_C(180.0)/PI,
                   matrix_get_element(&ekf.x, 1, 0));
        }
    }

    printf("\nThe filter found the gyroscope bias %.4f rad/s, and the true one\n",
           matrix_get_element(&ekf.x, 1, 0));
    printf("is %.4f rad/s. It found that value from the two sensors alone.\n",
           TRUE_BIAS);
    printf("\nWithout the bias in the state the angle would drift by about\n");
    printf("%.1f degrees in a minute.\n", TRUE_BIAS * REAL_C(60.0) * REAL_C(180.0)/PI);

    matrix_free(&x);
    matrix_free(&p);
    matrix_free(&q);
    matrix_free(&r);
    matrix_free(&gyroscope);
    matrix_free(&accelerometer);
    ekf_free(&ekf);

    return 0;
}

#endif
