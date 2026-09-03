# nolibm

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The arithmetic, without a maths library. Declared in `ffitt/core/nolibm.h`.

[Back to the index](../API.md) | [How the core modules work](../../ffitt/core/README.md)

## Functions

### `nolibm_sqrt`

```c
double nolibm_sqrt(double x);
```

The square root, by the method of Newton from a guess made by halving the
exponent. Gives a number that is not a number for a negative argument.

### `nolibm_hypot`

```c
double nolibm_hypot(double x, double y);
```

The length of the two sides taken together, the larger taken out first so
that squaring cannot run past what the width holds.

### `nolibm_erf`

```c
double nolibm_erf(double x);
```

The error function. A series near nothing, where the answer is small and a
share of it must still be right, and the approximation of Abramowitz and
Stegun beyond.

### `nolibm_sin`

```c
double nolibm_sin(double x);
```

The sine. The angle is brought within a quarter turn of nothing first, thus
a large angle costs a little accuracy in that reduction.

### `nolibm_cos`

```c
double nolibm_cos(double x);
```

The cosine, by the same reduction as the sine.

### `nolibm_tan`

```c
double nolibm_tan(double x);
```

The tangent, as the sine over the cosine. Gives an endless number where the
cosine is nothing.

### `nolibm_fabs`

```c
double nolibm_fabs(double x);
```

The size of a number, with any sign taken off.

### `nolibm_pow`

```c
double nolibm_pow(double x, double y);
```

One number raised to another, as the exponential of the power times the
logarithm. A negative base is answered only for a whole power, which is the
only case with a real answer.

### `nolibm_exp`

```c
double nolibm_exp(double x);
```

The exponential. The argument is split into a whole number of twos and what
is left, and only the small leftover reaches the series.

### `nolibm_log`

```c
double nolibm_log(double x);
```

The natural logarithm. Powers of two are taken out first, and the series in
(m-1)/(m+1) covers the little that is left.

### `nolibm_log10`

```c
double nolibm_log10(double x);
```

The logarithm to the base ten, as the natural one divided by the logarithm
of ten.

### `nolibm_atan`

```c
double nolibm_atan(double x);
```

The arc tangent. Anything above one is turned into its reciprocal and the
range halved again, so that the series stays short and honest.

### `nolibm_atan2`

```c
double nolibm_atan2(double y, double x);
```

The angle of a point from the origin, with the quarter it lies in decided by
the signs of both sides. Gives nothing for a point at the origin.

### `nolibm_sinh`

```c
double nolibm_sinh(double x);
```

The hyperbolic sine. A series near nothing, where taking one nearly equal
number from another would lose every digit, and the exponentials beyond.

### `nolibm_cosh`

```c
double nolibm_cosh(double x);
```

The hyperbolic cosine, from the exponential.

### `nolibm_asin`

```c
double nolibm_asin(double x);
```

The arc sine, by way of the arc tangent. Gives a number that is not a number
outside minus one to one.

### `nolibm_asinh`

```c
double nolibm_asinh(double x);
```

The inverse hyperbolic sine, by way of the logarithm.

### `nolibm_acosh`

```c
double nolibm_acosh(double x);
```

The inverse hyperbolic cosine. Gives a number that is not a number below
one.

### `nolibm_floor`

```c
double nolibm_floor(double x);
```

The largest whole number at or below the argument.

### `nolibm_ceil`

```c
double nolibm_ceil(double x);
```

The smallest whole number at or above the argument.

### `nolibm_fmod`

```c
double nolibm_fmod(double x, double y);
```

What is left when the divisor is taken away as often as it fits. Worked out
by doubling and halving and never by dividing, thus it is exact.
