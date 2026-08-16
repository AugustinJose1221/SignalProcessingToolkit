# cmatrix

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Matrices of complex numbers. Declared in `sptk/linalg/cmatrix.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Types

### `cmatrix_t`

```c
typedef struct{
    uint32_t m;
    uint32_t n;
    cnum_t *elem;
    bool dynamic_alloc;
}cmatrix_t;
```

## Functions

### `cmatrix_alloc`

```c
cmatrix_t cmatrix_alloc(uint32_t m, uint32_t n);
```

Give a matrix with m rows and n columns. The memory comes from the heap, and
the elements hold no value yet. Give the matrix to cmatrix_free when you no
longer need it.

### `cmatrix_static_alloc`

```c
cmatrix_t cmatrix_static_alloc(uint32_t m, uint32_t n, cnum_t* elem);
```

Give a matrix with m rows and n columns that uses the memory at elem. That
memory must hold m*n complex numbers. This function takes no memory from the
heap.

### `cmatrix_add_element`

```c
void cmatrix_add_element(cmatrix_t* matrix, uint32_t i, uint32_t j, cnum_t value);
```

Write a value into the matrix at the row i and the column j.

### `cmatrix_get_element`

```c
cnum_t cmatrix_get_element(cmatrix_t* matrix, uint32_t i, uint32_t j);
```

Give the value of the matrix at the row i and the column j.

### `cmatrix_create_unit_matrix`

```c
cmatrix_t cmatrix_create_unit_matrix(uint32_t size);
```

Give a new square matrix that holds 1 on the diagonal and 0 at every other
place. Give the result to cmatrix_free.

### `cmatrix_create_zero_matrix`

```c
cmatrix_t cmatrix_create_zero_matrix(uint32_t m, uint32_t n);
```

Give a new matrix that holds 0 at every place. Give the result to
cmatrix_free.

### `cmatrix_is_equal`

```c
bool cmatrix_is_equal(cmatrix_t* a, cmatrix_t* b);
```

True if the two matrices have the same order and exactly the same value at
every place.

### `cmatrix_is_near`

```c
bool cmatrix_is_near(cmatrix_t* a, cmatrix_t* b, float tolerance);
```

True if the two matrices have the same order and no pair of values differs
by more than the tolerance. Use this function after a calculation with
several steps, where the result is near the correct value but not equal to
it.

### `cmatrix_is_square`

```c
bool cmatrix_is_square(cmatrix_t* matrix);
```

True if the matrix has as many rows as columns.

### `cmatrix_is_zero`

```c
bool cmatrix_is_zero(cmatrix_t* matrix);
```

True if every element of the matrix is 0.

### `cmatrix_is_unit`

```c
bool cmatrix_is_unit(cmatrix_t* matrix);
```

True if the matrix holds 1 on the diagonal and 0 at every other place. The
matrix must be square.

### `cmatrix_is_multipliable`

```c
bool cmatrix_is_multipliable(cmatrix_t* a, cmatrix_t* b);
```

True if the first matrix can multiply the second one. That asks for as many
columns in the first matrix as rows in the second one.

### `cmatrix_is_hermitian`

```c
bool cmatrix_is_hermitian(cmatrix_t* matrix);
```

True if the matrix does not change when the conjugate transpose is taken.
Such a matrix is Hermitian, and its values on the diagonal are all real.

### `cmatrix_add`

```c
cmatrix_t cmatrix_add(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the sum of the two matrices. Both matrices must
have the same order. Give the result to cmatrix_free.

### `cmatrix_subtract`

```c
cmatrix_t cmatrix_subtract(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the first matrix less the second one. Both
matrices must have the same order. Give the result to cmatrix_free.

### `cmatrix_multiply`

```c
cmatrix_t cmatrix_multiply(cmatrix_t* a, cmatrix_t* b);
```

Give a new matrix that holds the product of the two matrices. The first
matrix must have as many columns as the second one has rows. Give the result
to cmatrix_free.

### `cmatrix_multiply_scalar`

```c
cmatrix_t cmatrix_multiply_scalar(cmatrix_t* matrix, cnum_t scalar);
```

Give a new matrix where each element is the element of the given matrix
multiplied by the scalar. Give the result to cmatrix_free.

### `cmatrix_transpose`

```c
cmatrix_t cmatrix_transpose(cmatrix_t* matrix);
```

Give a new matrix where the rows of the given matrix are the columns. This
operation does not change the sign of the imaginary parts. Give the result to
cmatrix_free.

### `cmatrix_conjugate_transpose`

```c
cmatrix_t cmatrix_conjugate_transpose(cmatrix_t* matrix);
```

The transpose where each element becomes its conjugate. This operation takes
the place of the transpose for a matrix of complex numbers.

### `cmatrix_trace`

```c
cnum_t cmatrix_trace(cmatrix_t* matrix);
```

Give the sum of the elements on the diagonal. The matrix must be square.

### `cmatrix_determinant`

```c
cnum_t cmatrix_determinant(cmatrix_t* matrix);
```

Give the determinant of the matrix. The matrix must be square.

The calculation uses elimination with a partial pivot, thus its cost grows
with the third power of the order. The elimination needs a copy of the
matrix, thus this function gets memory from the heap. Use
cmatrix_determinant_into on a target with no heap.

### `cmatrix_inverse`

```c
cmatrix_t cmatrix_inverse(cmatrix_t* matrix);
```

Give a new matrix that is the inverse of the given matrix. The matrix must
be square.

The elimination uses a partial pivot, thus a zero on the diagonal does not
stop it. If the matrix is singular it has no inverse, and the function gives
a matrix that holds 0 at every place. Use cmatrix_is_zero on the result to
find that state. Give the result to cmatrix_free.

### `cmatrix_copy`

```c
void cmatrix_copy(cmatrix_t* src, cmatrix_t* dest);
```

Write the elements of the source into the destination. Both matrices must
have the same order.

### `cmatrix_printf`

```c
void cmatrix_printf(cmatrix_t* matrix, print_t func);
```

Write the matrix, one row for each line, in the form "a + bi". Give NULL as
the function to write with printf.

### `cmatrix_free`

```c
void cmatrix_free(cmatrix_t* matrix);
```

Release the memory of a matrix that came from cmatrix_alloc. This function
does nothing for a matrix that came from cmatrix_static_alloc, thus a call
for either kind is safe. A second call does nothing.

### `cmatrix_add_into`

```c
void cmatrix_add_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Operations that write into a matrix that already holds memory.

The destination must have the correct order, and it must not be one of the
sources. These operations get no memory, thus code that must not use the
heap can call them.
Write the sum of the two matrices into the destination. All three matrices
must have the same order.

### `cmatrix_subtract_into`

```c
void cmatrix_subtract_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Write the first matrix less the second one into the destination. All three
matrices must have the same order.

### `cmatrix_multiply_into`

```c
void cmatrix_multiply_into(cmatrix_t* a, cmatrix_t* b, cmatrix_t* dest);
```

Write the product of the two matrices into the destination. The destination
must have as many rows as the first matrix and as many columns as the second
one.

### `cmatrix_multiply_scalar_into`

```c
void cmatrix_multiply_scalar_into(cmatrix_t* matrix, cnum_t scalar, cmatrix_t* dest);
```

Write each element of the matrix multiplied by the scalar into the
destination. Both matrices must have the same order.

### `cmatrix_transpose_into`

```c
void cmatrix_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
```

Write the transpose of the matrix into the destination. The destination must
have as many rows as the matrix has columns, and as many columns as the
matrix has rows.

### `cmatrix_conjugate_transpose_into`

```c
void cmatrix_conjugate_transpose_into(cmatrix_t* matrix, cmatrix_t* dest);
```

Write the conjugate transpose of the matrix into the destination. The
destination must have as many rows as the matrix has columns, and as many
columns as the matrix has rows.

### `cmatrix_set_unit`

```c
void cmatrix_set_unit(cmatrix_t* matrix);
```

Write 1 on the diagonal of the matrix and 0 at every other place. The matrix
must be square.

### `cmatrix_set_zero`

```c
void cmatrix_set_zero(cmatrix_t* matrix);
```

Write 0 into every element of the matrix.

### `cmatrix_determinant_into`

```c
cnum_t cmatrix_determinant_into(cmatrix_t* matrix, cmatrix_t* scratch);
```

The elimination writes its steps into the scratch matrix, which must have
the order n x n. The scratch matrix loses its content.

### `cmatrix_inverse_into`

```c
bool cmatrix_inverse_into(cmatrix_t* matrix, cmatrix_t* dest, cmatrix_t* scratch);
```

The scratch matrix must have the order n x 2n. The function gives false if
the matrix is singular, and it does not change the destination then.
