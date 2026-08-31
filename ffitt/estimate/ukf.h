#ifndef UKF_H
#define UKF_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif

// The unscented Kalman filter: following a state through a model that bends,
// without ever taking a derivative.
//
// WHAT IT ANSWERS THAT THE EXTENDED FILTER DOES NOT
//
// The plain Kalman filter is exact when the model is straight. Most models are
// not. The extended filter answers that by laying a straight line against the
// model at the point where the state stands now, and then running the plain
// filter on that line. It works while the model bends gently over the width of
// the spread.
//
// It fails when the model bends sharply, and it fails in a particular way that
// is worth knowing: A STRAIGHT LINE THROUGH THE MIDDLE OF A SPREAD GIVES BACK
// A SPREAD WHOSE MIDDLE IS WRONG. Put a spread through a bend and its middle
// moves, and a straight line cannot show that at all.
//
// Measured, a spread put through a square, where the true middle of what comes
// out is the middle squared plus the spread:
//
//     middle in   spread in   truth   this filter   a straight line
//       0.0         9.0        9.0        9.0             0.0
//       1.0         4.0        5.0        5.0             1.0
//       3.0         1.0       10.0       10.0             9.0
//
// This filter is exact there. A straight line misses the spread entirely, and
// at a middle of zero it reports nothing at all where the answer is nine.
//
// It takes no derivative to do it. It puts a handful of chosen points through
// the model ITSELF and looks at where they land. The points are placed so that
// they carry the middle and the spread of the state exactly, thus where they
// land carries the middle and the spread AFTER the model, bend and all.
//
// WHAT THIS DOES NOT MEAN
//
// It does not mean this filter beats the extended one at everything. For a
// model that is smooth and a run with many measurements, both settle to the
// same answer, and the extended filter often gets there with slightly less
// work. Measured on a state that does not move, seen through a square over
// sixty readings, the two ended within 3 percent of each other and the
// extended one was marginally the closer.
//
// The gain is in ONE step through a bend, which is what matters when the
// readings are few, when the model is run far forward between them, or when
// the bend is sharp enough that a straight line is not merely less accurate
// but wrong. And it is in needing no derivative at all.
//
// WHEN TO TAKE WHICH
//
//   the model is straight        kalman. Exact, and the cheapest.
//   it bends gently              ekf. One Jacobian, less work than this.
//   it bends sharply             ukf.
//   the derivative is awkward    ukf. It needs none, thus a model that is a
//                                table, a piece of code with a condition in
//                                it, or anything else that cannot be
//                                differentiated is no trouble.
//
// The last line is often the real reason. The extended filter of this library
// works its Jacobians out by a central difference, which needs the model to be
// smooth and needs a step chosen for it. This filter needs neither.
//
// WHAT IT COSTS. The model is run 2*nx+1 times for each step where the
// extended filter runs it about 2*nx times to make its Jacobians, thus the
// work is similar. The memory is more, because the points must be held.
//
// THE THREE NUMBERS THAT PLACE THE POINTS
//
// ALPHA says how far out the points are spread, from a very small number up to
// 1. Small keeps them near the middle, which suits a model that bends sharply
// near where the state stands.
//
// HOW SMALL ALPHA MAY BE DEPENDS ON THE WIDTH OF THE BUILD, and this is not a
// detail. The weights of the points are about 1/(alpha squared times nx) in
// size, and they must add up to 1. A small alpha therefore makes a set of very
// large weights that add to a very small number, and everything the filter
// works out is that sum.
//
// Measured, for nx of 3, what the weights really add up to:
//
//     alpha      0.001    0.010    0.050    0.100    0.500
//     32 bits    1.0625   1.0000   0.9999   1.0000   1.0000
//     64 bits    1.0000   1.0000   1.0000   1.0000   1.0000
//
// At 32 bits and an alpha of 0.001 the weights are 6 percent wrong before the
// filter has done anything at all, and every mean and every spread it works
// out afterwards carries that. The literature gives 0.001 as the usual choice
// because it assumes a wide number.
//
// UKF_DEFAULT_ALPHA therefore follows the width: 0.001 at 64 bits and 0.1 at
// 32. ukf_is_valid_spread says whether a given alpha can be held.
//
// BETA carries what is known about the shape of the spread. For a spread that
// follows a normal law, 2 is the best value, and that is UKF_DEFAULT_BETA.
//
// KAPPA is a second spreading number. 0 suits most work, and 3-nx is the other
// value that is often used.
//
// A CALLER WHO DOES NOT WANT TO CHOOSE should leave all three alone. They are
// set to the usual values at allocation and the filter works.
//
// WHEN IT REFUSES
//
// The points are placed using the factor of Cholesky of the covariance, thus
// the covariance must stay a real spread: symmetric, and positive in every
// direction.
//
// The filter holds the first half of that itself. A covariance is symmetric in
// principle, and the arithmetic that builds it does not know that, thus its two
// halves drift apart in their last digits. Every step therefore averages each
// pair across the diagonal, which costs one pass and holds the invariant.
// Without it a filter of four states seen through three measurements ran for
// three hundred steps at 32 bits and stopped at 64, because the wider build
// notices a smaller drift.
//
// The second half it cannot hold. A covariance can lose its positive spread
// through a long chain of arithmetic, and when it does this filter says so
// rather than carrying on with points that mean nothing. ukf_predict and
// ukf_update both give false then, and that is the first sign that something
// upstream has gone wrong.

// The state function. It reads the state and the input and writes the state
// that follows. The three matrices have the orders nx x 1, ni x 1 and nx x 1.
//
// This is the same shape as the state function of the ekf module, thus a model
// written for one filter can be given to the other unchanged.
typedef void (*ukf_state_function_t)(const matrix_t* state,
                                     const matrix_t* input, matrix_t* result);

// The measurement function. It reads the state and writes the measurement that
// this state would produce. The two matrices have the orders nx x 1 and ny x 1.
typedef void (*ukf_measurement_function_t)(const matrix_t* state,
                                           matrix_t* result);

// How many points the filter places for a state of the given size.
#define UKF_POINT_COUNT(nx)             ((2u*(nx)) + 1u)

// The number of float elements that ukf_static_alloc needs in the memory pool.
// Counted from what ukf_build_matrices really takes, in the same order:
//   nx by nx : p, q, factor, nxnx_a, nxnx_b
//   nx by ny : k, nxny_a, nxny_b, and nynx_a, which is ny by nx and holds the
//              same number of elements
//   ny by ny : r, nyny_a, nyny_b, and the augmented matrix, which is ny by 2ny
//   points   : the points and where they moved to, the same for the
//              measurement, and the two lists of weights
//   columns  : x and four working columns, y and four more, and the input
#define UKF_MEMPOOL_SIZE(ni, nx, ny)    ((5*(nx)*(nx)) + (4*(nx)*(ny)) \
                                        + (3*(ny)*(ny)) + (2*(ny)*(ny)) \
                                        + (2*(nx)*UKF_POINT_COUNT(nx)) \
                                        + (2*(ny)*UKF_POINT_COUNT(nx)) \
                                        + (2*UKF_POINT_COUNT(nx)) \
                                        + (ni) + (5*(nx)) + (5*(ny)))

// How far out the points are spread, when the caller sets no other value. It
// follows the width of the build, for the reason set out above.
#if defined(FFITT_REAL_64)
#define UKF_DEFAULT_ALPHA               REAL_C(0.001)
#else
#define UKF_DEFAULT_ALPHA               REAL_C(0.1)
#endif

// The smallest spreading that the width can carry.
//
// The weights are about 1 divided by this, thus a sum of them loses about
// REAL_EPSILON divided by this of its meaning. A thousand steps of the number
// keeps that loss near a thousandth.
#define UKF_MIN_SPREAD                  (REAL_C(1000.0) * REAL_EPSILON)

// What is known about the shape of the spread. 2 is best for a normal one.
#define UKF_DEFAULT_BETA                REAL_C(2.0)

// The second spreading number.
#define UKF_DEFAULT_KAPPA               REAL_C(0.0)

// Scratch matrices. The filter holds its intermediate results here, thus it
// gets no memory while it runs.
// THERE IS NO UKF_reset, AND THERE IS NOTHING MISSING.
//
// Every other filter in this library that carries state has a reset, because
// its state is private and a caller cannot reach it. Here the state and the
// covariance ARE the memory of the filter and both are set by the caller, thus
// putting them back is the reset:
//
//     ukf_set_state_matrix(f, &x);
//     ukf_set_covariance_matrix(f, &p);
//
// Nothing else survives a step. The gain, the innovation and the working
// matrices are all written afresh at every predict and update. Measured, a
// filter driven two hundred steps and then given its first state and
// covariance back answers EXACTLY as a filter that has never run: not nearly,
// but to the last digit at both widths.
//
typedef struct{
        matrix_t points;            // Where the points stand (nx x 2nx+1)
        matrix_t seen;              // What each point would measure (ny x 2nx+1)
        matrix_t weight_mean;       // The weight of each point for a middle
        matrix_t weight_spread;     // The weight of each point for a spread
        matrix_t factor;            // The factor of Cholesky (nx x nx)
        matrix_t nxnx_a;
        matrix_t nxnx_b;
        matrix_t moved;             // Where each point went (nx x 2nx+1)
        matrix_t nxny_a;
        matrix_t nxny_b;
        matrix_t nynx_a;            // The gain turned round (ny x nx)
        matrix_t nyny_a;
        matrix_t nyny_b;
        matrix_t measured;          // What each point measured (ny x 2nx+1)
        matrix_t augmented;         // Work matrix for the inverse (ny x 2ny)
        matrix_t nx1_a;
        matrix_t nx1_b;
        matrix_t nx1_c;
        matrix_t nx1_d;
        matrix_t ny1_a;
        matrix_t ny1_b;
        matrix_t ny1_c;
        matrix_t ny1_d;
}ukf_scratch_t;

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
        matrix_t k;                 // Kalman gain matrix (nx x ny)

        ukf_state_function_t state_function;
        ukf_measurement_function_t measurement_function;

        real_t alpha;               // How far out the points are spread
        real_t beta;                // What is known about the shape
        real_t kappa;               // The second spreading number

        ukf_scratch_t scratch;
        real_t* mempool;
        bool singular;              // The last step met a matrix it could not use
        bool dynamic_alloc;
}ukf_t;

// Give a filter for the given number of inputs, elements of the state, and
// elements of the measurement. The memory comes from the heap. Give the filter
// to ukf_free when you no longer need it.
//
// The three numbers that place the points are set to their usual values.
ukf_t ukf_alloc(uint32_t ni, uint32_t nx, uint32_t ny);

// Give a filter that uses the memory at mempool. That memory must hold as many
// float values as UKF_MEMPOOL_SIZE gives for the same three sizes. This
// function takes no memory from the heap.
ukf_t ukf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool);

// Set the function that carries the state forward.
void ukf_set_state_function(ukf_t* ukf, ukf_state_function_t function);

// Set the function that says what a state would measure.
void ukf_set_measurement_function(ukf_t* ukf, ukf_measurement_function_t function);

// True if a state of the given size can hold these numbers at the width of
// this build.
//
// The spreading is alpha squared times nx plus kappa, and the weights are
// about 1 divided by it. Below UKF_MIN_SPREAD those weights are so large that
// their sum, which is 1, is lost in the rounding.
bool ukf_is_valid_spread(uint32_t nx, real_t alpha, real_t kappa);

// Set how the points are placed. The header says what each number does.
//
// Give false if ukf_is_valid_spread is false for these numbers, and then the
// filter keeps the ones it had.
bool ukf_set_spread(ukf_t* ukf, real_t alpha, real_t beta, real_t kappa);

// Set where the state stands now. The matrix is copied.
void ukf_set_state_matrix(ukf_t* ukf, matrix_t* state_matrix);

// Set how far the state spreads, and how its parts lean on each other. The
// matrix is copied, and it must be a real spread: symmetric, and positive in
// every direction.
void ukf_set_covariance_matrix(ukf_t* ukf, matrix_t* covariance_matrix);

// Set how much the state can do that the model does not describe. The matrix
// is copied.
void ukf_set_process_noise_covariance_matrix(ukf_t* ukf, matrix_t* process_noise);

// Set how much the measurement is wrong by. The matrix is copied.
void ukf_set_measurement_covariance_matrix(ukf_t* ukf, matrix_t* measurement_noise);

// Set what drives the state from outside. The matrix is copied.
void ukf_set_input_matrix(ukf_t* ukf, matrix_t* input_matrix);

// Set what has just been measured. The matrix is copied.
void ukf_set_measurement_matrix(ukf_t* ukf, matrix_t* measurement_matrix);

// Place the points for the state as it stands, and write them into the
// destination, which must have the order nx x (2nx+1).
//
// This is worth looking at when a filter behaves oddly. The points ARE what
// the filter knows about the state, and a set that has collapsed together or
// spread absurdly wide says where the trouble is.
//
// Give false if the covariance is no longer a real spread.
bool ukf_place_points_into(ukf_t* ukf, matrix_t* dest);

// Carry the state forward through the model.
//
// Give false if the covariance is no longer a real spread, and then nothing is
// changed.
bool ukf_predict(ukf_t* ukf);

// Correct the state with the measurement that ukf_set_measurement_matrix holds.
//
// Give false if the covariance is no longer a real spread, or if the spread of
// the measurement cannot be inverted, and then nothing is changed.
bool ukf_update(ukf_t* ukf);

// Carry the state forward and then correct it, which is one whole step.
//
// Give NULL for either matrix to keep the one the filter already holds.
bool ukf_step(ukf_t* ukf, matrix_t* input_matrix, matrix_t* measurement_matrix);

// Give where the filter believes the state stands now.
matrix_t* ukf_get_state_matrix(ukf_t* ukf);

// Give how far the filter believes the state spreads. A spread that grows
// where readings are arriving says the filter is losing what it knew.
matrix_t* ukf_get_covariance_matrix(ukf_t* ukf);

// Give the gain of the last step, which is how far the state moved for each
// unit that the measurement differed from what was expected.
matrix_t* ukf_get_gain_matrix(ukf_t* ukf);

// Release the memory of a filter that came from ukf_alloc. This function does
// nothing for a filter that came from ukf_static_alloc.
void ukf_free(ukf_t* ukf);

#endif//UKF_H
