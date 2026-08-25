# eigen

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The directions a matrix stretches. Declared in `sptk/linalg/eigen.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Macros

### `EIGEN_LARGEST_SWEEPS`

```c
#define EIGEN_LARGEST_SWEEPS    30u
```

### `EIGEN_SMALLEST_PART`

```c
#define EIGEN_SMALLEST_PART     (REAL_C(10.0) * REAL_EPSILON)
```

## Functions

### `eigen_is_valid_matrix`

```c
bool eigen_is_valid_matrix(matrix_t* matrix);
```

True if this matrix can be given to eigen_solve: square, at least one by
one, and symmetric within the tolerance that eigen_solve uses.

### `eigen_solve`

```c
bool eigen_solve(matrix_t* matrix, real_t* values, matrix_t* vectors);
```

Find the eigenvalues and the eigenvectors of a symmetric matrix.

THE MATRIX LOSES ITS CONTENT. The method works by turning the matrix itself
until only its diagonal is left, thus a caller that still needs the matrix
must copy it first with matrix_copy.

The values hold one number for each row and come back LARGEST FIRST, which
is the order that makes the first of them the one that matters. The vectors
must be a matrix of the same order; column k of it is the direction that
belongs to value k, and it is of unit length.

The vectors may be NULL where only the values are wanted, and the work is
then a little less.

Give false if the matrix is not one eigen_is_valid_matrix accepts, if the
vectors are the wrong order, or if the rotations did not settle within
EIGEN_LARGEST_SWEEPS sweeps.

### `eigen_condition`

```c
real_t eigen_condition(const real_t* values, uint32_t count);
```

Give the largest eigenvalue divided by the smallest, both taken by size.

THIS IS THE ONE NUMBER TO LOOK AT. It says how much a small error in what
goes into a calculation is multiplied on its way out. A condition of 1 is as
good as a matrix gets; a condition near 1 divided by the smallest step the
width can tell means the answer is made of rounding.

Give REAL_LARGEST where the smallest eigenvalue is nothing, which means the
matrix squashes some direction to nothing and cannot be undone at all.

### `eigen_rank`

```c
uint32_t eigen_rank(const real_t* values, uint32_t count, real_t part);
```

How many directions the matrix really stretches, which is how many
eigenvalues stand above the largest one multiplied by the part given.

A part of about 1000 times the smallest step the width can tell is the usual
choice. Below that the answer counts directions that are nothing but
rounding.

### `eigen_part_held`

```c
real_t eigen_part_held(const real_t* values, uint32_t count, uint32_t first);
```

How much of the whole spread the first few directions hold, from 0 to 1.

This is what principal components is for: where the first two of six
directions hold 0.98 of the spread, the other four are noise and the
measurement really has two dimensions and not six.

Give 0 where the count is nothing or the eigenvalues do not add to anything.
