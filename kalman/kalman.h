#ifndef __KALMAN_H__
#define __KALMAN_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <matrix/matrix.h>
#else
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
        float* mempool;             // Start of the memory that holds all the matrices
        bool singular;              // The last update found a singular matrix
        bool dynamic_alloc;
}kalman_t;

kalman_t kalman_alloc(uint32_t ni, uint32_t nx, uint32_t ny);
kalman_t kalman_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, float* mempool);

void kalman_set_state_matrix(kalman_t* kalman, matrix_t* state_matrix);
void kalman_set_state_transition_matrix(kalman_t* kalman, matrix_t* state_transition_matrix);
void kalman_set_control_matrix(kalman_t* kalman, matrix_t* control_matrix);
void kalman_set_covariance_matrix(kalman_t* kalman, matrix_t* covariance_matrix);
void kalman_set_process_noise_covariance_matrix(kalman_t* kalman, matrix_t* process_noise_covariance);
void kalman_set_measurement_covariance_matrix(kalman_t* kalman, matrix_t* measurement_covariance);
void kalman_set_observation_matrix(kalman_t* kalman, matrix_t* observation_matrix);
void kalman_set_input_matrix(kalman_t* kalman, matrix_t* input_matrix);
void kalman_set_measurement_matrix(kalman_t* kalman, matrix_t* measurement_matrix);

void kalman_predict(kalman_t* kalman);
bool kalman_update(kalman_t* kalman);
bool kalman_step(kalman_t* kalman, matrix_t* input_matrix, matrix_t* measurement_matrix);

matrix_t* kalman_get_state_matrix(kalman_t* kalman);
matrix_t* kalman_get_covariance_matrix(kalman_t* kalman);
matrix_t* kalman_get_gain_matrix(kalman_t* kalman);

void kalman_free(kalman_t* kalman);

#endif//__KALMAN_H__
