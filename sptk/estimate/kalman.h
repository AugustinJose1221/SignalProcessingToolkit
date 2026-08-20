#ifndef KALMAN_H
#define KALMAN_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif//TEST

// The number of float elements that kalman_static_alloc needs in the memory
// pool. Give the same three sizes that you give to kalman_static_alloc.
#define KALMAN_MEMPOOL_SIZE(ni, nx, ny)     ((6*(nx)*(nx)) + (5*(nx)*(ny)) + (6*(ny)*(ny)) \
                                            + ((nx)*(ni)) + (4*(nx)) + (3*(ny)) + (ni))

// Scratch matrices. The filter uses these matrices to hold the intermediate
// results of the predict step and of the update step. The filter does not get
// memory while it runs. Thus a static filter needs no heap.
typedef struct{
        matrix_t nxnx_a;            // Intermediate matrix (nx x nx)
        matrix_t nxnx_b;            // Intermediate matrix (nx x nx)
        matrix_t nxnx_c;            // Intermediate matrix (nx x nx)
        matrix_t nxny_a;            // Intermediate matrix (nx x ny)
        matrix_t nxny_b;            // Intermediate matrix (nx x ny)
        matrix_t nynx_a;            // Intermediate matrix (ny x nx)
        matrix_t nyny_a;            // Intermediate matrix (ny x ny)
        matrix_t nyny_b;            // Intermediate matrix (ny x ny)
        matrix_t nyny_c;            // Intermediate matrix (ny x ny)
        matrix_t augmented;         // Work matrix for the inverse (ny x 2ny)
        matrix_t nx1_a;             // Intermediate matrix (nx x 1)
        matrix_t nx1_b;             // Intermediate matrix (nx x 1)
        matrix_t ny1_a;             // Intermediate matrix (ny x 1)
        matrix_t ny1_b;             // Intermediate matrix (ny x 1)
}kalman_scratch_t;

typedef struct{
        uint32_t ni;                // Number of inputs
        uint32_t nx;                // Number of elements in the state estimator matrix
        uint32_t ny;                // Number of elements in the measurement matrix

        matrix_t _x;                // Previous state matrix (nx x 1)
        matrix_t x;                 // State matrix (nx x 1)
        matrix_t y;                 // Measurement matrix (ny x 1)
        matrix_t u;                 // Input matrix (ni x 1)
        matrix_t a;                 // State transition matrix (nx x nx)
        matrix_t b;                 // Control matrix (nx x ni)
        matrix_t p;                 // Covariance matrix (nx x nx)
        matrix_t q;                 // Process noise covariance matrix (nx x nx)
        matrix_t r;                 // Measurement covariance matrix (ny x ny)
        matrix_t c;                 // Observation matrix (ny x nx)
        matrix_t k;                 // Kalman gain matrix (nx x ny)

        kalman_scratch_t scratch;   // Intermediate results
        real_t* mempool;             // Start of the memory that holds all the matrices
        bool singular;              // The last update found a singular matrix
        bool dynamic_alloc;
}kalman_t;

// Give a filter for the given sizes. The parameter ni is the number of inputs,
// nx the number of elements of the state, and ny the number of elements of the
// measurement. All three must be larger than zero.
//
// The memory comes from the heap, and every matrix holds zero. Give the filter
// to kalman_free when you no longer need it.
kalman_t kalman_alloc(uint32_t ni, uint32_t nx, uint32_t ny);

// Give a filter that uses the memory at mempool. That memory must hold as many
// float values as KALMAN_MEMPOOL_SIZE gives for the same three sizes, and it
// must stay while the filter is in use.
//
// This function takes no memory from the heap, and the filter takes none while
// it runs. Thus a target with no heap can use the filter.
kalman_t kalman_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool);

// Set the state of the filter. This function writes the value into the state
// and into the previous state, thus it gives the filter its first value.
void kalman_set_state_matrix(kalman_t* kalman, matrix_t* state_matrix);
// Set the matrix A, which says how the state moves from one step to the next.
void kalman_set_state_transition_matrix(kalman_t* kalman, matrix_t* state_transition_matrix);
// Set the matrix B, which says how the input acts on the state.
void kalman_set_control_matrix(kalman_t* kalman, matrix_t* control_matrix);
// Set the matrix P, which says how much doubt the state holds.
void kalman_set_covariance_matrix(kalman_t* kalman, matrix_t* covariance_matrix);
// Set the matrix Q, which says how much noise the model itself adds at each
// step.
void kalman_set_process_noise_covariance_matrix(kalman_t* kalman, matrix_t* process_noise_covariance);
// Set the matrix R, which says how much noise the measurement holds.
void kalman_set_measurement_covariance_matrix(kalman_t* kalman, matrix_t* measurement_covariance);
// Set the matrix C, which says which part of the state the measurement reads.
void kalman_set_observation_matrix(kalman_t* kalman, matrix_t* observation_matrix);
// Set the matrix u, which holds the input of the present step.
void kalman_set_input_matrix(kalman_t* kalman, matrix_t* input_matrix);
// Set the matrix y, which holds the measurement of the present step.
void kalman_set_measurement_matrix(kalman_t* kalman, matrix_t* measurement_matrix);

// Each set function above copies the values, thus the caller may release its
// own matrix after the call. Each matrix must have the order that the comment
// beside the member of the structure gives.

// Calculate the state and the covariance before the measurement:
//
//     x = A*x + B*u
//     P = A*P*A' + Q
void kalman_predict(kalman_t* kalman);

// Correct the state and the covariance with the measurement:
//
//     S = C*P*C' + R
//     K = P*C'*inverse(S)
//     x = x + K*(y - C*x)
//     P = (I - K*C)*P
//
// Give false if S is singular. The state does not change then, and the member
// singular becomes true.
bool kalman_update(kalman_t* kalman);

// Do one full cycle of the filter: set the input and the measurement, then
// predict, then update.
//
// Give NULL as the input matrix if the model has no control input. The input
// matrix keeps its last value then. Give false if the update could not run.
bool kalman_step(kalman_t* kalman, matrix_t* input_matrix, matrix_t* measurement_matrix);

// Give the state matrix x of the filter.
//
// This function and the two below give a pointer to a matrix inside the
// filter. The matrix belongs to the filter, thus the caller must not release
// it, and the next step of the filter changes its values.
matrix_t* kalman_get_state_matrix(kalman_t* kalman);

// Give the covariance matrix P of the filter.
matrix_t* kalman_get_covariance_matrix(kalman_t* kalman);

// Give the gain matrix K that the last update calculated.
matrix_t* kalman_get_gain_matrix(kalman_t* kalman);

// Release the memory of a filter that came from kalman_alloc. This function
// does nothing for a filter that came from kalman_static_alloc, thus a call
// for either kind is safe. A second call does nothing.
void kalman_free(kalman_t* kalman);

#endif//KALMAN_H
