#ifndef __KALMAN_H__
#define __KALMAN_H__

#include <matrix/matrix.h>

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
        matrix_t q;                 // Process noise covariance matrix (nx x ny)
        matrix_t r;                 // Measurement covariance matrix (ny x ny)
        matrix_t c;                 // Observation matrix (ny x nx)
        matrix_t k;                 // Kalman gain matrix (nx x ny)
}kalman_t;

#endif//__KALMAN_H__