// Find which way a device is pointing, from a gyroscope and an accelerometer.
//
// A board holds two sensors. The gyroscope says how fast the board turns about
// each of its own three axes. The accelerometer says which way is down.
// Neither one answers the question on its own.
//
//   THE GYROSCOPE IS SMOOTH AND DRIFTS. Adding up its readings gives the
//   attitude, and every reading carries a small steady error that is added up
//   with it. After a minute the answer is wrong by degrees; after an hour it
//   is useless. It also cannot say which way is down at all.
//
//   THE ACCELEROMETER NEVER DRIFTS AND IS NOISY. It sees gravity, thus it says
//   which way is down whenever the board is still. Every knock and every real
//   movement of the board is added to gravity and cannot be told from it.
//
// Together they answer. The gyroscope carries the attitude between readings
// and the accelerometer pulls it back to where down really is.
//
// WHY THIS FILTER AND NOT THE EXTENDED ONE
//
// Turning a vector by an attitude is not a straight operation, and the
// measurement here is exactly that: where gravity lands after the attitude has
// turned it. A straight line laid against that is an approximation, and its
// derivative must be worked out.
//
// This filter needs no derivative. It puts a handful of attitudes through the
// turn ITSELF and looks at where gravity lands for each.
//
// WHY THE ATTITUDE IS HELD AS FOUR NUMBERS AND NOT THREE ANGLES
//
// Three angles have a hole in them: at a pitch of straight up two axes line up
// and one number is lost. A board on a robot arm goes there on purpose. Four
// numbers have no such hole.
//
// TO PORT THIS: replace read_gyroscope and read_accelerometer with reads from
// your own sensors. Set STEP to the time between them.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_ATTITUDE_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/estimate/ukf.h>
#include <sptk/linalg/quaternion.h>
#include <sptk/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define STEP            REAL_C(0.01)
#define SAMPLES         3000u
#define PI              REAL_C(3.14159265358979323846)
#define GRAVITY         REAL_C(9.81)

// How far the gyroscope is wrong by, steadily. This is what makes adding its
// readings up fail, and it is what the accelerometer is there to answer.
#define GYRO_DRIFT      REAL_C(0.01)

static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// What the board is really doing: it pitches up to straight up and holds
// there, which is exactly where three angles would fail.
static quaternion_t true_attitude(uint32_t index)
{
    real_t time = (real_t)index * STEP;
    real_t pitch = (time < REAL_C(10.0))
                   ? ((PI / REAL_C(2.0)) * (time / REAL_C(10.0)))
                   : (PI / REAL_C(2.0));

    return quaternion_from_axis_angle(REAL_C(0.0), REAL_C(1.0), REAL_C(0.0),
                                      pitch);
}

// REPLACE THIS with a read from your own gyroscope. Radians for each second,
// about the three axes of the board itself.
static void read_gyroscope(uint32_t index, real_t* x, real_t* y, real_t* z)
{
    real_t time = (real_t)index * STEP;
    real_t turning = (time < REAL_C(10.0))
                     ? ((PI / REAL_C(2.0)) / REAL_C(10.0)) : REAL_C(0.0);

    *x = (REAL_C(0.002) * rough()) + GYRO_DRIFT;
    *y = turning + (REAL_C(0.002) * rough()) + GYRO_DRIFT;
    *z = (REAL_C(0.002) * rough()) + GYRO_DRIFT;
}

// REPLACE THIS with a read from your own accelerometer. It sees gravity turned
// into the frame of the board.
static void read_accelerometer(uint32_t index, real_t* x, real_t* y, real_t* z)
{
    quaternion_t truth = true_attitude(index);

    // Gravity points down in the world. What the board sees is gravity turned
    // by the opposite of the attitude, which is what the conjugate gives.
    quaternion_rotate(quaternion_conjugate(truth), REAL_C(0.0), REAL_C(0.0),
                      -GRAVITY, x, y, z);

    *x += REAL_C(0.3) * rough();
    *y += REAL_C(0.3) * rough();
    *z += REAL_C(0.3) * rough();
}

// The state is the four numbers of the attitude. The filter is told nothing
// about what turns it, thus the state simply stays as it was between readings;
// the gyroscope is applied outside the filter, before each step.
static void attitude_stays(const matrix_t* state, const matrix_t* input,
                           matrix_t* result)
{
    (void)input;
    matrix_copy((matrix_t*)state, result);
}

// What the accelerometer would read for a given attitude. THIS IS THE PART
// THAT BENDS: turning a vector by an attitude is not a straight operation.
static void what_the_accelerometer_would_see(const matrix_t* state,
                                             matrix_t* result)
{
    quaternion_t q = quaternion_normalise(
        quaternion_make(matrix_get_element((matrix_t*)state, 0, 0),
                        matrix_get_element((matrix_t*)state, 1, 0),
                        matrix_get_element((matrix_t*)state, 2, 0),
                        matrix_get_element((matrix_t*)state, 3, 0)));

    real_t x;
    real_t y;
    real_t z;

    quaternion_rotate(quaternion_conjugate(q), REAL_C(0.0), REAL_C(0.0),
                      -GRAVITY, &x, &y, &z);

    matrix_add_element(result, 0, 0, x);
    matrix_add_element(result, 1, 0, y);
    matrix_add_element(result, 2, 0, z);
}

// How far two attitudes stand apart altogether, in degrees.
static real_t how_far_apart(quaternion_t a, quaternion_t b)
{
    quaternion_t between = quaternion_multiply(quaternion_conjugate(a), b);
    real_t angle;

    quaternion_to_axis_angle(between, NULL, NULL, NULL, &angle);

    return angle * REAL_C(180.0) / PI;
}

// How far apart the two say DOWN is, in degrees.
//
// This is the part an accelerometer can put right, and the part above is not.
// An accelerometer sees gravity and nothing else, thus it says which way is
// down and says nothing whatever about which way the board faces about the
// vertical. Turning the board on the spot changes the attitude and changes
// nothing the accelerometer sees.
//
// Reporting the two apart is the honest way. Reporting only the total would
// hide which half is being answered and which half nothing here can answer.
static real_t how_far_apart_in_tilt(quaternion_t a, quaternion_t b)
{
    real_t ax;
    real_t ay;
    real_t az;
    real_t bx;
    real_t by;
    real_t bz;

    quaternion_rotate(quaternion_conjugate(a), REAL_C(0.0), REAL_C(0.0),
                      REAL_C(-1.0), &ax, &ay, &az);
    quaternion_rotate(quaternion_conjugate(b), REAL_C(0.0), REAL_C(0.0),
                      REAL_C(-1.0), &bx, &by, &bz);

    real_t together = (ax * bx) + (ay * by) + (az * bz);

    if(together > REAL_C(1.0))  { together = REAL_C(1.0); }
    if(together < REAL_C(-1.0)) { together = REAL_C(-1.0); }

    return REAL_ATAN2(REAL_SQRT(REAL_C(1.0) - (together * together)), together)
           * REAL_C(180.0) / PI;
}

int main(void)
{
    ukf_t ukf = ukf_alloc(1, 4, 3);

    matrix_t state = matrix_create_zero_matrix(4, 1);
    matrix_add_element(&state, 0, 0, REAL_C(1.0));

    matrix_t covariance = matrix_create_unit_matrix(4);
    matrix_multiply_scalar_into(&covariance, REAL_C(0.01), &covariance);

    matrix_t process = matrix_create_unit_matrix(4);
    matrix_multiply_scalar_into(&process, REAL_C(0.00001), &process);

    matrix_t measurement = matrix_create_unit_matrix(3);
    matrix_multiply_scalar_into(&measurement, REAL_C(0.09), &measurement);

    matrix_t reading = matrix_create_zero_matrix(3, 1);

    ukf_set_state_function(&ukf, attitude_stays);
    ukf_set_measurement_function(&ukf, what_the_accelerometer_would_see);
    ukf_set_state_matrix(&ukf, &state);
    ukf_set_covariance_matrix(&ukf, &covariance);
    ukf_set_process_noise_covariance_matrix(&ukf, &process);
    ukf_set_measurement_covariance_matrix(&ukf, &measurement);

    printf("A board that pitches up to straight up over 10 seconds and holds.\n");
    printf("Straight up is exactly where three angles lose a number.\n\n");
    printf("The gyroscope is wrong by %.3f radians a second, steadily.\n\n",
           (real_t)GYRO_DRIFT);

    // The gyroscope alone, for comparison.
    quaternion_t gyroscope_only = quaternion_identity();

    seed = 5u;

    printf("%8s %22s %22s\n", "", "gyroscope alone", "with the filter");
    printf("%8s %10s %11s %10s %11s\n", "time", "tilt", "altogether",
           "tilt", "altogether");

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t gx;
        real_t gy;
        real_t gz;
        read_gyroscope(index, &gx, &gy, &gz);

        // The gyroscope on its own: every reading added to the last.
        gyroscope_only = quaternion_integrate(gyroscope_only, gx, gy, gz, STEP);

        // The filter: carry the attitude forward with the gyroscope, then let
        // the accelerometer pull it back to where down really is.
        quaternion_t carried = quaternion_integrate(
            quaternion_make(matrix_get_element(ukf_get_state_matrix(&ukf), 0, 0),
                            matrix_get_element(ukf_get_state_matrix(&ukf), 1, 0),
                            matrix_get_element(ukf_get_state_matrix(&ukf), 2, 0),
                            matrix_get_element(ukf_get_state_matrix(&ukf), 3, 0)),
            gx, gy, gz, STEP);

        matrix_add_element(&state, 0, 0, carried.w);
        matrix_add_element(&state, 1, 0, carried.x);
        matrix_add_element(&state, 2, 0, carried.y);
        matrix_add_element(&state, 3, 0, carried.z);
        ukf_set_state_matrix(&ukf, &state);

        real_t ax;
        real_t ay;
        real_t az;
        read_accelerometer(index, &ax, &ay, &az);

        matrix_add_element(&reading, 0, 0, ax);
        matrix_add_element(&reading, 1, 0, ay);
        matrix_add_element(&reading, 2, 0, az);

        if(!ukf_step(&ukf, NULL, &reading))
        {
            printf("  the filter met a spread it could not use at %u\n", index);
            break;
        }

        if(((index + 1u) % 500u) == 0u)
        {
            quaternion_t truth = true_attitude(index);
            quaternion_t from_filter = quaternion_normalise(
                quaternion_make(
                    matrix_get_element(ukf_get_state_matrix(&ukf), 0, 0),
                    matrix_get_element(ukf_get_state_matrix(&ukf), 1, 0),
                    matrix_get_element(ukf_get_state_matrix(&ukf), 2, 0),
                    matrix_get_element(ukf_get_state_matrix(&ukf), 3, 0)));

            printf("%7.1f s %7.2f deg %7.2f deg %7.2f deg %7.2f deg\n",
                   (real_t)(index + 1u) * STEP,
                   how_far_apart_in_tilt(truth, gyroscope_only),
                   how_far_apart(truth, gyroscope_only),
                   how_far_apart_in_tilt(truth, from_filter),
                   how_far_apart(truth, from_filter));
        }
    }

    printf("\nREAD THE TILT COLUMN. That is the part an accelerometer can\n");
    printf("answer, and it is the part that matters for knowing which way is\n");
    printf("up. The gyroscope alone walks away and never comes back, because\n");
    printf("its small steady error is added up with every reading. The filter\n");
    printf("holds, because the accelerometer keeps telling it where down is.\n\n");

    printf("The other column grows for BOTH, and that is not a fault of the\n");
    printf("filter. An accelerometer sees gravity and nothing else, thus it\n");
    printf("says nothing whatever about which way the board faces about the\n");
    printf("vertical. Turning the board on the spot changes the attitude and\n");
    printf("changes nothing the accelerometer sees. Nothing in this example\n");
    printf("can hold that part, and a magnetometer is what a real device adds\n");
    printf("for it.\n\n");

    printf("Neither sensor could do even the tilt alone. The accelerometer\n");
    printf("cannot follow a fast movement without the knocks going in with\n");
    printf("it, and the gyroscope cannot say which way is down at all. What\n");
    printf("each one lacks, the other has.\n");

    matrix_free(&state);
    matrix_free(&covariance);
    matrix_free(&process);
    matrix_free(&measurement);
    matrix_free(&reading);
    ukf_free(&ukf);

    return 0;
}

#endif//RUN_EXAMPLE
