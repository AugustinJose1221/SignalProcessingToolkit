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


## quaternion

Which way something is pointing, held as four numbers.

**Why not three angles.** At one attitude two of the three axes line up, and
from that moment the three no longer describe three separate turns: the third
number is lost. This is gimbal lock, and it is not a rounding trouble that a
wider number would fix — the description itself has a hole in it. The hole is
not in an odd corner either: for roll, pitch and yaw it stands at a pitch of
straight up, which is where an aircraft, a robot arm and a camera all go on
purpose.

**Why not a rotation matrix.** A matrix has no hole either and is the right
thing to hold when a rotation is about to be applied to many vectors. But it
holds nine numbers keeping six rules between them, and arithmetic wears those
rules away: after a few thousand small turns the rows are no longer quite at
right angles and what was a rotation has quietly become a rotation with a
stretch in it. Four numbers keep **one** rule, and `quaternion_normalise` puts
it back in one line. A test carries an attitude through 100 000 steps and finds
its length still 1.

**The half angle, and the trap it sets.** A turn of an angle about an axis
gives `w = cos(angle/2)` and the axis times `sin(angle/2)`. Thus turning by a
whole circle gives `w = -1` and not `1`, and **a quaternion and its negative
are the same attitude**. Any code that compares two attitudes must allow for
that; `quaternion_is_same_attitude` does, and a great deal of trouble comes
from code that does not.

**The order of two turns matters.** `quaternion_multiply(a, b)` gives `b`
followed by `a`, which is the same order as multiplying two rotation matrices.

**Reading a matrix back needs care.** There are four forms of the answer, each
dividing by a root, and a root near nothing loses most of its digits.
`quaternion_from_matrix` reads whichever of the four is largest, thus it is
accurate at every attitude; reading one form always would lose its accuracy
near a half turn, which is an ordinary attitude and not a corner case.

**`quaternion_slerp` turns at a steady rate.** Adding two attitudes and
normalising is the obvious shortcut and it hurries through the middle. It also
takes the short way round, which needs the sign allowed for: without that, two
attitudes a few degrees apart could be interpolated the long way round.

## lstsq

**A fit is not a solve.** Twenty readings and three numbers to find has no
answer that passes through all twenty, and looking for one is the wrong
question. The right one is which three numbers leave the smallest total error.

**Where x sits matters as much as the order.** This is the one thing to carry
away. Fitting a sine through 60 points, the highest order that still follows
the readings to three digits:

| x runs from | 32 bits | 64 bits |
|---|---|---|
| 0 to 1 | 5 | 11 |
| -1 to 1 | 10 | 23 |

The same readings and the same width reach more than twice the order when x is
moved. Nothing about the data changed; only where its x sits. Move 50 points to
x from 1000 to 1001 and **even a cubic is refused**, at 64 bits, on data that
fits perfectly. A thermistor read in ohms runs from 1000 to 70000, which is
exactly this case.

**Thus use `lstsq_polyfit_scaled` unless x already runs about -1 to 1.** It
brings x to that range first and gives back the centre and the width it used.
The coefficients it gives are for the scaled place: they must be read with
`lstsq_evaluate_scaled` and those two numbers. They are **not** a polynomial in
x, and using them as one gives nonsense.

**The module refuses rather than answering badly.** The normal equations square
how badly conditioned a problem is, and a factor of Cholesky exists long after
the answer has stopped meaning anything. The fit looks at the diagonal of that
factor and says no where the answer would be made of rounding, which
measurement puts at one order past the last one worth having.

**Look at `lstsq_fit_quality` afterwards.** It gives how much of the movement of
the readings the curve accounts for, from 0 to 1. A fit that is never examined
is a fit that is believed for no reason.

**A high order is usually the wrong answer anyway.** A polynomial of the ninth
order through twelve calibration points passes through all of them and swings
wildly between them. Where a table is what is wanted, `interp` reads between its
points without inventing anything; where a curve is wanted, the third or the
fourth order is nearly always enough.

## eigen

**A symmetric matrix does one thing: it stretches space**, by different amounts
in different directions, and those directions stand at right angles. The amounts
are the eigenvalues and the directions the eigenvectors, and together they are
the whole of what the matrix does.

**Reading a covariance is the use that matters.** The largest eigenvalue is how
far a set of measurements spreads at its widest, and the eigenvector beside it
is which way. For a sensor of three axes watching something that moves along one
line, `eigen_part_held` reports that one direction holds over 0.99 of the spread
and the eigenvector names the line — whatever the axes of the sensor happen to
be. That is what principal components means, and it is two lines once the
eigenvalues are in hand.

**`eigen_condition` is the number behind two things this library already
records**: why `lstsq` refuses a fit, and why an RLS filter can run correctly for
thousands of samples and then fall apart. It says how much a small error in what
goes in is multiplied on its way out.

**Symmetric only, and that is on purpose.** Every covariance is symmetric, so
that is the case signal processing asks for — and it is also the case that
behaves, with real eigenvalues and directions at right angles. A matrix that is
not symmetric can have complex eigenvalues and directions lying almost on top of
each other, and needs a method several times larger that holds far less well in a
float.

**The error does not follow the conditioning**, which is what parts the rotations
of Jacobi from the methods that are quicker. Measured at 32 bits on matrices of
order 5 built to a chosen conditioning, checking that the matrix really does
stretch each direction by its value:

| condition of the matrix | 1 | 10 | 1 000 | 100 000 | 10 000 000 |
|---|---|---|---|---|---|
| worst of `A·v` less `λ·v` | 0.0 | 5e-8 | 7e-8 | 9e-8 | 3e-8 |

A matrix whose widest direction is ten million times its narrowest still gives
directions right to seven digits. A method working through the normal equations,
as `lstsq` does, would have nothing left by then.

## poly

**A polynomial is a list of numbers, lowest power first** — the same order
`lstsq` gives its answers in.

**The roots are what this is for, and stability is why.** An `iir` filter is two
polynomials divided by each other; where the one below crosses nothing the filter
has a pole, and **a pole outside the unit circle is a filter that runs away**.
Not a slow drift — the answer doubles every few samples until it is nothing but
infinities. `poly_is_inside_circle` finds out before it happens. A filter
designed by the `iir` module is stable by construction; one whose coefficients
came from a file, or from a design changed by hand, is stable only if somebody
checked.

**Why the order is capped, and it is not the method that caps it.** Measured on
polynomials built by multiplying known roots together — both how far each root
came back from where it was built, and how near nothing the polynomial really is
there:

| order | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|
| 32 bits, from intended | 3.5e-05 | 6.0e-06 | 1.4e-05 | 4.3e-02 | 2.9e-01 |
| 32 bits, `p(root)` | 6.0e-08 | 7.5e-08 | 1.8e-07 | 1.8e-07 | 3.7e-07 |
| 64 bits, from intended | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 |

**Read the two 32-bit rows against each other.** At order 5 the roots come back a
twentieth away from where they were built, **and the polynomial is still nearly
nothing there**. Both are true at once, and what it means is that the module
found the right roots *of the wrong polynomial* — by order 5 the coefficients
themselves, held at 32 bits, no longer describe the polynomial that was meant. No
method finds roots the coefficients no longer hold.

**For a filter this is rarely a limit.** An `iir` filter is a chain of biquads and
each is order 2, which has a closed form and is exact. Ask about one section at a
time.

**Order 2 is written so that no two nearly equal numbers are subtracted.** Where
one root is far larger than the other, the plain form of the quadratic answer
makes the smaller root out of rounding. Taking the root that adds and reaching
the other through the product of the two costs nothing and holds every digit — a
pair standing twelve orders apart still comes back right.

**Every root is polished against the original polynomial** after the deflation.
Each division carries its own error into what follows, and without the polishing
the answer at order 4 is out by a sixth rather than by a part in fifty thousand.
