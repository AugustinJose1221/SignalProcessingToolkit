# lstsq

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Fitting a curve through readings. Declared in `ffitt/linalg/lstsq.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../ffitt/linalg/README.md)

## Macros

### `LSTSQ_SMALLEST_PIVOT_PART`

```c
#define LSTSQ_SMALLEST_PIVOT_PART       (REAL_C(2.0) * REAL_SQRT(REAL_EPSILON))
```

### `LSTSQ_LARGEST_EXCESS`

```c
#define LSTSQ_LARGEST_EXCESS    REAL_C(0.01)
```

### `LSTSQ_SMALLEST_ERROR`

```c
#define LSTSQ_SMALLEST_ERROR    (REAL_C(1000.0) * REAL_EPSILON)
```

### `LSTSQ_HIGHEST_ORDER`

```c
#define LSTSQ_HIGHEST_ORDER     23u
```

### `LSTSQ_HIGHEST_ORDER`

```c
#define LSTSQ_HIGHEST_ORDER     10u
```

### `LSTSQ_COEFFICIENT_COUNT`

```c
#define LSTSQ_COEFFICIENT_COUNT(order)      ((order) + 1u)
```

How many numbers a polynomial of the given order holds, which is one more
than the order: a line is of the first order and holds two.

## Functions

### `lstsq_is_valid_fit`

```c
// that, and it does.
```

True if a polynomial of this order can be fitted through this many points at
the width of this build.

There must be at least as many points as numbers to find, and the order must
not be above LSTSQ_HIGHEST_ORDER. This says nothing about whether the
readings can fix a polynomial of that order; only the fit itself can say

### `lstsq_solve`

```c
// answer would be made of rounding. bool lstsq_solve(matrix_t* model, matrix_t* readings, matrix_t* answer,
```

Solve a set of equations that has more rows than columns, in the sense of
the least total squared error.

The model holds one row for each reading and one column for each number to
find. The readings hold one row each. The answer holds one row for each
number to find.

The two scratch matrices must be square and as wide as the model, and one
column matrix as tall. They lose their content.

Give false if the shapes do not fit together, if the small problem has no
factor, or if two columns of the model say so nearly the same thing that the

### `lstsq_polyfit`

```c
// readings sits far from zero. Read the header on that last one. bool lstsq_polyfit(const real_t* x, const real_t* y, uint32_t size,
```

Fit a polynomial of the given order through the points, in the sense of the
least total squared error.

The coefficients hold LSTSQ_COEFFICIENT_COUNT values, lowest power first:
the first is the constant, the second multiplies x, the third x squared.

This function gets memory from the heap for the matrices it needs. It runs
once, when a calibration is worked out, and not while a device runs.

Give false if lstsq_is_valid_fit is false, or if the points cannot fix a
polynomial of that order. That happens when too many of them share an x,
when the order is too high for the width, and above all when the x of the

### `lstsq_scaling`

```c
// on to a fit that will refuse for the real reason. void lstsq_scaling(const real_t* x, uint32_t size, real_t* centre,
```

Give the centre and the width that bring a set of x to a range about -1 to 1.

The centre is the middle of the range that the readings cover and the width
is half of it. A set of readings that all share one x has no width; the
width then comes back as 1, which changes nothing and lets the caller carry

### `lstsq_polyfit_scaled`

```c
// are not a polynomial in x and using them as one gives nonsense. bool lstsq_polyfit_scaled(const real_t* x, const real_t* y, uint32_t size, uint32_t order, real_t* coefficients,
```

Fit a polynomial through the points, bringing x to a range about -1 to 1
first.

TAKE THIS ONE unless the x of the readings already runs about -1 to 1. The
header says why: a plain fit through readings whose x runs from 1000 to 1001
fails at any order and at either width.

The coefficients are for the SCALED place, thus they must be read with
lstsq_evaluate_scaled and the centre and the width that come back here. They

### `lstsq_evaluate_scaled`

```c
// The centre and the width must be the ones that lstsq_polyfit_scaled gave. real_t lstsq_evaluate_scaled(const real_t* coefficients, uint32_t order,
```

Give the value at one place of a fit that was scaled.

### `lstsq_evaluate`

```c
// x to the ninth directly loses digits that this way keeps.
```

Give the value of a polynomial at one place.

The coefficients are lowest power first, as lstsq_polyfit writes them. The
work is done from the highest power inwards, which needs one multiplication
and one addition for each order and never forms a power on its own: forming

### `lstsq_fit_quality`

```c
// that is believed for no reason. real_t lstsq_fit_quality(const real_t* x, const real_t* y, uint32_t size,
```

Give how much of the movement of the readings the fit accounts for, from 0
to 1.

This is the one number to look at after a fit. A value near 1 says the curve
follows the readings; a value near 0 says it does not, and then the order,
the model or the readings are wrong. A fit that is never examined is a fit

### `lstsq_fit_quality_scaled`

```c
// The centre and the width must be the ones that lstsq_polyfit_scaled gave. real_t lstsq_fit_quality_scaled(const real_t* x, const real_t* y, uint32_t size, const real_t* coefficients, uint32_t order,
```

Give how much of the movement of the readings a scaled fit accounts for,
from 0 to 1.
