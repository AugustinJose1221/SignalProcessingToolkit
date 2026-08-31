# pmatrix

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Matrices with a parameter. Declared in `ffitt/linalg/pmatrix.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../ffitt/linalg/README.md)

## Types

### `pmatrix_t`

```c
typedef struct{
    uint32_t m;
    uint32_t n;
    pmatrix_function_t *elem;
    bool dynamic_alloc;
}pmatrix_t;
```

## Functions

### `pmatrix_alloc`

```c
pmatrix_t pmatrix_alloc(uint32_t m, uint32_t n);
```

Give a parameter matrix with m rows and n columns. The memory comes from
the heap, and every element holds zero. Give the matrix to pmatrix_free when
you no longer need it.

### `pmatrix_static_alloc`

```c
pmatrix_t pmatrix_static_alloc(uint32_t m, uint32_t n, pmatrix_function_t* elem);
```

Give a parameter matrix that uses the memory at elem. That memory must hold
m*n pointers to a function. Every element holds zero after the call. This
function takes no memory from the heap.

### `pmatrix_add_element`

```c
void pmatrix_add_element(pmatrix_t* matrix, uint32_t i, uint32_t j, pmatrix_function_t function);
```

An element that holds NULL gives the value zero. Thus a new matrix that
pmatrix_set_zero cleared holds zero at every place, and a user who needs a
zero at one place does not need a function for it.

### `pmatrix_get_element`

```c
pmatrix_function_t pmatrix_get_element(pmatrix_t* matrix, uint32_t i, uint32_t j);
```

Give the function that stands at the row i and the column j. The result is
NULL if that element holds zero.

### `pmatrix_set_zero`

```c
void pmatrix_set_zero(pmatrix_t* matrix);
```

Write zero into every element of the matrix.

### `pmatrix_evaluate_element`

```c
real_t pmatrix_evaluate_element(pmatrix_t* matrix, uint32_t i, uint32_t j, real_t x);
```

Give the value of one element for the given value of the parameter.

### `pmatrix_evaluate`

```c
matrix_t pmatrix_evaluate(pmatrix_t* matrix, real_t x);
```

Give a new matrix of float values for the given value of the parameter. This
function gets memory. Use pmatrix_evaluate_into on a target with no heap.

### `pmatrix_evaluate_into`

```c
void pmatrix_evaluate_into(pmatrix_t* matrix, real_t x, matrix_t* dest);
```

Write the values into a matrix that already holds memory. The destination
must have the same order as the parameter matrix.

### `pmatrix_zero`

```c
real_t pmatrix_zero(real_t x);
```

An element that always gives zero.

### `pmatrix_one`

```c
real_t pmatrix_one(real_t x);
```

An element that always gives one.

### `pmatrix_free`

```c
void pmatrix_free(pmatrix_t* matrix);
```

Release the memory of a matrix that came from pmatrix_alloc. This function
does nothing for a matrix that came from pmatrix_static_alloc.
