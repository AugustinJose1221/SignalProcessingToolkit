# interp

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Reading between the points of a table. Declared in `sptk/interpolate/interp.h`.

[Back to the index](../API.md) | [How the interpolate modules work](../../sptk/interpolate/README.md)

## Macros

### `INTERP_SLOPE_COUNT`

```c
#define INTERP_SLOPE_COUNT(size)    (size)
```

How many working values interp_pchip needs for a table of the given size,
which is one slope for each point.

## Functions

### `interp_is_valid_kind`

```c
bool interp_is_valid_kind(interp_kind_t kind);
```

True if the module knows this kind.

### `interp_is_valid_table`

```c
bool interp_is_valid_table(const real_t* input, uint32_t size);
```

True if a table of the given inputs can be read: at least two points, and
the inputs rise through it with no two the same.

Ask this once when the table is set up. Two entries with the same input
would ask the curve to hold two values at one place, and the answer would be
a division by nothing.

### `interp_linear`

```c
real_t interp_linear(const real_t* input, const real_t* output, uint32_t size, real_t place);
```

Read the table at one place, with a straight line between the neighbours.

A place below the first input or above the last is outside the table. The
answer is then the first or the last output, held flat rather than carried
on: a straight line carried on past the end of a calibration says what the
device would read at a temperature it was never calibrated at, and saying
nothing is better than saying that.

### `interp_pchip_slopes`

```c
bool interp_pchip_slopes(const real_t* input, const real_t* output, uint32_t size, real_t* slopes);
```

Work out the slope at each point of the table, for pchip.

The slopes must hold INTERP_SLOPE_COUNT values. This runs once when the
table is set up; interp_pchip then reads the table and the slopes together.

Give false if the table cannot be read.

### `interp_pchip`

```c
real_t interp_pchip(const real_t* input, const real_t* output, const real_t* slopes, uint32_t size, real_t place);
```

Read the table at one place, smoothly and without overshooting.

The slopes must be the ones that interp_pchip_slopes gave for this table.
Outside the table the answer is held flat, as with a straight line.

### `interp_block`

```c
bool interp_block(const real_t* input, const real_t* output, const real_t* slopes, uint32_t size, interp_kind_t kind, const real_t* places, real_t* answers, uint32_t count);
```

Read the table at many places at once, into a list of answers.

For INTERP_PCHIP the slopes must be the ones that interp_pchip_slopes gave.
For INTERP_LINEAR they are not read and may be NULL.

Give false if the table or the kind cannot be used.
