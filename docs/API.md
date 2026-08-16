# API reference

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

## Modules

- [`matrix`](#matrix) &mdash; Matrices of float values
- [`cnum`](#cnum) &mdash; Complex numbers
- [`cmatrix`](#cmatrix) &mdash; Matrices of complex numbers
- [`pmatrix`](#pmatrix) &mdash; Matrices with a parameter
- [`fft`](#fft) &mdash; The fast Fourier transform
- [`hilbert`](#hilbert) &mdash; The Hilbert transform
- [`hht`](#hht) &mdash; The Hilbert-Huang transform
- [`fir`](#fir) &mdash; Filters with a finite impulse response
- [`iir`](#iir) &mdash; Filters with an infinite impulse response
- [`vector`](#vector) &mdash; Vectors of float values
- [`vector2d`](#vector2d) &mdash; Vectors with two values
- [`cspline`](#cspline) &mdash; Cubic splines
- [`imf`](#imf) &mdash; Intrinsic mode functions
- [`emd`](#emd) &mdash; Empirical mode decomposition
- [`kalman`](#kalman) &mdash; The Kalman filter
- [`binarysearch`](#binarysearch) &mdash; Binary search
- [`peakdetect`](#peakdetect) &mdash; Peak detection
- [`valleydetect`](#valleydetect) &mdash; Valley detection
- [`point2d`](#point2d) &mdash; A point on a plane
- [`callback`](#callback) &mdash; The print callback

---

## matrix

Matrices of float values. Declared in `matrix/matrix.h`.

### Types

#### `matrix_t`

A matrix of float values.

The elements lie in one block, one row after the other. Thus the element at
the row i and the column j lies at the position (i*n)+j.

Two functions give a matrix. matrix_alloc takes the memory from the heap,
and the caller must give the matrix to matrix_free. matrix_static_alloc
takes memory that the caller holds, and matrix_free then does nothing. The
member dynamic_alloc says which of the two made the matrix.

Every operation that gives a new matrix takes memory from the heap. On a
target with no heap, use the operations at the end of this file, which write
into a matrix that the caller holds.

```c
typedef struct{
    uint32_t m;                 // The number of rows
    uint32_t n;                 // The number of columns
    float *elem;                // The elements, one row after the other
    bool dynamic_alloc;         // True if the memory comes from the heap
}matrix_t;
```

### Functions

#### `matrix_alloc`

```c
matrix_t matrix_alloc(uint32_t m, uint32_t n);
```

Give a matrix with m rows and n columns. The memory comes from the heap, and
the elements hold no value yet. Give the matrix to matrix_free when you no
longer need it.

#### `matrix_static_alloc`

```c
matrix_t matrix_static_alloc(uint32_t m, uint32_t n, float* elem);
```

Give a matrix with m rows and n columns that uses the memory at elem. That
memory must hold m*n float values, and it must stay while the matrix is in
use. This function takes no memory from the heap.

#### `matrix_add_element`

```c
void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, float value);
```

Write a value into the matrix at the row i and the column j.

#### `matrix_get_element`

```c
float matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j);
```

Give the value of the matrix at the row i and the column j.

#### `matrix_get_nth_row`

```c
matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index);
```

Give a new matrix with one row that holds the given row of the matrix. Give
the result to matrix_free.

#### `matrix_get_nth_col`

```c
matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index);
```

Give a new matrix with one column that holds the given column of the matrix.
Give the result to matrix_free.

#### `matrix_get_order`

```c
matrix_t matrix_get_order(matrix_t* matrix);
```

Give a new matrix with one row and two columns. The first element is the
number of rows of the matrix, and the second element is the number of
columns. Give the result to matrix_free.

#### `matrix_trace`

```c
float matrix_trace(matrix_t* matrix);
```

Give the sum of the elements on the diagonal. The matrix must be square.

#### `matrix_determinant`

```c
float matrix_determinant(matrix_t* matrix);
```

Give the determinant of the matrix. The matrix must be square.

The calculation uses the rule of the cofactors. The cost of that rule grows
with the factorial of the order, thus a matrix of a large order takes a long
time. Keep the order below 10.

#### `matrix_create_unit_matrix`

```c
matrix_t matrix_create_unit_matrix(uint32_t size);
```

Give a new square matrix that holds 1 on the diagonal and 0 at every other
place. Give the result to matrix_free.

#### `matrix_create_zero_matrix`

```c
matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n);
```

Give a new matrix that holds 0 at every place. Give the result to
matrix_free.

#### `matrix_is_equal`

```c
bool matrix_is_equal(matrix_t* a, matrix_t* b);
```

True if the two matrices have the same order and the same value at every
place.

#### `matrix_is_square`

```c
bool matrix_is_square(matrix_t* matrix);
```

True if the matrix has as many rows as columns.

#### `matrix_is_zero`

```c
bool matrix_is_zero(matrix_t* matrix);
```

True if every element of the matrix is 0.

#### `matrix_is_unit`

```c
bool matrix_is_unit(matrix_t* matrix);
```

True if the matrix holds 1 on the diagonal and 0 at every other place. The
matrix must be square.

#### `matrix_is_multipliable`

```c
bool matrix_is_multipliable(matrix_t* a, matrix_t* b);
```

True if the first matrix can multiply the second one. That asks for as many
columns in the first matrix as rows in the second one.

#### `matrix_add`

```c
matrix_t matrix_add(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the sum of the two matrices. Both matrices must
have the same order. Give the result to matrix_free.

#### `matrix_subtract`

```c
matrix_t matrix_subtract(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the first matrix less the second one. Both
matrices must have the same order. Give the result to matrix_free.

#### `matrix_multiply_scalar`

```c
matrix_t matrix_multiply_scalar(matrix_t* matrix, float scalar);
```

Give a new matrix where each element is the element of the given matrix
multiplied by the scalar. Give the result to matrix_free.

#### `matrix_multiply`

```c
matrix_t matrix_multiply(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the product of the two matrices. The first
matrix must have as many columns as the second one has rows. The result has
as many rows as the first matrix and as many columns as the second one. Give
the result to matrix_free.

#### `matrix_transpose`

```c
matrix_t matrix_transpose(matrix_t* matrix);
```

Give a new matrix where the rows of the given matrix are the columns. Give
the result to matrix_free.

#### `matrix_inverse`

```c
matrix_t matrix_inverse(matrix_t* matrix);
```

Give a new matrix that is the inverse of the given matrix. The matrix must
be square.

The elimination uses a partial pivot, thus a zero on the diagonal does not
stop it. If the matrix is singular it has no inverse, and the function gives
a matrix that holds 0 at every place. Use matrix_is_zero on the result to
find that state. Give the result to matrix_free.

#### `matrix_copy`

```c
void matrix_copy(matrix_t* src, matrix_t* dest);
```

Write the elements of the source into the destination. Both matrices must
have the same order.

#### `matrix_printf`

```c
void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...));
```

Write the matrix, one row for each line. Give NULL as the function to write
with printf.

#### `matrix_free`

```c
void matrix_free(matrix_t* matrix);
```

Release the memory of a matrix that came from matrix_alloc. This function
does nothing for a matrix that came from matrix_static_alloc, thus a call
for either kind is safe. A second call does nothing.

#### `matrix_add_into`

```c
void matrix_add_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the sum of the two matrices into the destination. All three matrices
must have the same order.

#### `matrix_subtract_into`

```c
void matrix_subtract_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the first matrix less the second one into the destination. All three
matrices must have the same order.

#### `matrix_multiply_into`

```c
void matrix_multiply_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the product of the two matrices into the destination. The destination
must have as many rows as the first matrix and as many columns as the second
one.

#### `matrix_multiply_scalar_into`

```c
void matrix_multiply_scalar_into(matrix_t* matrix, float scalar, matrix_t* dest);
```

Write each element of the matrix multiplied by the scalar into the
destination. Both matrices must have the same order.

#### `matrix_transpose_into`

```c
void matrix_transpose_into(matrix_t* matrix, matrix_t* dest);
```

Write the transpose of the matrix into the destination. The destination must
have as many rows as the matrix has columns, and as many columns as the
matrix has rows.

#### `matrix_set_unit`

```c
void matrix_set_unit(matrix_t* matrix);
```

Write 1 on the diagonal of the matrix and 0 at every other place. The matrix
must be square.

#### `matrix_set_zero`

```c
void matrix_set_zero(matrix_t* matrix);
```

Write 0 into every element of the matrix.

#### `matrix_inverse_into`

```c
bool matrix_inverse_into(matrix_t* matrix, matrix_t* dest, matrix_t* scratch);
```

Write the inverse of the matrix into the destination. The matrix must be
square, and the destination must have the same order.

The scratch matrix must have the order n x 2n, where n is the order of the
matrix. It loses its content. The function gives false if the matrix is
singular, and it does not change the destination then.

---

## cnum

Complex numbers. Declared in `cnum/cnum.h`.

### Types

#### `cnum_t`

```c
typedef struct{
    float re;                   // The real part
    float im;                   // The imaginary part
}cnum_t;
```

### Functions

#### `cnum_make`

```c
cnum_t cnum_make(float re, float im);
```

Give a complex number with the given real part and imaginary part.

#### `cnum_from_real`

```c
cnum_t cnum_from_real(float re);
```

Give a complex number whose imaginary part is zero.

#### `cnum_zero`

```c
cnum_t cnum_zero(void);
```

Give the number zero.

#### `cnum_one`

```c
cnum_t cnum_one(void);
```

Give the number one.

#### `cnum_add`

```c
cnum_t cnum_add(cnum_t a, cnum_t b);
```

Give the sum of the two numbers.

#### `cnum_subtract`

```c
cnum_t cnum_subtract(cnum_t a, cnum_t b);
```

Give the first number less the second one.

#### `cnum_multiply`

```c
cnum_t cnum_multiply(cnum_t a, cnum_t b);
```

Give the product of the two numbers.

#### `cnum_divide`

```c
cnum_t cnum_divide(cnum_t a, cnum_t b);
```

Give the quotient of the two numbers. If the second number is zero, the
quotient has no value, and the function gives zero.

#### `cnum_scale`

```c
cnum_t cnum_scale(cnum_t a, float factor);
```

Give the number with both parts multiplied by a real factor.

#### `cnum_conjugate`

```c
cnum_t cnum_conjugate(cnum_t a);
```

Give the number with the sign of the imaginary part changed.

#### `cnum_negate`

```c
cnum_t cnum_negate(cnum_t a);
```

Give the number with the sign of both parts changed.

#### `cnum_real`

```c
float cnum_real(cnum_t a);
```

Give the real part of the number.

#### `cnum_imaginary`

```c
float cnum_imaginary(cnum_t a);
```

Give the imaginary part of the number.

#### `cnum_magnitude`

```c
float cnum_magnitude(cnum_t a);
```

Give the distance of the number from zero.

#### `cnum_magnitude_squared`

```c
float cnum_magnitude_squared(cnum_t a);
```

Give the square of the distance from zero. This function does not take a
square root, thus it is faster than cnum_magnitude and it keeps more of the
accuracy. Use it when you only compare two distances.

#### `cnum_is_zero`

```c
bool cnum_is_zero(cnum_t a);
```

True if both parts of the number are zero.

#### `cnum_is_equal`

```c
bool cnum_is_equal(cnum_t a, cnum_t b);
```

True if both parts of the two numbers are the same.

#### `cnum_is_near`

```c
bool cnum_is_near(cnum_t a, cnum_t b, float tolerance);
```

Give true if the two numbers differ by less than the tolerance. A float
keeps about 7 digits, thus a calculation with several steps gives a result
that is near the correct value but not equal to it.

---

## cmatrix

Matrices of complex numbers. Declared in `cmatrix/cmatrix.h`.

### Types

#### `cmatrix_t`

```c
typedef struct{
    uint32_t m;
    uint32_t n;
    cnum_t *elem;
    bool dynamic_alloc;
}cmatrix_t;
```

### Functions

#### `cmatrix_alloc`

```c
cmatrix_t cmatrix_alloc(uint32_t m, uint32_t n);
```

Give a matrix with m rows and n columns. The memory comes from the heap, and
the elements hold no value yet. Give the matrix to cmatrix_free when you no
longer need it.

#### `cmatrix_static_alloc`

```c
cmatrix_t cmatrix_static_alloc(uint32_t m, uint32_t n, cnum_t* elem);
```

Give a matrix with m rows and n columns that uses the memory at elem. That
memory must hold m*n complex numbers. This function takes no memory from the
heap.

#### `cmatrix_add_element`

```c
void cmatrix_add_element(cmatrix_t* matrix, uint32_t i, uint32_t j, cnum_t value);
```

Write a value into the matrix at the row i and the column j.

#### `cmatrix_get_element`

```c
cnum_t cmatrix_get_element(cmatrix_t* matrix, uint32_t i, uint32_t j);
```

Give the value of the matrix at the row i and the column j.

#### `cmatrix_create_unit_matrix`

```c
cmatrix_t cmatrix_create_unit_matrix(uint32_t size);
```

Give a new square matrix that holds 1 on the diagonal and 0 at every other
place. Give the result to cmatrix_free.

#### `cmatrix_create_zero_matrix`

```c
cmatrix_t cmatrix_create_zero_matrix(uint32_t m, uint32_t n);
```

Give a new matrix that holds 0 at every place. Give the result to
cmatrix_free.

#### `cmatrix_is_equal`

```c
bool cmatrix_is_equal(cmatrix_t* a, cmatrix_t* b);
```

True if the two matrices have the same order and exactly the same value at
every place.

#### `cmatrix_is_near`

```c
bool cmatrix_is_near(cmatrix_t* a, cmatrix_t* b, float tolerance);
```

True if the two matrices have the same order and no pair of values differs
by more than the tolerance. Use this function after a calculation with
several steps, where the result is near the correct value but not equal to
it.

#### `cmatrix_is_square`

```c
bool cmatrix_is_square(cmatrix_t* matrix);
```

True if the matrix has as many rows as columns.

#### `cmatrix_is_zero`

```c
bool cmatrix_is_zero(cmatrix_t* matrix);
```

True if every element of the matrix is 0.

#### `cmatrix_is_unit`

```c
bool cmatrix_is_unit(cmatrix_t* matrix);
```

True if the matrix holds 1 on the diagonal and 0 at every other place. The
matrix must be square.

#### `cmatrix_is_multipliable`

```c
bool cmatrix_is_multipliable(cmatrix_t* a, cmatrix_t* b);
```

True if the first matrix can multiply the second one. That asks for as many
columns in the first matrix as rows in the second one.

#### `cmatrix_is_hermitian`

```c
bool cmatrix_is_hermitian(cmatrix_t* matrix);
```

True if the matrix does not change when the conjugate transpose is taken.
Such a matrix is Hermitian, and its values on the diagonal are all real.

#### `cmatrix_add`

```c
cmatrix_t cmatrix_add(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the sum of the two matrices. Both matrices must
have the same order. Give the result to cmatrix_free.

#### `cmatrix_subtract`

```c
cmatrix_t cmatrix_subtract(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the first matrix less the second one. Both
matrices must have the same order. Give the result to cmatrix_free.

#### `cmatrix_multiply`

```c
cmatrix_t cmatrix_multiply(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the product of the two matrices. The first
matrix must have as many columns as the second one has rows. Give the result
to cmatrix_free.

#### `cmatrix_multiply_scalar`

```c
cmatrix_t cmatrix_multiply_scalar(cmatrix_t* matrix, cnum_t scalar);
```

Give a new matrix where each element is the element of the given matrix
multiplied by the scalar. Give the result to cmatrix_free.

#### `cmatrix_transpose`

```c
cmatrix_t cmatrix_transpose(cmatrix_t* matrix);
```

Give a new matrix where the rows of the given matrix are the columns. This
operation does not change the sign of the imaginary parts. Give the result to
cmatrix_free.

#### `cmatrix_conjugate_transpose`

```c
cmatrix_t cmatrix_conjugate_transpose(cmatrix_t* matrix);
```

The transpose where each element becomes its conjugate. This operation takes
the place of the transpose for a matrix of complex numbers.

#### `cmatrix_trace`

```c
cnum_t cmatrix_trace(cmatrix_t* matrix);
```

Give the sum of the elements on the diagonal. The matrix must be square.

#### `cmatrix_determinant`

```c
cnum_t cmatrix_determinant(cmatrix_t* matrix);
```

Give the determinant of the matrix. The matrix must be square.

The calculation uses elimination with a partial pivot, thus its cost grows
with the third power of the order. The elimination needs a copy of the
matrix, thus this function gets memory from the heap. Use
cmatrix_determinant_into on a target with no heap.

#### `cmatrix_inverse`

```c
cmatrix_t cmatrix_inverse(cmatrix_t* matrix);
```

Give a new matrix that is the inverse of the given matrix. The matrix must
be square.

The elimination uses a partial pivot, thus a zero on the diagonal does not
stop it. If the matrix is singular it has no inverse, and the function gives
a matrix that holds 0 at every place. Use cmatrix_is_zero on the result to
find that state. Give the result to cmatrix_free.

#### `cmatrix_copy`

```c
void cmatrix_copy(cmatrix_t* src, cmatrix_t* dest);
```

Write the elements of the source into the destination. Both matrices must
have the same order.

#### `cmatrix_printf`

```c
void cmatrix_printf(cmatrix_t* matrix, print_t func);
```

Write the matrix, one row for each line, in the form "a + bi". Give NULL as
the function to write with printf.

#### `cmatrix_free`

```c
void cmatrix_free(cmatrix_t* matrix);
```

Release the memory of a matrix that came from cmatrix_alloc. This function
does nothing for a matrix that came from cmatrix_static_alloc, thus a call
for either kind is safe. A second call does nothing.

#### `cmatrix_add_into`

```c
void cmatrix_add_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Operations that write into a matrix that already holds memory.

The destination must have the correct order, and it must not be one of the
sources. These operations get no memory, thus code that must not use the
heap can call them.
Write the sum of the two matrices into the destination. All three matrices
must have the same order.

#### `cmatrix_subtract_into`

```c
void cmatrix_subtract_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Write the first matrix less the second one into the destination. All three
matrices must have the same order.

#### `cmatrix_multiply_into`

```c
void cmatrix_multiply_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Write the product of the two matrices into the destination. The destination
must have as many rows as the first matrix and as many columns as the second
one.

#### `cmatrix_multiply_scalar_into`

```c
void cmatrix_multiply_scalar_into(cmatrix_t* matrix, cnum_t scalar, cmatrix_t* dest);
```

Write each element of the matrix multiplied by the scalar into the
destination. Both matrices must have the same order.

#### `cmatrix_transpose_into`

```c
void cmatrix_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
```

Write the transpose of the matrix into the destination. The destination must
have as many rows as the matrix has columns, and as many columns as the
matrix has rows.

#### `cmatrix_conjugate_transpose_into`

```c
void cmatrix_conjugate_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
```

Write the conjugate transpose of the matrix into the destination. The
destination must have as many rows as the matrix has columns, and as many
columns as the matrix has rows.

#### `cmatrix_set_unit`

```c
void cmatrix_set_unit(cmatrix_t* matrix);
```

Write 1 on the diagonal of the matrix and 0 at every other place. The matrix
must be square.

#### `cmatrix_set_zero`

```c
void cmatrix_set_zero(cmatrix_t* matrix);
```

Write 0 into every element of the matrix.

#### `cmatrix_determinant_into`

```c
cnum_t cmatrix_determinant_into(cmatrix_t* matrix, cmatrix_t* scratch);
```

The elimination writes its steps into the scratch matrix, which must have
the order n x n. The scratch matrix loses its content.

#### `cmatrix_inverse_into`

```c
bool cmatrix_inverse_into(cmatrix_t* matrix, cmatrix_t* dest, cmatrix_t* scratch);
```

The scratch matrix must have the order n x 2n. The function gives false if
the matrix is singular, and it does not change the destination then.

---

## pmatrix

Matrices with a parameter. Declared in `pmatrix/pmatrix.h`.

### Types

#### `pmatrix_t`

```c
typedef struct{
    uint32_t m;
    uint32_t n;
    pmatrix_function_t *elem;
    bool dynamic_alloc;
}pmatrix_t;
```

### Functions

#### `pmatrix_alloc`

```c
pmatrix_t pmatrix_alloc(uint32_t m, uint32_t n);
```

Give a parameter matrix with m rows and n columns. The memory comes from
the heap, and every element holds zero. Give the matrix to pmatrix_free when
you no longer need it.

#### `pmatrix_static_alloc`

```c
pmatrix_t pmatrix_static_alloc(uint32_t m, uint32_t n, pmatrix_function_t* elem);
```

Give a parameter matrix that uses the memory at elem. That memory must hold
m*n pointers to a function. Every element holds zero after the call. This
function takes no memory from the heap.

#### `pmatrix_add_element`

```c
void pmatrix_add_element(pmatrix_t* matrix, uint32_t i, uint32_t j, pmatrix_function_t function);
```

An element that holds NULL gives the value zero. Thus a new matrix that
pmatrix_set_zero cleared holds zero at every place, and a user who needs a
zero at one place does not need a function for it.

#### `pmatrix_get_element`

```c
pmatrix_function_t pmatrix_get_element(pmatrix_t* matrix, uint32_t i, uint32_t j);
```

Give the function that stands at the row i and the column j. The result is
NULL if that element holds zero.

#### `pmatrix_set_zero`

```c
void pmatrix_set_zero(pmatrix_t* matrix);
```

Write zero into every element of the matrix.

#### `pmatrix_evaluate_element`

```c
float pmatrix_evaluate_element(pmatrix_t* matrix, uint32_t i, uint32_t j, float x);
```

Give the value of one element for the given value of the parameter.

#### `pmatrix_evaluate`

```c
matrix_t pmatrix_evaluate(pmatrix_t* matrix, float x);
```

Give a new matrix of float values for the given value of the parameter. This
function gets memory. Use pmatrix_evaluate_into on a target with no heap.

#### `pmatrix_evaluate_into`

```c
void pmatrix_evaluate_into(pmatrix_t* matrix, float x, matrix_t* dest);
```

Write the values into a matrix that already holds memory. The destination
must have the same order as the parameter matrix.

#### `pmatrix_zero`

```c
float pmatrix_zero(float x);
```

An element that always gives zero.

#### `pmatrix_one`

```c
float pmatrix_one(float x);
```

An element that always gives one.

#### `pmatrix_free`

```c
void pmatrix_free(pmatrix_t* matrix);
```

Release the memory of a matrix that came from pmatrix_alloc. This function
does nothing for a matrix that came from pmatrix_static_alloc.

---

## fft

The fast Fourier transform. Declared in `fft/fft.h`.

### Macros

#### `FFT_TWIDDLE_COUNT`

```c
#define FFT_TWIDDLE_COUNT(size)     ((size)/2)
```

The number of turning factors that a transform of the given size needs.

#### `FFT_REVERSE_COUNT`

```c
#define FFT_REVERSE_COUNT(size)     (size)
```

The number of indices of the bit reversal that a transform of the given size
needs.

### Types

#### `fft_t`

```c
typedef struct{
    uint32_t size;              // The number of points, a power of two
    cnum_t* twiddle;            // The turning factors, size/2 of them
    uint32_t* reverse;          // The order of the bit reversal, size of them
    bool dynamic_alloc;         // True if the memory comes from the heap
}fft_t;
```

### Functions

#### `fft_is_valid_size`

```c
bool fft_is_valid_size(uint32_t size);
```

True if the size is a power of two and larger than one. Only such a size
works with this module.

#### `fft_alloc`

```c
fft_t fft_alloc(uint32_t size);
```

Give a transform for the given number of points. The memory comes from the
heap. Give the transform to fft_free when you no longer need it.

#### `fft_static_alloc`

```c
fft_t fft_static_alloc(uint32_t size, cnum_t* twiddle, uint32_t* reverse);
```

Give a transform that uses the memory that the caller holds. The table
twiddle must hold FFT_TWIDDLE_COUNT(size) complex numbers, and the table
reverse must hold FFT_REVERSE_COUNT(size) values. This function takes no
memory from the heap.

#### `fft_forward`

```c
void fft_forward(fft_t* fft, cnum_t* data);
```

Change the given data from the time domain into the frequency domain. The
data must hold as many complex numbers as the size of the transform. The
function writes the result over the data, and it gets no memory.

#### `fft_inverse`

```c
void fft_inverse(fft_t* fft, cnum_t* data);
```

Change the given data from the frequency domain into the time domain. This
operation is the opposite of fft_forward: a forward transform and then an
inverse transform give the first data again. The function writes the result
over the data, and it gets no memory.

#### `fft_forward_real`

```c
void fft_forward_real(fft_t* fft, const float* input, cnum_t* output);
```

Change a signal of float values into the frequency domain.

The function writes each value of the input into the real part of the
output, sets each imaginary part to zero, and then does a forward transform.
The output must hold as many complex numbers as the size of the transform.

A signal of real values gives a result where the second half mirrors the
first half. Thus only the bins from 0 to size/2 hold new information. This
function is not the faster method that uses that mirror. It gives the same
result with less code.

#### `fft_magnitude`

```c
void fft_magnitude(const cnum_t* data, float* magnitude, uint32_t size);
```

Write the size of each element of the data into the magnitude list. The size
of an element says how strong that frequency is in the signal. Both lists
must hold as many values as the given size.

#### `fft_power`

```c
void fft_power(const cnum_t* data, float* power, uint32_t size);
```

Write the square of the size of each element into the power list. This
function takes no square root, thus it is faster than fft_magnitude. Both
lists must hold as many values as the given size.

#### `fft_bin_frequency`

```c
float fft_bin_frequency(uint32_t index, uint32_t size, float sample_rate);
```

Give the frequency in hertz that the bin with the given index holds. The
sample rate is the number of samples in one second.

A bin above size/2 holds a frequency above half the sample rate. Such a bin
mirrors a lower bin, and this function gives the negative frequency for it,
which is the frequency that the mirror holds.

#### `fft_free`

```c
void fft_free(fft_t* fft);
```

Release the memory of a transform that came from fft_alloc. This function
does nothing for a transform that came from fft_static_alloc, thus a call
for either kind is safe. A second call does nothing.

---

## hilbert

The Hilbert transform. Declared in `hilbert/hilbert.h`.

### Functions

#### `hilbert_analytic_signal`

```c
void hilbert_analytic_signal(fft_t* fft, const float* signal, cnum_t* analytic);
```

Give the analytic signal of a real signal.

The signal and the work buffer must hold as many values as the size of the
transform. The function writes the result into the work buffer, thus it gets
no memory.

#### `hilbert_amplitude`

```c
void hilbert_amplitude(const cnum_t* analytic, float* amplitude, uint32_t size);
```

Write the instantaneous amplitude of each point into the amplitude list. The
amplitude follows the envelope of the signal, and it is never less than
zero.

#### `hilbert_phase`

```c
void hilbert_phase(const cnum_t* analytic, float* phase, uint32_t size);
```

Write the instantaneous phase of each point into the phase list. The phase
lies between -pi and pi.

#### `hilbert_frequency`

```c
void hilbert_frequency(const cnum_t* analytic, float* frequency, uint32_t size, float sample_rate);
```

Write the instantaneous frequency of each point into the frequency list.

The frequency comes from the change of the phase between two samples. The
function takes the change into the range from -pi to pi before it makes the
frequency, because the phase itself jumps from pi to -pi.

The list holds one value less than the signal, because a change needs two
points. The caller gives the sample rate in samples for each second, and the
result is in hertz.

---

## hht

The Hilbert-Huang transform. Declared in `hht/hht.h`.

### Functions

#### `hht_transform_imf`

```c
void hht_transform_imf(fft_t* fft, imf_t* imf, cnum_t* work, float* amplitude, float* frequency, float sample_rate);
```

Give the amplitude and the frequency at each point of time, for one
intrinsic mode function.

The function writes size values into the amplitude list, and size-1 values
into the frequency list, because a frequency needs two points of the phase.
The work buffer must hold size complex numbers. The function gets no memory.

The size must be the same as the size of the transform, and it must be a
power of two.

#### `hht_transform`

```c
void hht_transform(fft_t* fft, imf_t* imf, uint32_t count, cnum_t* work, float* amplitude, float* frequency, float sample_rate);
```

Give the amplitude and the frequency for a list of intrinsic mode
functions, one after the other.

The lists amplitude and frequency hold the result of each function one after
the other. Thus the amplitude list must hold count*size values, and the
frequency list must hold count*(size-1) values. The work buffer must hold
size complex numbers.

#### `hht_mean_frequency`

```c
float hht_mean_frequency(const float* amplitude, const float* frequency, uint32_t size);
```

Give the mean frequency of one intrinsic mode function, where each point
counts as much as the square of its amplitude.

A point with a small amplitude holds a phase that noise moves easily. This
mean gives such a point little weight, thus it describes the function better
than a plain mean does.

---

## fir

Filters with a finite impulse response. Declared in `fir/fir.h`.

### Types

#### `fir_t`

```c
typedef struct{
    uint32_t length;            // The number of coefficients
    float* coefficient;         // The coefficients
    float* history;             // The last samples, length of them
    uint32_t position;          // Where the next sample goes in the history
    bool dynamic_alloc;         // True if the memory comes from the heap
}fir_t;
```

### Functions

#### `fir_alloc`

```c
fir_t fir_alloc(uint32_t length);
```

Give a filter with the given number of coefficients. The memory comes from
the heap, and every coefficient and every sample of the history holds zero.
Give the filter to fir_free when you no longer need it.

#### `fir_static_alloc`

```c
fir_t fir_static_alloc(uint32_t length, float* coefficient, float* history);
```

Give a filter that uses the memory that the caller holds. Both lists must
hold as many float values as the given length. This function takes no
memory from the heap.

#### `fir_design_low_pass`

```c
void fir_design_low_pass(fir_t* fir, float cutoff);
```

Build the coefficients of a filter that lets the low frequencies pass. The
cutoff is a part of the sample rate, and it must lie between 0 and 0.5.

#### `fir_design_high_pass`

```c
void fir_design_high_pass(fir_t* fir, float cutoff);
```

Build the coefficients of a filter that lets the high frequencies pass.

#### `fir_design_band_pass`

```c
void fir_design_band_pass(fir_t* fir, float low_cutoff, float high_cutoff);
```

Build the coefficients of a filter that lets a band of frequencies pass. The
low cutoff must be smaller than the high cutoff, and both must lie between 0
and 0.5.

#### `fir_set_coefficient`

```c
void fir_set_coefficient(fir_t* fir, uint32_t index, float value);
```

Write one coefficient. Use this function to give the filter a set of
coefficients that another program calculated.

#### `fir_get_coefficient`

```c
float fir_get_coefficient(fir_t* fir, uint32_t index);
```

Give one coefficient.

#### `fir_process_sample`

```c
float fir_process_sample(fir_t* fir, float sample);
```

Give the filtered value of one sample. The filter keeps the sample in its
history, thus the next call sees it.

#### `fir_process_block`

```c
void fir_process_block(fir_t* fir, const float* input, float* output, uint32_t size);
```

Filter a block of samples. The input and the output may be the same list.

#### `fir_reset`

```c
void fir_reset(fir_t* fir);
```

Set every sample of the history to zero. The filter then behaves as a filter
that has seen no sample yet.

#### `fir_get_gain`

```c
float fir_get_gain(fir_t* fir, float frequency);
```

Give the size of the answer of the filter at the given frequency, which is a
part of the sample rate. A value of 1 says that the frequency passes
unchanged, and a value of 0 says that the filter stops it.

#### `fir_free`

```c
void fir_free(fir_t* fir);
```

Release the memory of a filter that came from fir_alloc. This function does
nothing for a filter that came from fir_static_alloc.

---

## iir

Filters with an infinite impulse response. Declared in `iir/iir.h`.

### Macros

#### `IIR_COEFFICIENT_COUNT`

```c
#define IIR_COEFFICIENT_COUNT       5u
```

The number of coefficients of one section: b0, b1, b2, a1 and a2.

#### `IIR_STATE_COUNT`

```c
#define IIR_STATE_COUNT             2u
```

The number of values of the state of one section.

#### `IIR_COEFFICIENT_SIZE`

```c
#define IIR_COEFFICIENT_SIZE(sections)  ((sections) * IIR_COEFFICIENT_COUNT)
```

The number of float values that a filter with the given number of sections
needs for its coefficients.

#### `IIR_STATE_SIZE`

```c
#define IIR_STATE_SIZE(sections)        ((sections) * IIR_STATE_COUNT)
```

The number of float values that a filter with the given number of sections
needs for its state.

### Types

#### `iir_t`

```c
typedef struct{
    uint32_t sections;          // The number of biquad sections
    float* coefficient;         // Five coefficients for each section
    float* state;               // Two values for each section
    bool dynamic_alloc;         // True if the memory comes from the heap
}iir_t;
```

### Functions

#### `iir_alloc`

```c
iir_t iir_alloc(uint32_t sections);
```

Give a filter with the given number of sections. The memory comes from the
heap. The filter lets everything pass until a design function or
iir_set_section gives it coefficients. Give the filter to iir_free when you
no longer need it.

#### `iir_static_alloc`

```c
iir_t iir_static_alloc(uint32_t sections, float* coefficient, float* state);
```

Give a filter that uses the memory that the caller holds. The list
coefficient must hold IIR_COEFFICIENT_SIZE(sections) float values, and the
list state must hold IIR_STATE_SIZE(sections) of them. This function takes
no memory from the heap.

#### `iir_design_low_pass`

```c
void iir_design_low_pass(iir_t* iir, float cutoff);
```

Build the coefficients of a filter of Butterworth that lets the low
frequencies pass. The order of the filter is two times the number of
sections.

#### `iir_design_high_pass`

```c
void iir_design_high_pass(iir_t* iir, float cutoff);
```

Build the coefficients of a filter of Butterworth that lets the high
frequencies pass.

#### `iir_set_section`

```c
void iir_set_section(iir_t* iir, uint32_t section, float b0, float b1, float b2, float a0, float a1, float a2);
```

Write the five coefficients of one section. The three coefficients b belong
to the input, and the two coefficients a belong to the feedback. The
function divides every coefficient by a0, thus the caller may give the
coefficients as another program calculated them.

#### `iir_process_sample`

```c
float iir_process_sample(iir_t* iir, float sample);
```

Give the filtered value of one sample.

#### `iir_process_block`

```c
void iir_process_block(iir_t* iir, const float* input, float* output, uint32_t size);
```

Filter a block of samples. The input and the output may be the same list.

#### `iir_reset`

```c
void iir_reset(iir_t* iir);
```

Set the state of every section to zero. The filter then behaves as a filter
that has seen no sample yet.

#### `iir_get_gain`

```c
float iir_get_gain(iir_t* iir, float frequency);
```

Give the size of the answer of the filter at the given frequency, which is a
part of the sample rate. A value of 1 says that the frequency passes
unchanged, and a value of 0 says that the filter stops it.

#### `iir_free`

```c
void iir_free(iir_t* iir);
```

Release the memory of a filter that came from iir_alloc. This function does
nothing for a filter that came from iir_static_alloc.

---

## vector

Vectors of float values. Declared in `vector/vector.h`.

### Types

#### `vector_t`

A vector of float values.

Two functions give a vector. vector_alloc takes the memory from the heap,
and the caller must give the vector to vector_free. vector_static_alloc
takes memory that the caller holds, and vector_free then does nothing.

```c
typedef struct{
    uint32_t size;              // The number of values
    float* data;                // The values
    bool dynamic_alloc;         // True if the memory comes from the heap
}vector_t;
```

### Functions

#### `vector_alloc`

```c
vector_t vector_alloc(uint32_t size);
```

Give a vector that holds the given number of values. The memory comes from
the heap, and the values hold nothing yet. Give the vector to vector_free
when you no longer need it.

#### `vector_static_alloc`

```c
vector_t vector_static_alloc(uint32_t size, float* mempool);
```

Give a vector that uses the memory at mempool. That memory must hold as many
float values as the given size, and it must stay while the vector is in use.
This function takes no memory from the heap.

#### `vector_add_point_at_index`

```c
void vector_add_point_at_index(vector_t* vector, uint32_t index, float data);
```

Write a value into the vector at the given index. The index must be below
the size of the vector.

#### `vector_add_from_array`

```c
void vector_add_from_array(vector_t* vector, uint32_t size, float* data);
```

Write the values of an array into the vector. The size must be the same as
the size of the vector.

#### `vector_printf`

```c
void vector_printf(vector_t* vector, print_t func);
```

Write the vector, one value for each line. Give NULL as the function to
write with printf.

#### `vector_get`

```c
float vector_get(vector_t* vector, uint32_t index);
```

Give the value of the vector at the given index. The index must be below the
size of the vector.

#### `vector_dot_product`

```c
float vector_dot_product(vector_t* x, vector_t* y);
```

Give the dot product of the two vectors, which is the sum of the products of
the values at the same index. Both vectors must have the same size.

#### `vector_norm`

```c
float vector_norm(vector_t* x);
```

Give the length of the vector, which is the square root of the dot product
of the vector with itself. The result is never less than zero.

#### `vector_free`

```c
void vector_free(vector_t* vector);
```

Release the memory of a vector that came from vector_alloc. This function
does nothing for a vector that came from vector_static_alloc, thus a call
for either kind is safe.

---

## vector2d

Vectors with two values. Declared in `vector2d/vector2d.h`.

### Functions

#### `vector2d_alloc`

```c
vector_t vector2d_alloc();
```

Give a vector with two values. The memory comes from the heap. Give the
vector to vector_free when you no longer need it.

#### `vector2d_static_alloc`

```c
vector_t vector2d_static_alloc(float* mempool);
```

Give a vector with two values that uses the memory at mempool. That memory
must hold two float values. This function takes no memory from the heap.

#### `vector2d_add_point_at_index`

```c
void vector2d_add_point_at_index(vector_t* vector, uint32_t index, float data);
```

Write a value into the vector at the given index. The index must be 0 or 1.

#### `vector2d_add_from_array`

```c
void vector2d_add_from_array(vector_t* vector, float* data);
```

Write two values from an array into the vector.

#### `vector2d_printf`

```c
void vector2d_printf(vector_t* vector, int (*func)(const char *, ...));
```

Write the vector, one value for each line. Give NULL as the function to
write with printf.

#### `vector2d_get`

```c
float vector2d_get(vector_t* vector, uint32_t index);
```

Give the value of the vector at the given index. The index must be 0 or 1.

#### `vector2d_dot_product`

```c
float vector2d_dot_product(vector_t* x, vector_t* y);
```

Give the dot product of the two vectors.

#### `vector2d_norm`

```c
float vector2d_norm(vector_t* x);
```

Give the length of the vector.

---

## cspline

Cubic splines. Declared in `cspline/cspline.h`.

### Types

#### `cspline_t`

A cubic spline through a set of points.

A spline gives a smooth curve through the given points. Between two
neighbouring points it follows a polynomial of the third power, and at each
point the curve has no step and no corner.

The spline holds the points and the three coefficients of each interval. The
arrays b, c and d hold one value for each interval, thus they hold one value
less than the number of points.

```c
typedef struct{
    uint32_t size;
    float* x;
    float* y;
    float* b;
    float* c;
    float* d;
    bool dynamic_alloc;
}cspline_t;
```

#### `cspline_mempool_t`

The memory that cspline_init needs while it calculates the coefficients.

This memory is separate from the spline, because several splines can share
one memory pool, one after the other. The pool holds nothing that the spline
needs after cspline_init gives back.

```c
typedef struct{
    float* dx;
    float* dp;
    float* d;
    float* b;
    float* q;
    bool dynamic_alloc;
}cspline_mempool_t;
```

### Functions

#### `cspline_alloc`

```c
cspline_t cspline_alloc(uint32_t size);
```

Give a spline for the given number of points. The memory comes from the
heap. Give the spline to cspline_free when you no longer need it.

#### `cspline_static_alloc`

```c
cspline_t cspline_static_alloc(uint32_t size, float** membank);
```

Give a spline that uses the memory that the caller holds. The parameter
membank is a list of five pointers. Each of them must hold room for as many
float values as the given size. This function takes no memory from the heap.

#### `cspline_alloc_mempool`

```c
cspline_mempool_t cspline_alloc_mempool(uint32_t size);
```

Give a memory pool for a spline of the given number of points. The memory
comes from the heap. Give the pool to cspline_free_mempool.

#### `cspline_static_alloc_mempool`

```c
cspline_mempool_t cspline_static_alloc_mempool(float** membank);
```

Give a memory pool that uses the memory that the caller holds. The parameter
membank is a list of five pointers. This function takes no memory from the
heap.

#### `cspline_init`

```c
void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, float* x, float* y);
```

Calculate the coefficients of the spline for the given points.

The values of x must rise, and no two of them may be the same. The lists x
and y must hold as many values as the size of the spline. The memory pool
must be as large as the spline. The pool holds nothing after the call.

#### `cspline_update_size`

```c
void cspline_update_size(cspline_t* cspline, uint32_t size);
```

Change the number of points that the spline uses. The new size must not be
larger than the size that the allocation gave. Call cspline_init after this
function, because the coefficients belong to the points of the old size.

#### `cspline_get_interpolated_point`

```c
float cspline_get_interpolated_point(cspline_t* cspline, float x);
```

Give the value of the curve at the position x.

A position between the first knot and the last knot gives a point on the
curve. A position outside that range gives the polynomial of the nearest
interval, which moves away from the points quickly. Call cspline_init before
this function.

#### `cspline_free`

```c
void cspline_free(cspline_t cspline);
```

Release the memory of a spline that came from cspline_alloc. This function
does nothing for a spline that came from cspline_static_alloc.

The parameter is the structure itself and not a pointer to it, thus the
caller must not use the structure after the call.

#### `cspline_free_mempool`

```c
void cspline_free_mempool(cspline_mempool_t mempool);
```

Release the memory of a pool that came from cspline_alloc_mempool. This
function does nothing for a pool that came from cspline_static_alloc_mempool.

---

## imf

Intrinsic mode functions. Declared in `imf/imf.h`.

### Types

#### `imf_t`

An intrinsic mode function.

The empirical mode decomposition takes a signal apart into such functions.
Each one holds a part of the signal at one range of frequency. The module
emd makes them, and this module holds one of them and writes it out.

```c
typedef struct{
    float* x;                   // The position of each point
    float* y;                   // The value of each point
    uint32_t size;              // The number of points
    bool dynamic_alloc;         // True if the memory comes from the heap
}imf_t;
```

### Functions

#### `imf_alloc`

```c
imf_t imf_alloc(uint32_t size);
```

Give a function that holds the given number of points. The memory comes from
the heap. Give the function to imf_free when you no longer need it.

#### `imf_static_alloc`

```c
imf_t imf_static_alloc(uint32_t size, float* x, float* y);
```

Give a function that uses the memory at x and at y. Both must hold as many
float values as the given size. This function takes no memory from the heap.

#### `imf_printf`

```c
void imf_printf(imf_t* imf, print_t func);
```

Write the function, one point for each line, as "x, y". Give NULL as the
print function to write with printf.

#### `imf_print_all`

```c
void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, print_t func);
```

Write several functions beside each other, one point for each line and one
column for each function. The list must hold as many functions as num_of_imf
says, and each of them must hold as many points as size says. Give NULL as
the print function to write with printf.

#### `imf_free`

```c
void imf_free(imf_t imf);
```

Release the memory of a function that came from imf_alloc. This function
does nothing for a function that came from imf_static_alloc.

The parameter is the structure itself and not a pointer to it, thus the
caller must not use the structure after the call.

---

## emd

Empirical mode decomposition. Declared in `emd/emd.h`.

### Macros

#### `EMD_MINIMUM_SIZE`

```c
#define EMD_MINIMUM_SIZE    3u
```

A signal with fewer than three samples holds no peak and no valley, thus
the decomposition cannot take anything out of it.

### Types

#### `emd_t`

The empirical mode decomposition.

The decomposition takes a signal apart into intrinsic mode functions and a
residue. Each function holds one range of frequency of the signal. The sum
of all the functions and the residue gives the signal again.

The method works in steps. It finds the peaks and the valleys of the signal,
draws a spline through the peaks and a spline through the valleys, and takes
the mean of the two curves away from the signal. It repeats this until the
result is an intrinsic mode function. The rest is the residue, and the
method starts again with it.

```c
typedef struct{
    float* x;
    float* y;
    uint32_t size;
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    float* peak_buffer;
    float* peak_index_buffer;
    float* valley_buffer;
    float* valley_index_buffer;
    imf_t* imf;
    uint32_t imf_count;
    float* residue;
    float* working_buffer;
    bool dynamic_alloc;
}emd_t;
```

### Functions

#### `emd_alloc`

```c
emd_t emd_alloc(uint32_t size);
```

Give a decomposition for a signal of the given number of samples. The memory
comes from the heap. Give it to emd_free when you no longer need it.

#### `emd_static_alloc`

```c
emd_t emd_static_alloc(uint32_t size, float** membank, float** mempool, float* peak_buffer, float* valley_buffer);
```

Give a decomposition that uses the memory that the caller holds. The
parameters membank and mempool are lists of five pointers for the spline and
for its memory pool. The two buffers must each hold room for as many float
values as the number of samples. This function takes no memory from the heap.

#### `emd_initialize`

```c
void emd_initialize(emd_t* emd, uint32_t num_of_imf, imf_t* imf, float* x, float* y, float* residue, float* working_buffer, float* peak_index_buffer, float* valley_index_buffer);
```

Give the decomposition the signal and the memory that it needs while it
runs.

The parameter imf is a list of intrinsic mode functions, and num_of_imf says
how many it holds. That number is the largest number of functions that the
decomposition can give. The lists x and y hold the signal. The other four
lists must each hold room for as many float values as the number of samples.

#### `emd_get_imf`

```c
imf_t* emd_get_imf(emd_t* emd, uint32_t imf_index, uint32_t stopping_threshold, uint32_t* status);
```

Take one intrinsic mode function out of the residue and give a pointer to
it. The pointer shows into the list that emd_initialize took.

The parameter stopping_threshold says how many times the method may repeat
its step. The status becomes 1 if the method did at least one step, and 0 if
it did none. A status of 0 says that the residue holds no more function.

A signal with fewer than EMD_MINIMUM_SIZE samples holds no peak and no
valley. The function then gives a function with the value zero and the
status 0.

#### `emd_sift`

```c
uint32_t emd_sift(emd_t* emd, uint32_t stopping_threshold);
```

Take the signal apart and give the number of intrinsic mode functions that
the method found.

The function fills the list that emd_initialize took, and it leaves the rest
of the signal in the residue. The sum of all the functions and the residue
gives the signal again. The method stops when it has as many functions as
emd_initialize allowed, or when the residue holds no more function.

#### `emd_free`

```c
void emd_free(emd_t emd);
```

Release the memory of a decomposition that came from emd_alloc. This
function does nothing for one that came from emd_static_alloc. It does not
touch the memory that emd_initialize took.

The parameter is the structure itself and not a pointer to it, thus the
caller must not use the structure after the call.

---

## kalman

The Kalman filter. Declared in `kalman/kalman.h`.

### Macros

#### `KALMAN_MEMPOOL_SIZE`

```c
#define KALMAN_MEMPOOL_SIZE(ni, nx, ny)     ((6*(nx)*(nx)) + (5*(nx)*(ny)) + (6*(ny)*(ny)) \
```

The number of float elements that kalman_static_alloc needs in the memory
pool. Give the same three sizes that you give to kalman_static_alloc.

### Types

#### `kalman_scratch_t`

Scratch matrices. The filter uses these matrices to hold the intermediate
results of the predict step and of the update step. The filter does not get
memory while it runs. Thus a static filter needs no heap.

```c
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
```

#### `kalman_t`

```c
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
```

### Functions

#### `kalman_alloc`

```c
kalman_t kalman_alloc(uint32_t ni, uint32_t nx, uint32_t ny);
```

Give a filter for the given sizes. The parameter ni is the number of inputs,
nx the number of elements of the state, and ny the number of elements of the
measurement. All three must be larger than zero.

The memory comes from the heap, and every matrix holds zero. Give the filter
to kalman_free when you no longer need it.

#### `kalman_static_alloc`

```c
kalman_t kalman_static_alloc(uint32_t ni, uint32_t nx, uint32_t ny, float* mempool);
```

Give a filter that uses the memory at mempool. That memory must hold as many
float values as KALMAN_MEMPOOL_SIZE gives for the same three sizes, and it
must stay while the filter is in use.

This function takes no memory from the heap, and the filter takes none while
it runs. Thus a target with no heap can use the filter.

#### `kalman_set_state_matrix`

```c
void kalman_set_state_matrix(kalman_t* kalman, matrix_t* state_matrix);
```

Set the state of the filter. This function writes the value into the state
and into the previous state, thus it gives the filter its first value.

#### `kalman_set_state_transition_matrix`

```c
void kalman_set_state_transition_matrix(kalman_t* kalman, matrix_t* state_transition_matrix);
```

Set the matrix A, which says how the state moves from one step to the next.

#### `kalman_set_control_matrix`

```c
void kalman_set_control_matrix(kalman_t* kalman, matrix_t* control_matrix);
```

Set the matrix B, which says how the input acts on the state.

#### `kalman_set_covariance_matrix`

```c
void kalman_set_covariance_matrix(kalman_t* kalman, matrix_t* covariance_matrix);
```

Set the matrix P, which says how much doubt the state holds.

#### `kalman_set_process_noise_covariance_matrix`

```c
void kalman_set_process_noise_covariance_matrix(kalman_t* kalman, matrix_t* process_noise_covariance);
```

Set the matrix Q, which says how much noise the model itself adds at each
step.

#### `kalman_set_measurement_covariance_matrix`

```c
void kalman_set_measurement_covariance_matrix(kalman_t* kalman, matrix_t* measurement_covariance);
```

Set the matrix R, which says how much noise the measurement holds.

#### `kalman_set_observation_matrix`

```c
void kalman_set_observation_matrix(kalman_t* kalman, matrix_t* observation_matrix);
```

Set the matrix C, which says which part of the state the measurement reads.

#### `kalman_set_input_matrix`

```c
void kalman_set_input_matrix(kalman_t* kalman, matrix_t* input_matrix);
```

Set the matrix u, which holds the input of the present step.

#### `kalman_set_measurement_matrix`

```c
void kalman_set_measurement_matrix(kalman_t* kalman, matrix_t* measurement_matrix);
```

Set the matrix y, which holds the measurement of the present step.

#### `kalman_predict`

```c
void kalman_predict(kalman_t* kalman);
```

Calculate the state and the covariance before the measurement:

    x = A*x + B*u
    P = A*P*A' + Q

#### `kalman_update`

```c
bool kalman_update(kalman_t* kalman);
```

Correct the state and the covariance with the measurement:

    S = C*P*C' + R
    K = P*C'*inverse(S)
    x = x + K*(y - C*x)
    P = (I - K*C)*P

Give false if S is singular. The state does not change then, and the member
singular becomes true.

#### `kalman_step`

```c
bool kalman_step(kalman_t* kalman, matrix_t* input_matrix, matrix_t* measurement_matrix);
```

Do one full cycle of the filter: set the input and the measurement, then
predict, then update.

Give NULL as the input matrix if the model has no control input. The input
matrix keeps its last value then. Give false if the update could not run.

#### `kalman_get_state_matrix`

```c
matrix_t* kalman_get_state_matrix(kalman_t* kalman);
```

Give the state matrix x of the filter.

This function and the two below give a pointer to a matrix inside the
filter. The matrix belongs to the filter, thus the caller must not release
it, and the next step of the filter changes its values.

#### `kalman_get_covariance_matrix`

```c
matrix_t* kalman_get_covariance_matrix(kalman_t* kalman);
```

Give the covariance matrix P of the filter.

#### `kalman_get_gain_matrix`

```c
matrix_t* kalman_get_gain_matrix(kalman_t* kalman);
```

Give the gain matrix K that the last update calculated.

#### `kalman_free`

```c
void kalman_free(kalman_t* kalman);
```

Release the memory of a filter that came from kalman_alloc. This function
does nothing for a filter that came from kalman_static_alloc, thus a call
for either kind is safe. A second call does nothing.

---

## binarysearch

Binary search. Declared in `utils/binarysearch/binarysearch.h`.

### Functions

#### `binarysearch_get_index`

```c
uint32_t binarysearch_get_index(float* data, float value, uint32_t size);
```

Give the index of the first value of the list that is not less than the
given value. The values of the list must rise.

The result is always an index that the caller can use. If every value of the
list is less than the given value, the result is the index of the last
value. Thus a caller that reads the list at the result never reads memory
after the end of the list.

---

## peakdetect

Peak detection. Declared in `utils/peakdetect/peakdetect.h`.

### Functions

#### `peakdetect_get_peaks`

```c
uint32_t peakdetect_get_peaks(float* input, float* index_buffer, float* peak_buffer, uint32_t size);
```

Find every peak of the signal and give the number of them.

A peak is a sample that is larger than the sample before it and larger than
the sample after it. Thus the first sample and the last sample are never
peaks, and a signal with fewer than three samples holds no peak.

The function writes the index of each peak into index_buffer and the value
of each peak into peak_buffer. Both buffers must hold room for as many
values as the signal holds.

---

## valleydetect

Valley detection. Declared in `utils/valleydetect/valleydetect.h`.

### Functions

#### `valleydetect_get_valley`

```c
uint32_t valleydetect_get_valley(float* input, float* index_buffer, float* valley_buffer, uint32_t size);
```

Find every valley of the signal and give the number of them.

A valley is a sample that is smaller than the sample before it and smaller
than the sample after it. Thus the first sample and the last sample are
never valleys, and a signal with fewer than three samples holds no valley.

The function writes the index of each valley into index_buffer and the value
of each valley into valley_buffer. Both buffers must hold room for as many
values as the signal holds.

---

## point2d

A point on a plane. Declared in `point2d/point2d.h`.

### Types

#### `point2d_t`

A point on a plane.

```c
typedef struct{
    float x;
    float y;
}point2d_t;
```

---

## callback

The print callback. Declared in `common/callback.h`.
