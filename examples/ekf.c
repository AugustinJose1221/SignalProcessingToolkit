// Follow a state through a model that is not linear.
//
// A radar stands at the point (0, 0). It reads two values: the distance to an
// object and the angle to it. The object moves along a straight line at a
// fixed speed.
//
// Neither reading is a linear function of the state: the distance is the
// square root of a sum of squares, and the angle is an arc tangent. Thus a
// plain Kalman filter cannot take them. The extended filter can, because it
// takes a function.
//
// The radar must read two values and not one. With the distance alone every
// point of a circle around the radar fits the reading, thus the filter could
// never say where on that circle the object is. Two readings together name
// one point.
//
// The state holds four values: the position and the speed in two directions.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_EKF_EXAMPLE)

#include <sptk/estimate/ekf.h>
#include <sptk/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define STEPS       25u

// The object keeps its speed, and the position grows with the speed.
static void move(const matrix_t* state, const matrix_t* input, matrix_t* result)
{
    (void)input;

    float x = matrix_get_element((matrix_t*)state, 0, 0);
    float y = matrix_get_element((matrix_t*)state, 1, 0);
    float speed_x = matrix_get_element((matrix_t*)state, 2, 0);
    float speed_y = matrix_get_element((matrix_t*)state, 3, 0);

    matrix_add_element(result, 0, 0, x + speed_x);
    matrix_add_element(result, 1, 0, y + speed_y);
    matrix_add_element(result, 2, 0, speed_x);
    matrix_add_element(result, 3, 0, speed_y);
}

// The radar reads the distance to the object and the angle to it.
static void radar(const matrix_t* state, matrix_t* result)
{
    float x = matrix_get_element((matrix_t*)state, 0, 0);
    float y = matrix_get_element((matrix_t*)state, 1, 0);

    matrix_add_element(result, 0, 0, sqrtf((x*x) + (y*y)));
    matrix_add_element(result, 1, 0, atan2f(y, x));
}

int main(void)
{
    ekf_t ekf = ekf_alloc(1, 4, 2);

    ekf_set_state_function(&ekf, move);
    ekf_set_measurement_function(&ekf, radar);

    // The filter starts with a guess that is not correct, and with a large
    // doubt in it.
    matrix_t start = matrix_create_zero_matrix(4, 1);
    matrix_add_element(&start, 0, 0, 8.0f);
    matrix_add_element(&start, 1, 0, 8.0f);
    matrix_add_element(&start, 2, 0, 0.5f);
    matrix_add_element(&start, 3, 0, 0.5f);
    ekf_set_state_matrix(&ekf, &start);

    matrix_t covariance = matrix_create_unit_matrix(4);
    matrix_t product = matrix_multiply_scalar(&covariance, 10.0f);
    ekf_set_covariance_matrix(&ekf, &product);

    matrix_t process = matrix_create_unit_matrix(4);
    matrix_t small = matrix_multiply_scalar(&process, 0.01f);
    ekf_set_process_noise_covariance_matrix(&ekf, &small);

    // The distance holds a doubt of about one, and the angle a much smaller
    // one, because an angle is in radians.
    matrix_t noise = matrix_create_zero_matrix(2, 2);
    matrix_add_element(&noise, 0, 0, 1.0f);
    matrix_add_element(&noise, 1, 1, 0.0001f);
    ekf_set_measurement_covariance_matrix(&ekf, &noise);

    printf("A radar at (0,0) reads the distance and the angle to an object.\n");
    printf("The object starts at (10,10) and moves by (1,2) at each step.\n");
    printf("The filter starts at (8,8) with the speed (0.5,0.5), which is wrong.\n\n");
    printf("%6s %9s %9s %9s %9s %9s\n",
           "STEP", "TRUE X", "TRUE Y", "FOUND X", "FOUND Y", "ERROR");

    matrix_t measurement = matrix_alloc(2, 1);

    for(uint32_t step = 1; step <= STEPS; step++)
    {
        float true_x = 10.0f + (1.0f*(float)step);
        float true_y = 10.0f + (2.0f*(float)step);

        matrix_add_element(&measurement, 0, 0,
                           sqrtf((true_x*true_x) + (true_y*true_y)));
        matrix_add_element(&measurement, 1, 0, atan2f(true_y, true_x));
        ekf_step(&ekf, NULL, &measurement);

        if((step % 5) == 0)
        {
            float found_x = matrix_get_element(&ekf.x, 0, 0);
            float found_y = matrix_get_element(&ekf.x, 1, 0);
            float error = sqrtf(((found_x - true_x)*(found_x - true_x))
                                + ((found_y - true_y)*(found_y - true_y)));

            printf("%6u %9.2f %9.2f %9.2f %9.2f %9.3f\n", step,
                   true_x, true_y, found_x, found_y, error);
        }
    }

    printf("\nThe true speed is (1.00, 2.00).\n");
    printf("The filter found (%.2f, %.2f).\n",
           matrix_get_element(&ekf.x, 2, 0), matrix_get_element(&ekf.x, 3, 0));

    matrix_free(&start);
    matrix_free(&covariance);
    matrix_free(&product);
    matrix_free(&process);
    matrix_free(&small);
    matrix_free(&noise);
    matrix_free(&measurement);
    ekf_free(&ekf);

    return 0;
}

#endif
