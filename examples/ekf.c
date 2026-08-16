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

#include <sptk/estimate/ekf.h>
#include <sptk/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define DT              0.01f       // 100 reads in a second
#define GRAVITY         9.81f       // metres in a second, in a second
#define STEPS           500u        // 5 seconds
#define PI              3.14159265358979323846f

// The true bias of the gyroscope. A real device does not know this value; the
// filter must find it. Here it stands only to make the readings.
#define TRUE_BIAS       0.02f

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
    float angle = matrix_get_element((matrix_t*)state, 0, 0);
    float bias = matrix_get_element((matrix_t*)state, 1, 0);
    float gyroscope = matrix_get_element((matrix_t*)input, 0, 0);

    matrix_add_element(result, 0, 0, angle + ((gyroscope - bias) * DT));
    matrix_add_element(result, 1, 0, bias);
}

// h says what the accelerometer would read for a given state.
static void expected_accelerometer(const matrix_t* state, matrix_t* result)
{
    float angle = matrix_get_element((matrix_t*)state, 0, 0);

    matrix_add_element(result, 0, 0, -GRAVITY * sinf(angle));
    matrix_add_element(result, 1, 0, GRAVITY * cosf(angle));
}

// ---------------------------------------------------------------------------
// Replace these two functions with reads from your own sensors.
//
// They stand for a device that tilts slowly to and fro. The gyroscope holds
// the true turning speed plus its bias plus noise. The accelerometer holds
// gravity through the true angle plus noise.
// ---------------------------------------------------------------------------
static float true_angle_at(uint32_t step)
{
    return 0.4f * sinf(2.0f*PI*0.2f*(float)step*DT);
}

static float true_speed_at(uint32_t step)
{
    return 0.4f * 2.0f*PI*0.2f * cosf(2.0f*PI*0.2f*(float)step*DT);
}

static float sensor_noise(uint32_t step, float size)
{
    return size * sinf((float)step * 12.9898f) * cosf((float)step * 78.233f);
}

static float read_gyroscope(uint32_t step)
{
    return true_speed_at(step) + TRUE_BIAS + sensor_noise(step, 0.01f);
}

static void read_accelerometer(uint32_t step, float* ax, float* az)
{
    float angle = true_angle_at(step);

    *ax = (-GRAVITY * sinf(angle)) + sensor_noise(step + 7, 0.3f);
    *az = (GRAVITY * cosf(angle)) + sensor_noise(step + 13, 0.3f);
}

// ---------------------------------------------------------------------------

static matrix_t make(uint32_t m, uint32_t n, float* values)
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
    float start_state[2] = {0.0f, 0.0f};
    float start_covariance[4] = {1.0f, 0.0f,
                                 0.0f, 1.0f};
    matrix_t x = make(2, 1, start_state);
    matrix_t p = make(2, 2, start_covariance);
    ekf_set_state_matrix(&ekf, &x);
    ekf_set_covariance_matrix(&ekf, &p);

    // Q says how much the model itself is wrong at each step. The angle
    // follows the gyroscope closely, thus its part is small. The bias moves
    // very slowly, thus its part is smaller still. Make these larger if the
    // filter follows the accelerometer too slowly.
    float process[4] = {0.0001f, 0.0f,
                        0.0f,    0.000001f};
    matrix_t q = make(2, 2, process);
    ekf_set_process_noise_covariance_matrix(&ekf, &q);

    // R says how much noise the accelerometer holds. It comes from the data
    // sheet of the sensor, or from a measurement of the sensor lying still.
    float measurement_noise[4] = {0.3f, 0.0f,
                                  0.0f, 0.3f};
    matrix_t r = make(2, 2, measurement_noise);
    ekf_set_measurement_covariance_matrix(&ekf, &r);

    matrix_t gyroscope = matrix_alloc(1, 1);
    matrix_t accelerometer = matrix_alloc(2, 1);

    printf("Sensor fusion of a gyroscope and an accelerometer\n");
    printf("%.0f reads in a second, %u steps, thus %.1f seconds\n\n",
           1.0f/DT, STEPS, (float)STEPS*DT);
    printf("The true bias of the gyroscope is %.3f rad/s. The filter does not\n",
           TRUE_BIAS);
    printf("know it and must find it.\n\n");

    printf("%8s %12s %12s %12s %12s\n",
           "TIME [s]", "TRUE [deg]", "FOUND [deg]", "ERROR [deg]", "BIAS FOUND");

    for(uint32_t step = 0; step < STEPS; step++)
    {
        float ax, az;

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
            float found = matrix_get_element(&ekf.x, 0, 0);
            float truth = true_angle_at(step);

            printf("%8.2f %12.2f %12.2f %12.2f %12.4f\n",
                   (float)step*DT,
                   truth * 180.0f/PI,
                   found * 180.0f/PI,
                   (found - truth) * 180.0f/PI,
                   matrix_get_element(&ekf.x, 1, 0));
        }
    }

    printf("\nThe filter found the gyroscope bias %.4f rad/s, and the true one\n",
           matrix_get_element(&ekf.x, 1, 0));
    printf("is %.4f rad/s. It found that value from the two sensors alone.\n",
           TRUE_BIAS);
    printf("\nWithout the bias in the state the angle would drift by about\n");
    printf("%.1f degrees in a minute.\n", TRUE_BIAS * 60.0f * 180.0f/PI);

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
