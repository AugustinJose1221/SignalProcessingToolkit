# matrix

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Matrices of float values. Declared in `matrix/matrix.h`.

[Back to the index](../API.md)

## Types

### `matrix_t`

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

## Functions

### `matrix_alloc`

```c
matrix_t matrix_alloc(uint32_t m, uint32_t n);
```

Give a matrix with m rows and n columns. The memory comes from the heap, and
the elements hold no value yet. Give the matrix to matrix_free when you no
longer need it.

### `matrix_static_alloc`

```c
matrix_t matrix_static_alloc(uint32_t m, uint32_t n, float* elem);
```

Give a matrix with m rows and n columns that uses the memory at elem. That
memory must hold m*n float values, and it must stay while the matrix is in
use. This function takes no memory from the heap.

### `matrix_add_element`

```c
void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, float value);
```

Write a value into the matrix at the row i and the column j.

### `matrix_get_element`

```c
float matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j);
```

Give the value of the matrix at the row i and the column j.

### `matrix_get_nth_row`

```c
matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index);
```

Give a new matrix with one row that holds the given row of the matrix. Give
the result to matrix_free.

### `matrix_get_nth_col`

```c
matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index);
```

Give a new matrix with one column that holds the given column of the matrix.
Give the result to matrix_free.

### `matrix_get_order`

```c
matrix_t matrix_get_order(matrix_t* matrix);
```

Give a new matrix with one row and two columns. The first element is the
number of rows of the matrix, and the second element is the number of
columns. Give the result to matrix_free.

### `matrix_trace`

```c
float matrix_trace(matrix_t* matrix);
```

Give the sum of the elements on the diagonal. The matrix must be square.

### `matrix_determinant`

```c
float matrix_determinant(matrix_t* matrix);
```

Give the determinant of the matrix. The matrix must be square.

The calculation uses the rule of the cofactors. The cost of that rule grows
with the factorial of the order, thus a matrix of a large order takes a long
time. Keep the order below 10.

### `matrix_create_unit_matrix`

```c
matrix_t matrix_create_unit_matrix(uint32_t size);
```

Give a new square matrix that holds 1 on the diagonal and 0 at every other
place. Give the result to matrix_free.

### `matrix_create_zero_matrix`

```c
matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n);
```

Give a new matrix that holds 0 at every place. Give the result to
matrix_free.

### `matrix_is_equal`

```c
bool matrix_is_equal(matrix_t* a, matrix_t* b);
```

True if the two matrices have the same order and the same value at every
place.

### `matrix_is_square`

```c
bool matrix_is_square(matrix_t* matrix);
```

True if the matrix has as many rows as columns.

### `matrix_is_zero`

```c
bool matrix_is_zero(matrix_t* matrix);
```

True if every element of the matrix is 0.

### `matrix_is_unit`

```c
bool matrix_is_unit(matrix_t* matrix);
```

True if the matrix holds 1 on the diagonal and 0 at every other place. The
matrix must be square.

### `matrix_is_multipliable`

```c
bool matrix_is_multipliable(matrix_t* a, matrix_t* b);
```

True if the first matrix can multiply the second one. That asks for as many
columns in the first matrix as rows in the second one.

### `matrix_add`

```c
matrix_t matrix_add(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the sum of the two matrices. Both matrices must
have the same order. Give the result to matrix_free.

### `matrix_subtract`

```c
matrix_t matrix_subtract(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the first matrix less the second one. Both
matrices must have the same order. Give the result to matrix_free.

### `matrix_multiply_scalar`

```c
matrix_t matrix_multiply_scalar(matrix_t* matrix, float scalar);
```

Give a new matrix where each element is the element of the given matrix
multiplied by the scalar. Give the result to matrix_free.

### `matrix_multiply`

```c
matrix_t matrix_multiply(matrix_t* a, matrix_t* b);
```

Give a new matrix that holds the product of the two matrices. The first
matrix must have as many columns as the second one has rows. The result has
as many rows as the first matrix and as many columns as the second one. Give
the result to matrix_free.

### `matrix_transpose`

```c
matrix_t matrix_transpose(matrix_t* matrix);
```

Give a new matrix where the rows of the given matrix are the columns. Give
the result to matrix_free.

### `matrix_inverse`

```c
matrix_t matrix_inverse(matrix_t* matrix);
```

Give a new matrix that is the inverse of the given matrix. The matrix must
be square.

The elimination uses a partial pivot, thus a zero on the diagonal does not
stop it. If the matrix is singular it has no inverse, and the function gives
a matrix that holds 0 at every place. Use matrix_is_zero on the result to
find that state. Give the result to matrix_free.

### `matrix_copy`

```c
void matrix_copy(matrix_t* src, matrix_t* dest);
```

Write the elements of the source into the destination. Both matrices must
have the same order.

### `matrix_printf`

```c
void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...));
```

Write the matrix, one row for each line. Give NULL as the function to write
with printf.

### `matrix_free`

```c
void matrix_free(matrix_t* matrix);
```

Release the memory of a matrix that came from matrix_alloc. This function
does nothing for a matrix that came from matrix_static_alloc, thus a call
for either kind is safe. A second call does nothing.

### `matrix_add_into`

```c
void matrix_add_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the sum of the two matrices into the destination. All three matrices
must have the same order.

### `matrix_subtract_into`

```c
void matrix_subtract_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the first matrix less the second one into the destination. All three
matrices must have the same order.

### `matrix_multiply_into`

```c
void matrix_multiply_into(matrix_t* a, matrix_t* b, matrix_t* dest);
```

Write the product of the two matrices into the destination. The destination
must have as many rows as the first matrix and as many columns as the second
one.

### `matrix_multiply_scalar_into`

```c
void matrix_multiply_scalar_into(matrix_t* matrix, float scalar, matrix_t* dest);
```

Write each element of the matrix multiplied by the scalar into the
destination. Both matrices must have the same order.

### `matrix_transpose_into`

```c
void matrix_transpose_into(matrix_t* matrix, matrix_t* dest);
```

Write the transpose of the matrix into the destination. The destination must
have as many rows as the matrix has columns, and as many columns as the
matrix has rows.

### `matrix_set_unit`

```c
void matrix_set_unit(matrix_t* matrix);
```

Write 1 on the diagonal of the matrix and 0 at every other place. The matrix
must be square.

### `matrix_set_zero`

```c
void matrix_set_zero(matrix_t* matrix);
```

Write 0 into every element of the matrix.

### `matrix_inverse_into`

```c
bool matrix_inverse_into(matrix_t* matrix, matrix_t* dest, matrix_t* scratch);
```

Write the inverse of the matrix into the destination. The matrix must be
square, and the destination must have the same order.

The scratch matrix must have the order n x 2n, where n is the order of the
matrix. It loses its content. The function gives false if the matrix is
singular, and it does not change the destination then.
