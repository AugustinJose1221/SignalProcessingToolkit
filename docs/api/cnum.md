# cnum

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Complex numbers. Declared in `sptk/linalg/cnum.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Types

### `cnum_t`

```c
typedef struct{
    real_t re;                   // The real part
    real_t im;                   // The imaginary part
}cnum_t;
```

## Functions

### `cnum_make`

```c
cnum_t cnum_make(real_t re, real_t im);
```

Give a complex number with the given real part and imaginary part.

### `cnum_from_real`

```c
cnum_t cnum_from_real(real_t re);
```

Give a complex number whose imaginary part is zero.

### `cnum_zero`

```c
cnum_t cnum_zero(void);
```

Give the number zero.

### `cnum_one`

```c
cnum_t cnum_one(void);
```

Give the number one.

### `cnum_add`

```c
cnum_t cnum_add(cnum_t a, cnum_t b);
```

Give the sum of the two numbers.

### `cnum_subtract`

```c
cnum_t cnum_subtract(cnum_t a, cnum_t b);
```

Give the first number less the second one.

### `cnum_multiply`

```c
cnum_t cnum_multiply(cnum_t a, cnum_t b);
```

Give the product of the two numbers.

### `cnum_divide`

```c
cnum_t cnum_divide(cnum_t a, cnum_t b);
```

Give the quotient of the two numbers. If the second number is zero, the
quotient has no value, and the function gives zero.

### `cnum_scale`

```c
cnum_t cnum_scale(cnum_t a, real_t factor);
```

Give the number with both parts multiplied by a real factor.

### `cnum_conjugate`

```c
cnum_t cnum_conjugate(cnum_t a);
```

Give the number with the sign of the imaginary part changed.

### `cnum_negate`

```c
cnum_t cnum_negate(cnum_t a);
```

Give the number with the sign of both parts changed.

### `cnum_real`

```c
real_t cnum_real(cnum_t a);
```

Give the real part of the number.

### `cnum_imaginary`

```c
real_t cnum_imaginary(cnum_t a);
```

Give the imaginary part of the number.

### `cnum_magnitude`

```c
real_t cnum_magnitude(cnum_t a);
```

Give the distance of the number from zero.

### `cnum_magnitude_squared`

```c
real_t cnum_magnitude_squared(cnum_t a);
```

Give the square of the distance from zero. This function does not take a
square root, thus it is faster than cnum_magnitude and it keeps more of the
accuracy. Use it when you only compare two distances.

### `cnum_is_zero`

```c
bool cnum_is_zero(cnum_t a);
```

True if both parts of the number are zero.

### `cnum_is_equal`

```c
bool cnum_is_equal(cnum_t a, cnum_t b);
```

True if both parts of the two numbers are the same.

### `cnum_is_near`

```c
bool cnum_is_near(cnum_t a, cnum_t b, real_t tolerance);
```

Give true if the two numbers differ by less than the tolerance. A float
keeps about 7 digits, thus a calculation with several steps gives a result
that is near the correct value but not equal to it.
