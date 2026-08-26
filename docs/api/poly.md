# poly

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Polynomials, and where they cross nothing. Declared in `sptk/linalg/poly.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Macros

### `POLY_LARGEST_ROOT_ORDER`

```c
#define POLY_LARGEST_ROOT_ORDER     12u
```

### `POLY_LARGEST_ROOT_ORDER`

```c
#define POLY_LARGEST_ROOT_ORDER     4u
```

### `POLY_COEFFICIENT_COUNT`

```c
#define POLY_COEFFICIENT_COUNT(order)   ((order) + 1u)
```

How many numbers a polynomial of the given order holds, which is one more
than the order.

### `POLY_CIRCLE_ROOM`

```c
#define POLY_CIRCLE_ROOM        REAL_C(0.000001)
```

## Functions

### `poly_is_valid_order`

```c
bool poly_is_valid_order(uint32_t order);
```

True if the order is one whose roots this module will find.

### `poly_evaluate`

```c
real_t poly_evaluate(const real_t* coefficient, uint32_t order, real_t x);
```

Give the value of a polynomial at one place.

The work is done from the highest power inwards, which needs one
multiplication and one addition for each order and never forms a power on
its own: forming x to the ninth directly loses digits that this way keeps.

### `poly_evaluate_complex`

```c
cnum_t poly_evaluate_complex(const real_t* coefficient, uint32_t order, cnum_t x);
```

Give the value of a polynomial at one complex place.

A root of a polynomial with real coefficients may be complex, thus checking
one needs this rather than the plain form above.

### `poly_multiply`

```c
bool poly_multiply(const real_t* first, uint32_t first_order, const real_t* second, uint32_t second_order, real_t* answer, uint32_t room);
```

Multiply two polynomials together.

The answer holds POLY_COEFFICIENT_COUNT(first order plus second order)
numbers and room says how many it can hold.

THIS IS HOW A FILTER IS BUILT FROM ITS SECTIONS. Multiplying the denominators
of two biquads gives the denominator of the two in a chain, and its roots are
the poles of the pair.

Give false if the room is too small.

### `poly_derivative`

```c
bool poly_derivative(const real_t* coefficient, uint32_t order, real_t* answer);
```

Give the polynomial whose value is how fast the given one is changing.

The answer is of one order less and holds POLY_COEFFICIENT_COUNT(order - 1)
numbers. Give false where the order is nothing, since a constant does not
change.

### `poly_roots`

```c
bool poly_roots(const real_t* coefficient, uint32_t order, cnum_t* roots);
```

Find where the polynomial crosses nothing.

The roots hold one complex number for each order and come back in no
particular order. A root that is real has an imaginary part of nothing; the
complex ones come in pairs, one the mirror of the other.

Give false if the order is not one poly_is_valid_order accepts, if the
highest coefficient is nothing, which means the polynomial is really of a
lower order, or if the roots could not be found.

### `poly_is_inside_circle`

```c
bool poly_is_inside_circle(const real_t* coefficient, uint32_t order);
```

True if every root of the polynomial lies inside the unit circle.

THIS IS THE STABILITY OF A FILTER, and it is the reason this module exists.
Give the DENOMINATOR of the filter, which for one biquad section of the iir
module is 1, a1, a2.

A pole outside the circle is a filter that runs away: the answer doubles
every few samples until it is nothing but infinities. A pole exactly on the
circle never settles either, thus the room is measured inwards and a root
within POLY_CIRCLE_ROOM of the edge is not counted as inside.

Give false where the roots cannot be found, which is the safe answer: a
filter that cannot be shown to be stable should not be trusted to be.
