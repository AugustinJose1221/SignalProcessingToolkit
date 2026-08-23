# ukf

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The unscented Kalman filter. Declared in `sptk/estimate/ukf.h`.

[Back to the index](../API.md) | [How the estimate modules work](../../sptk/estimate/README.md)

## Macros

### `UKF_POINT_COUNT`

```c
#define UKF_POINT_COUNT(nx)             ((2u*(nx)) + 1u)
```

How many points the filter places for a state of the given size.

### `UKF_MEMPOOL_SIZE`

```c
#define UKF_MEMPOOL_SIZE(ni, nx, ny)    ((5*(nx)*(nx)) + (3*(nx)*(ny)) \
```

The number of float elements that ukf_static_alloc needs in the memory pool.
Counted from what ukf_build_matrices really takes, in the same order:
  nx by nx : p, q, factor, nxnx_a, nxnx_b
  nx by ny : k, nxny_a, nxny_b
  ny by ny : r, nyny_a, nyny_b, and the augmented matrix, which is ny by 2ny
  points   : the points and where they moved to, the same for the
             measurement, and the two lists of weights
  columns  : x and four working columns, y and four more, and the input

### `UKF_DEFAULT_ALPHA`

```c
#define UKF_DEFAULT_ALPHA               REAL_C(0.001)
```

### `UKF_DEFAULT_ALPHA`

```c
#define UKF_DEFAULT_ALPHA               REAL_C(0.1)
```

### `UKF_MIN_SPREAD`

```c
#define UKF_MIN_SPREAD                  (REAL_C(1000.0) * REAL_EPSILON)
```

The smallest spreading that the width can carry.

The weights are about 1 divided by this, thus a sum of them loses about
REAL_EPSILON divided by this of its meaning. A thousand steps of the number
keeps that loss near a thousandth.

### `UKF_DEFAULT_BETA`

```c
#define UKF_DEFAULT_BETA                REAL_C(2.0)
```

What is known about the shape of the spread. 2 is best for a normal one.

### `UKF_DEFAULT_KAPPA`

```c
#define UKF_DEFAULT_KAPPA               REAL_C(0.0)
```

The second spreading number.

## Types

### `ukf_scratch_t`

Scratch matrices. The filter holds its intermediate results here, thus it
gets no memory while it runs.

```c
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
```

### `ukf_t`

```c
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
```

## Functions

### `ukf_alloc`

```c
ukf_t ukf_alloc(uint32_t ni, uint32_t nx, uint32_t ny);
```

Give a filter for the given number of inputs, elements of the state, and
elements of the measurement. The memory comes from the heap. Give the filter
to ukf_free when you no longer need it.

The three numbers that place the points are set to their usual values.

### `ukf_static_alloc`

```c
ukf_t ukf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, real_t* mempool);
```

Give a filter that uses the memory at mempool. That memory must hold as many
float values as UKF_MEMPOOL_SIZE gives for the same three sizes. This
function takes no memory from the heap.

### `ukf_set_state_function`

```c
void ukf_set_state_function(ukf_t* ukf, ukf_state_function_t function);
```

Set the function that carries the state forward.

### `ukf_set_measurement_function`

```c
void ukf_set_measurement_function(ukf_t* ukf, ukf_measurement_function_t function);
```

Set the function that says what a state would measure.

### `ukf_is_valid_spread`

```c
bool ukf_is_valid_spread(uint32_t nx, real_t alpha, real_t kappa);
```

True if a state of the given size can hold these numbers at the width of
this build.

The spreading is alpha squared times nx plus kappa, and the weights are
about 1 divided by it. Below UKF_MIN_SPREAD those weights are so large that
their sum, which is 1, is lost in the rounding.

### `ukf_set_spread`

```c
bool ukf_set_spread(ukf_t* ukf, real_t alpha, real_t beta, real_t kappa);
```

Set how the points are placed. The header says what each number does.

Give false if ukf_is_valid_spread is false for these numbers, and then the
filter keeps the ones it had.

### `ukf_set_state_matrix`

```c
void ukf_set_state_matrix(ukf_t* ukf, matrix_t* state_matrix);
```

Set where the state stands now. The matrix is copied.

### `ukf_set_covariance_matrix`

```c
void ukf_set_covariance_matrix(ukf_t* ukf, matrix_t* covariance_matrix);
```

Set how far the state spreads, and how its parts lean on each other. The
matrix is copied, and it must be a real spread: symmetric, and positive in
every direction.

### `ukf_set_process_noise_covariance_matrix`

```c
void ukf_set_process_noise_covariance_matrix(ukf_t* ukf, matrix_t* process_noise);
```

Set how much the state can do that the model does not describe. The matrix
is copied.

### `ukf_set_measurement_covariance_matrix`

```c
void ukf_set_measurement_covariance_matrix(ukf_t* ukf, matrix_t* measurement_noise);
```

Set how much the measurement is wrong by. The matrix is copied.

### `ukf_set_input_matrix`

```c
void ukf_set_input_matrix(ukf_t* ukf, matrix_t* input_matrix);
```

Set what drives the state from outside. The matrix is copied.

### `ukf_set_measurement_matrix`

```c
void ukf_set_measurement_matrix(ukf_t* ukf, matrix_t* measurement_matrix);
```

Set what has just been measured. The matrix is copied.

### `ukf_place_points_into`

```c
bool ukf_place_points_into(ukf_t* ukf, matrix_t* dest);
```

Place the points for the state as it stands, and write them into the
destination, which must have the order nx x (2nx+1).

This is worth looking at when a filter behaves oddly. The points ARE what
the filter knows about the state, and a set that has collapsed together or
spread absurdly wide says where the trouble is.

Give false if the covariance is no longer a real spread.

### `ukf_predict`

```c
bool ukf_predict(ukf_t* ukf);
```

Carry the state forward through the model.

Give false if the covariance is no longer a real spread, and then nothing is
changed.

### `ukf_update`

```c
bool ukf_update(ukf_t* ukf);
```

Correct the state with the measurement that ukf_set_measurement_matrix holds.

Give false if the covariance is no longer a real spread, or if the spread of
the measurement cannot be inverted, and then nothing is changed.

### `ukf_step`

```c
bool ukf_step(ukf_t* ukf, matrix_t* input_matrix, matrix_t* measurement_matrix);
```

Carry the state forward and then correct it, which is one whole step.

Give NULL for either matrix to keep the one the filter already holds.

### `ukf_get_state_matrix`

```c
matrix_t* ukf_get_state_matrix(ukf_t* ukf);
```

Give where the filter believes the state stands now.

### `ukf_get_covariance_matrix`

```c
matrix_t* ukf_get_covariance_matrix(ukf_t* ukf);
```

Give how far the filter believes the state spreads. A spread that grows
where readings are arriving says the filter is losing what it knew.

### `ukf_get_gain_matrix`

```c
matrix_t* ukf_get_gain_matrix(ukf_t* ukf);
```

Give the gain of the last step, which is how far the state moved for each
unit that the measurement differed from what was expected.

### `ukf_free`

```c
void ukf_free(ukf_t* ukf);
```

Release the memory of a filter that came from ukf_alloc. This function does
nothing for a filter that came from ukf_static_alloc.
