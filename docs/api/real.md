# real

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The one type that holds every number. Declared in `sptk/core/real.h`.

[Back to the index](../API.md) | [How the core modules work](../../sptk/core/README.md)

## Macros

### `REAL_C`

```c
#define REAL_C(x)       (x)
```

### `REAL_EPSILON`

```c
#define REAL_EPSILON    DBL_EPSILON
```

### `REAL_DIGITS`

```c
#define REAL_DIGITS     DBL_DIG
```

### `REAL_LARGEST`

```c
#define REAL_LARGEST    DBL_MAX
```

### `REAL_SMALLEST`

```c
#define REAL_SMALLEST   DBL_MIN
```

### `REAL_SQRT`

```c
#define REAL_SQRT(x)        sqrt(x)
```

### `REAL_SIN`

```c
#define REAL_SIN(x)         sin(x)
```

### `REAL_COS`

```c
#define REAL_COS(x)         cos(x)
```

### `REAL_TAN`

```c
#define REAL_TAN(x)         tan(x)
```

### `REAL_ABS`

```c
#define REAL_ABS(x)         fabs(x)
```

### `REAL_POW`

```c
#define REAL_POW(x, y)      pow((x), (y))
```

### `REAL_EXP`

```c
#define REAL_EXP(x)         exp(x)
```

### `REAL_LOG`

```c
#define REAL_LOG(x)         log(x)
```

### `REAL_LOG10`

```c
#define REAL_LOG10(x)       log10(x)
```

### `REAL_ATAN2`

```c
#define REAL_ATAN2(y, x)    atan2((y), (x))
```

### `REAL_FLOOR`

```c
#define REAL_FLOOR(x)       floor(x)
```

### `REAL_CEIL`

```c
#define REAL_CEIL(x)        ceil(x)
```

### `REAL_FMOD`

```c
#define REAL_FMOD(x, y)     fmod((x), (y))
```

### `REAL_C`

```c
#define REAL_C(x)       (x##f)
```

### `REAL_EPSILON`

```c
#define REAL_EPSILON    FLT_EPSILON
```

### `REAL_DIGITS`

```c
#define REAL_DIGITS     FLT_DIG
```

### `REAL_LARGEST`

```c
#define REAL_LARGEST    FLT_MAX
```

### `REAL_SMALLEST`

```c
#define REAL_SMALLEST   FLT_MIN
```

### `REAL_SQRT`

```c
#define REAL_SQRT(x)        sqrtf(x)
```

### `REAL_SIN`

```c
#define REAL_SIN(x)         sinf(x)
```

### `REAL_COS`

```c
#define REAL_COS(x)         cosf(x)
```

### `REAL_TAN`

```c
#define REAL_TAN(x)         tanf(x)
```

### `REAL_ABS`

```c
#define REAL_ABS(x)         fabsf(x)
```

### `REAL_POW`

```c
#define REAL_POW(x, y)      powf((x), (y))
```

### `REAL_EXP`

```c
#define REAL_EXP(x)         expf(x)
```

### `REAL_LOG`

```c
#define REAL_LOG(x)         logf(x)
```

### `REAL_LOG10`

```c
#define REAL_LOG10(x)       log10f(x)
```

### `REAL_ATAN2`

```c
#define REAL_ATAN2(y, x)    atan2f((y), (x))
```

### `REAL_FLOOR`

```c
#define REAL_FLOOR(x)       floorf(x)
```

### `REAL_CEIL`

```c
#define REAL_CEIL(x)        ceilf(x)
```

### `REAL_FMOD`

```c
#define REAL_FMOD(x, y)     fmodf((x), (y))
```

### `REAL_PI`

```c
#define REAL_PI         REAL_C(3.14159265358979323846)
```

The number pi, at the width of the build.

## Functions

### `real_sin`

```c
real_t real_sin(real_t x);
```

The functions of mathematics again, as functions and not as macros.

The macros above cost nothing, because the compiler puts the right call in
where they stand. But a macro has no address, thus none of them can be
GIVEN to something that takes a function.

The pmatrix module holds a function for each of its elements, and before
real_t existed a caller could give it sinf directly. That no longer works
and, worse, it does not fail to build: sinf takes a float, the module calls
it through a pointer that takes a real_t, and in a 64 bit build the two do
not agree. The answer is then not wrong by a little but nonsense.

These give a name with an address that always agrees with real_t. Use the
macros in ordinary code and these only where a function must be handed over.
The sine of x, where x is an angle in radians.

### `real_cos`

```c
real_t real_cos(real_t x);
```

The cosine of x, where x is an angle in radians.

### `real_tan`

```c
real_t real_tan(real_t x);
```

The tangent of x, where x is an angle in radians.

### `real_sqrt`

```c
real_t real_sqrt(real_t x);
```

The square root of x.

### `real_exp`

```c
real_t real_exp(real_t x);
```

The number e raised to the power x.

### `real_log`

```c
real_t real_log(real_t x);
```

The logarithm of x to the base e.

### `real_abs`

```c
real_t real_abs(real_t x);
```

The size of x, without its sign.
