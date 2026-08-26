# quantise

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Putting a signal into steps. Declared in `sptk/util/quantise.h`.

[Back to the index](../API.md) | [How the util modules work](../../sptk/util/README.md)

## Macros

### `QUANTISE_LARGEST_BITS`

```c
#define QUANTISE_LARGEST_BITS       24u
```

The most steps a quantiser may have, which is what 24 bits holds.

Beyond this the step is smaller than the smallest difference a 32 bit number
can tell across the range, and the quantiser stops quantising.

## Types

### `quantise_t`

```c
typedef struct{
    quantise_way_t way;         // Which way the rounding is done
    real_t step;                // How far apart two steps stand
    real_t reach;               // The largest value that fits
    real_t carried;             // The error of the sample before, for shaping
    uint32_t seed;              // Where the dither stands
    bool designed;              // True once quantise_design has been called
}quantise_t;
```

## Functions

### `quantise_is_valid_way`

```c
bool quantise_is_valid_way(quantise_way_t way);
```

True if the way is one this module knows.

### `quantise_is_valid_bits`

```c
bool quantise_is_valid_bits(uint32_t bits);
```

True if a quantiser of this many bits can be made.

### `quantise_make`

```c
quantise_t quantise_make(void);
```

Give a quantiser. It takes no memory at all, thus there is no free and one
may be made on the stack.

### `quantise_design`

```c
bool quantise_design(quantise_t* quantise, quantise_way_t way, uint32_t bits, real_t reach);
```

Choose how many steps there are and how far the signal reaches.

The bits say how many steps: 8 bits is 256 of them. The reach is the largest
value that fits, thus a signal running from -1 to 1 has a reach of 1. A
value beyond the reach is held at it rather than wrapping round, because a
signal that wraps does not sound loud, it sounds broken.

Give false if the way or the number of bits is one the module cannot use, or
if the reach is not above nothing.

### `quantise_set_seed`

```c
void quantise_set_seed(quantise_t* quantise, uint32_t seed);
```

Set where the dither starts, so that a run can be repeated exactly.

### `quantise_sample`

```c
real_t quantise_sample(quantise_t* quantise, real_t sample);
```

Put one sample into steps.

### `quantise_block`

```c
bool quantise_block(quantise_t* quantise, const real_t* input, real_t* output, uint32_t count);
```

Put a list of samples into steps. The output may be the input.

Give false if the quantiser has not been designed.

### `quantise_step_of`

```c
real_t quantise_step_of(const quantise_t* quantise);
```

How far apart two steps stand, which is the size of the error before
anything is done about its shape.

### `quantise_noise_floor`

```c
real_t quantise_noise_floor(uint32_t bits);
```

How far down the noise of a quantiser of this many bits should lie, in
decibels, against a signal that fills its whole reach.

This is the number every converter is sold on: about 6 dB for each bit. Use
it to see whether a measurement is meeting what the converter can do, or
whether something else is in the way.

### `quantise_reset`

```c
void quantise_reset(quantise_t* quantise);
```

Put the carried error back to nothing, without changing the design.
