# Linear algebra

The arithmetic that the other areas of the library stand on. The Kalman filters
move their covariance through matrices, the filter of Savitzky and Golay solves
a least squares problem, and the transforms hold complex numbers.

| Module | What an element is |
| --- | --- |
| `matrix` | A float value |
| `cmatrix` | A complex number, from `cnum` |
| `pmatrix` | A pointer to a function of one parameter |
| `cnum` | A complex number by itself |
| `vector` | A list of float values |
| `vector2d` | The same, for the size 2 |

## matrix

The elements lie in one block, one row after the other, thus the element at the
row `i` and the column `j` lies at the position `(i*n)+j`.

**Every operation comes in two forms.** `matrix_multiply` gives a new matrix
and takes memory from the heap. `matrix_multiply_into` writes into a matrix
that the caller already holds and takes no memory. The second form is what lets
the Kalman filters run on a target with no heap. The first form calls the
second one, thus each calculation stands in one place only.

The destination of an operation that writes into a matrix must not be one of
the sources.

**The inverse** uses elimination of Gauss and Jordan with a partial pivot: at
each step it moves the row with the largest element of the column into the
pivot position. That keeps the division stable, and it lets the elimination
pass a zero on the diagonal, which does not always mean the matrix is singular.
`matrix_inverse` gives a matrix of zeros when the matrix truly is singular.

**The determinant** uses the rule of the cofactors, whose cost grows with the
factorial of the order. Keep the order below 10. The complex module uses
elimination instead, whose cost grows with the third power.

## cnum and cmatrix

`cnum` holds a complex number as two float members. The C standard gives
`<complex.h>` and the type `float _Complex`, and this library uses neither.
Many small compilers do not give them, and `<complex.h>` gives the macro
`complex`, thus a module of this library could not carry that name.

`cmatrix` is a separate module from `matrix` and not a setting inside it. A
matrix of complex numbers holds another type of element, thus every operation
needs another calculation. One module for both would need a second copy of each
function, or a check of the type inside every loop, which taxes the user who
only needs real numbers.

It gives the same names as `matrix` for the same operations, and two that
belong to complex numbers only. `cmatrix_conjugate_transpose` takes the
transpose and the conjugate of each element, and it takes the place of the
plain transpose in complex work. `cmatrix_is_hermitian` says whether that
operation gives the same matrix again.

## pmatrix

A matrix such as `[sin(x) cos(x)]` holds elements that change with a parameter.
Each element here is a pointer to a function of that parameter. Give a value,
and `pmatrix_evaluate` gives a `matrix_t` that every other module accepts.

The other way to build this is a module that reads an expression from text and
holds a tree of operations. Such a tree needs memory while the program runs and
needs a parser, and both go against the way this library works.

An element that holds `NULL` gives zero, thus a new matrix holds zero at every
place. `pmatrix_zero` and `pmatrix_one` cover an element that does not change.

A function of the standard library such as `sinf` fits the type of an element
directly.

**It is not enough for a Jacobian.** An element is a function of one float, and
a Jacobian needs functions of the whole state. That is why `ekf` holds its own
type of function.

## vector and vector2d

`vector` gives the dot product and the length. `vector2d` gives the same
operations for a vector of two values, so that the caller does not name the
size at each call. Its result is a `vector_t`, thus every function of `vector`
takes it.


## The factor of Cholesky

`matrix_cholesky` gives the lower triangle L for which L times its own
transpose gives the matrix back. Only a symmetric matrix that is positive
definite has one.

**What it is for.** A covariance matrix says how far a set of numbers spreads
and how their spreads lean on each other. Its factor is the **shape** of that
spread: a set of directions, each as long as the spread reaches that way.
Multiply a step of unit length by the factor and the step lands on the edge of
the spread, whichever way it points. A test holds exactly that, taking unit
steps all the way round a circle and checking each shaped step lands on the
edge.

That is what the unscented Kalman filter needs to place its points, and it is
how a set of unrelated random numbers is made to spread the way a given
covariance says.

It is also the fast way to solve a set of equations whose matrix is a
covariance: about half the work of a general elimination, because it uses the
symmetry instead of ignoring it.

**When it does not exist, and why that matters.** Positive definite means the
spread is real: no direction in which it is zero or negative. A covariance
worked out by a long chain of arithmetic can lose that, and when it does the
failure is the first sign that something upstream has gone wrong. The function
gives a matrix of all zeros then, as `matrix_inverse` does for a singular
matrix.

**It refuses a matrix that is not symmetric** rather than quietly using the
lower half and ignoring the upper half, which would be a different matrix from
the one that was given. The tolerance follows the size of the elements, because
a covariance built by arithmetic is symmetric in principle and not in its last
digits.
