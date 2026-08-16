# ekf

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The extended Kalman filter. Declared in `ekf/ekf.h`.

[Back to the index](../API.md)

## Macros

### `EKF_MEMPOOL_SIZE`

```c
#define EKF_MEMPOOL_SIZE(ni, nx, ny)    ((6*(nx)*(nx)) + (5*(nx)*(ny)) + (6*(ny)*(ny)) \
```

The number of float elements that ekf_static_alloc needs in the memory pool.

### `EKF_DEFAULT_DERIVATIVE_STEP`

```c
#define EKF_DEFAULT_DERIVATIVE_STEP     0.001f
```

The step of the central difference that the filter uses when the caller sets
no other one.

## Types

### `ekf_scratch_t`

Scratch matrices. The filter holds its intermediate results here, thus it
gets no memory while it runs.

```c
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
```

### `ekf_t`

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
        matrix_t a;                 // Jacobian of the state function (nx x nx)
        matrix_t c;                 // Jacobian of the measurement function (ny x nx)
        matrix_t k;                 // Kalman gain matrix (nx x ny)

        ekf_state_function_t state_function;
        ekf_measurement_function_t measurement_function;
        float derivative_step;

        ekf_scratch_t scratch;
        float* mempool;
        bool singular;              // The last update found a singular matrix
        bool dynamic_alloc;
}ekf_t;
```

## Functions

### `ekf_alloc`

```c
ekf_t ekf_alloc(uint32_t ni, uint32_t nx, uint32_t ny);
```

Give a filter for the given sizes. The memory comes from the heap, and every
matrix holds zero. Give the filter to ekf_free when you no longer need it.

### `ekf_static_alloc`

```c
ekf_t ekf_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, float* mempool);
```

Give a filter that uses the memory at mempool. That memory must hold as many
float values as EKF_MEMPOOL_SIZE gives for the same three sizes. This
function takes no memory from the heap.

### `ekf_set_state_function`

```c
void ekf_set_state_function(ekf_t* ekf, ekf_state_function_t function);
```

Set the function that gives the next state. The filter needs this function
before the first predict step.

### `ekf_set_measurement_function`

```c
void ekf_set_measurement_function(ekf_t* ekf, ekf_measurement_function_t function);
```

Set the function that gives the measurement of a state. The filter needs
this function before the first update step.

### `ekf_set_derivative_step`

```c
void ekf_set_derivative_step(ekf_t* ekf, float step);
```

Set the step of the central difference. A larger value suits a state whose
values are large.

### `ekf_set_state_matrix`

```c
void ekf_set_state_matrix(ekf_t* ekf, matrix_t* state_matrix);
```

Set the state of the filter.

### `ekf_set_covariance_matrix`

```c
void ekf_set_covariance_matrix(ekf_t* ekf, matrix_t* covariance_matrix);
```

Set the covariance matrix P, which says how much doubt the state holds.

### `ekf_set_process_noise_covariance_matrix`

```c
void ekf_set_process_noise_covariance_matrix(ekf_t* ekf, matrix_t* process_noise);
```

Set the matrix Q, which says how much noise the model adds at each step.

### `ekf_set_measurement_covariance_matrix`

```c
void ekf_set_measurement_covariance_matrix(ekf_t* ekf, matrix_t* measurement_noise);
```

Set the matrix R, which says how much noise the measurement holds.

### `ekf_set_input_matrix`

```c
void ekf_set_input_matrix(ekf_t* ekf, matrix_t* input_matrix);
```

Set the input of the present step.

### `ekf_set_measurement_matrix`

```c
void ekf_set_measurement_matrix(ekf_t* ekf, matrix_t* measurement_matrix);
```

Set the measurement of the present step.

### `ekf_state_jacobian_into`

```c
void ekf_state_jacobian_into(ekf_t* ekf, matrix_t* dest);
```

Write the Jacobian of the state function at the present state into the
destination, which must have the order nx x nx. The filter calls this
function itself at each predict step, and a caller may call it to examine
the model.

### `ekf_measurement_jacobian_into`

```c
void ekf_measurement_jacobian_into(ekf_t* ekf, matrix_t* dest);
```

Write the Jacobian of the measurement function at the present state into the
destination, which must have the order ny x nx.

### `ekf_predict`

```c
void ekf_predict(ekf_t* ekf);
```

Calculate the state and the covariance before the measurement:

    x = f(x, u)
    P = A*P*A' + Q,  where A is the Jacobian of f at the old state

### `ekf_update`

```c
bool ekf_update(ekf_t* ekf);
```

Correct the state and the covariance with the measurement:

    C = the Jacobian of h at the present state
    S = C*P*C' + R
    K = P*C'*inverse(S)
    x = x + K*(y - h(x))
    P = (I - K*C)*P

Give false if S is singular. The state does not change then.

### `ekf_step`

```c
bool ekf_step(ekf_t* ekf, matrix_t* input_matrix, matrix_t* measurement_matrix);
```

Do one full cycle: set the input and the measurement, then predict, then
update. Give NULL as the input matrix if the model has no input.

### `ekf_get_state_matrix`

```c
matrix_t* ekf_get_state_matrix(ekf_t* ekf);
```

Give the state matrix of the filter. The matrix belongs to the filter, thus
the caller must not release it.

### `ekf_get_covariance_matrix`

```c
matrix_t* ekf_get_covariance_matrix(ekf_t* ekf);
```

Give the covariance matrix of the filter.

### `ekf_get_gain_matrix`

```c
matrix_t* ekf_get_gain_matrix(ekf_t* ekf);
```

Give the gain matrix that the last update calculated.

### `ekf_free`

```c
void ekf_free(ekf_t* ekf);
```

Release the memory of a filter that came from ekf_alloc. This function does
nothing for a filter that came from ekf_static_alloc.
