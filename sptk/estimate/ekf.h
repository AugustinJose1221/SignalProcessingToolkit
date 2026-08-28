#ifndef EKF_H
#define EKF_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif

// The extended Kalman filter.
//
// The Kalman filter in the module kalman works with a model where a matrix
// moves the state forward and another matrix reads the measurement from the
// state. Many real models do not have that shape. A radar gives a distance,
// which is the square root of a sum of squares of the state. A pendulum turns
// with the sine of its angle. Such a model needs a function and not a matrix.
//
// This filter takes two functions:
//
// - the state function f, which gives the next state from the present state
//   and the input;
// - the measurement function h, which gives the measurement that the present
//   state would produce.
//
// The filter still needs a matrix at each step, because the covariance moves
// through a matrix. It gets that matrix from the slope of the function at the
// present state, which is the Jacobian matrix. The filter calculates the
// Jacobian with the central difference: it moves one element of the state a
// little to each side, calls the function two times, and takes the difference.
// Thus the caller writes the two functions only, and writes no derivative.
//
// A note on the module pmatrix: an element of a pmatrix is a function of one
// float. A Jacobian needs a function of the whole state, which holds nx
// values. For that reason this module holds its own type of function and does
// not use pmatrix.
//
// The step of the difference must suit the size of the values of the state. A
// step that is too small loses every digit in a float, and a step that is too
// large gives the slope of the wrong place. The default is 0.001, and
// ekf_set_derivative_step changes it.
//
// The filter takes no memory while it runs. Thus a target with no heap can use
// it, as with the module kalman.

// The state function. It reads the state and the input, and it writes the next
// state into the result. The three matrices have the orders nx x 1, ni x 1 and
// nx x 1.
typedef void (*ekf_state_function_t)(const matrix_t* state, const matrix_t* input,
                                     matrix_t* result);

// The measurement function. It reads the state and writes the measurement that
// this state would produce. The two matrices have the orders nx x 1 and
// ny x 1.
typedef void (*ekf_measurement_function_t)(const matrix_t* state, matrix_t* result);

// The number of float elements that ekf_static_alloc needs in the memory pool.
#define EKF_MEMPOOL_SIZE(ni, nx, ny)    ((6*(nx)*(nx)) + (5*(nx)*(ny)) + (6*(ny)*(ny)) \
                                        + (ni) + (5*(nx)) + (4*(ny)))

// The step of the central difference that the filter uses when the caller sets
// no other one.
#define EKF_DEFAULT_DERIVATIVE_STEP     REAL_C(0.001)

// Scratch matrices. The filter holds its intermediate results here, thus it
// gets no memory while it runs.
// THERE IS NO EKF_reset, AND THERE IS NOTHING MISSING.
//
// Every other filter in this library that carries state has a reset, because
// its state is private and a caller cannot reach it. Here the state and the
// covariance ARE the memory of the filter and both are set by the caller, thus
// putting them back is the reset:
//
//     ekf_set_state_matrix(f, &x);
//     ekf_set_covariance_matrix(f, &p);
//
// Nothing else survives a step. The gain, the innovation and the working
// matrices are all written afresh at every predict and update. Measured, a
// filter driven two hundred steps and then given its first state and
// covariance back answers EXACTLY as a filter that has never run: not nearly,
// but to the last digit at both widths.
//
typedef struct{
        matrix_t nxnx_a;
        matrix_t nxnx_b;
        matrix_t nxnx_c;
        matrix_t nxny_a;
        matrix_t nxny_b;
        matrix_t nynx_a;
        matrix_t nyny_a;
        matrix_t nyny_b;
        matrix_t nyny_c;
        matrix_t augmented;         // Work matrix for the inverse (ny x 2ny)
        matrix_t nx1_a;
        matrix_t nx1_b;
        matrix_t nx1_c;
        matrix_t nx1_d;
        matrix_t ny1_a;
        matrix_t ny1_b;
        matrix_t ny1_c;
}ekf_scratch_t;

typedef struct{
        uint32_t ni;                // Number of inputs
        uint32_t nx;                // Number of elements of the state
        uint32_t ny;                // Number of elements of the measurement

        matrix_t x;                 // State matrix (nx x 1)
        matrix_t y;                 // Measurement matrix (ny x 1)
        matrix_t u;                 // Input matrix (ni x 1)
        matrix_t p;                 // Covariance matrix (nx x nx)
        matrix_t q;                 // Process noise covariance matrix (nx x nx)
        matrix_t r;                 // Measurement covariance matrix (ny x ny)
        matrix_t a;                 // Jacobian of the state function (nx x nx)
        matrix_t c;                 // Jacobian of the measurement function (ny x nx)
        matrix_t k;                 // Kalman gain matrix (nx x ny)

        ekf_state_function_t state_function;
        ekf_measurement_function_t measurement_function;
        real_t derivative_step;

        ekf_scratch_t scratch;
        real_t* mempool;
        bool singular;              // The last update found a singular matrix
        bool dynamic_alloc;
}ekf_t;

// Give a filter for the given sizes. The memory comes from the heap, and every
// matrix holds zero. Give the filter to ekf_free when you no longer need it.
ekf_t ekf_alloc(uint32_t ni, uint32_t nx, uint32_t ny);

// Give a filter that uses the memory at mempool. That memory must hold as many
// float values as EKF_MEMPOOL_SIZE gives for the same three sizes. This
// function takes no memory from the heap.
ekf_t ekf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool);

// Set the function that gives the next state. The filter needs this function
// before the first predict step.
void ekf_set_state_function(ekf_t* ekf, ekf_state_function_t function);

// Set the function that gives the measurement of a state. The filter needs
// this function before the first update step.
void ekf_set_measurement_function(ekf_t* ekf, ekf_measurement_function_t function);

// Set the step of the central difference. A larger value suits a state whose
// values are large.
void ekf_set_derivative_step(ekf_t* ekf, real_t step);

// Set the state of the filter.
void ekf_set_state_matrix(ekf_t* ekf, matrix_t* state_matrix);

// Set the covariance matrix P, which says how much doubt the state holds.
void ekf_set_covariance_matrix(ekf_t* ekf, matrix_t* covariance_matrix);

// Set the matrix Q, which says how much noise the model adds at each step.
void ekf_set_process_noise_covariance_matrix(ekf_t* ekf, matrix_t* process_noise);

// Set the matrix R, which says how much noise the measurement holds.
void ekf_set_measurement_covariance_matrix(ekf_t* ekf, matrix_t* measurement_noise);

// Set the input of the present step.
void ekf_set_input_matrix(ekf_t* ekf, matrix_t* input_matrix);

// Set the measurement of the present step.
void ekf_set_measurement_matrix(ekf_t* ekf, matrix_t* measurement_matrix);

// Write the Jacobian of the state function at the present state into the
// destination, which must have the order nx x nx. The filter calls this
// function itself at each predict step, and a caller may call it to examine
// the model.
void ekf_state_jacobian_into(ekf_t* ekf, matrix_t* dest);

// Write the Jacobian of the measurement function at the present state into the
// destination, which must have the order ny x nx.
void ekf_measurement_jacobian_into(ekf_t* ekf, matrix_t* dest);

// Calculate the state and the covariance before the measurement:
//
//     x = f(x, u)
//     P = A*P*A' + Q,  where A is the Jacobian of f at the old state
void ekf_predict(ekf_t* ekf);

// Correct the state and the covariance with the measurement:
//
//     C = the Jacobian of h at the present state
//     S = C*P*C' + R
//     K = P*C'*inverse(S)
//     x = x + K*(y - h(x))
//     P = (I - K*C)*P
//
// Give false if S is singular. The state does not change then.
bool ekf_update(ekf_t* ekf);

// Do one full cycle: set the input and the measurement, then predict, then
// update. Give NULL as the input matrix if the model has no input.
bool ekf_step(ekf_t* ekf, matrix_t* input_matrix, matrix_t* measurement_matrix);

// Give the state matrix of the filter. The matrix belongs to the filter, thus
// the caller must not release it.
matrix_t* ekf_get_state_matrix(ekf_t* ekf);

// Give the covariance matrix of the filter.
matrix_t* ekf_get_covariance_matrix(ekf_t* ekf);

// Give the gain matrix that the last update calculated.
matrix_t* ekf_get_gain_matrix(ekf_t* ekf);

// Release the memory of a filter that came from ekf_alloc. This function does
// nothing for a filter that came from ekf_static_alloc.
void ekf_free(ekf_t* ekf);

#endif//EKF_H
