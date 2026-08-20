# fir

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Filters with a finite impulse response. Declared in `sptk/filter/fir.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `FIR_TRANSITION`

```c
#define FIR_TRANSITION      REAL_C(2.0)
```

How wide the change from the pass band to the stop band is, as a number
divided by the length of the filter.

A filter with a finite impulse response cannot turn from passing to stopping
at once. The turn takes a band of frequencies, and that band is narrower only
when the filter is longer. This is the width of that turn, and it is the
reason a low cutoff needs a long filter.

## Types

### `fir_t`

```c
typedef struct{
    uint32_t length;            // The number of coefficients
    real_t* coefficient;         // The coefficients
    real_t* history;             // The last samples, length of them
    uint32_t position;          // Where the next sample goes in the history
    bool dynamic_alloc;         // True if the memory comes from the heap
}fir_t;
```

## Functions

### `fir_is_valid_cutoff`

```c
bool fir_is_valid_cutoff(uint32_t length, real_t cutoff);
```

True if a filter of the given length can hold the given cutoff.

The turn from passing to stopping is FIR_TRANSITION/length wide. A cutoff
nearer to 0 than that, or nearer to 0.5 than that, has no room for the turn.
The design then gives back a filter whose pass band never reaches 1, and it
does so quietly.

Measured, for a low pass of 101 coefficients, where the turn is 0.0198 wide,
at the gain that should be 1.0 in the pass band:

    cutoff    0.0500   0.0200   0.0100   0.0050   0.0020
    gain      1.0024   1.0039   0.8443   0.5065   0.2140

The gain holds while the cutoff is above the width of the turn, and falls
away under it. Thus: make the filter longer, or bring the sample rate down.

### `fir_is_valid_band`

```c
bool fir_is_valid_band(uint32_t length, real_t low_cutoff, real_t high_cutoff);
```

True if a filter of the given length can hold the given band. Both edges
must be valid, and the band between them must be at least as wide as the
turn, or the two edges run into each other and no frequency passes fully.

### `fir_alloc`

```c
fir_t fir_alloc(uint32_t length);
```

Give a filter with the given number of coefficients. The memory comes from
the heap, and every coefficient and every sample of the history holds zero.
Give the filter to fir_free when you no longer need it.

### `fir_static_alloc`

```c
fir_t fir_static_alloc(uint32_t length, real_t* coefficient, real_t* history);
```

Give a filter that uses the memory that the caller holds. Both lists must
hold as many float values as the given length. This function takes no
memory from the heap.

### `fir_design_low_pass`

```c
bool fir_design_low_pass(fir_t* fir, real_t cutoff);
```

Build the coefficients of a filter that lets the low frequencies pass. The
cutoff is a part of the sample rate, and it must lie between 0 and 0.5.
Give false and leave the filter as it was if fir_is_valid_cutoff is false.

### `fir_design_high_pass`

```c
bool fir_design_high_pass(fir_t* fir, real_t cutoff);
```

Build the coefficients of a filter that lets the high frequencies pass.
Give false and leave the filter as it was if fir_is_valid_cutoff is false.

### `fir_design_band_pass`

```c
bool fir_design_band_pass(fir_t* fir, real_t low_cutoff, real_t high_cutoff);
```

Build the coefficients of a filter that lets a band of frequencies pass. The
low cutoff must be smaller than the high cutoff, and both must lie between 0
and 0.5.
Give false and leave the filter as it was if fir_is_valid_band is false.

### `fir_set_coefficient`

```c
void fir_set_coefficient(fir_t* fir, uint32_t index, real_t value);
```

Write one coefficient. Use this function to give the filter a set of
coefficients that another program calculated.

### `fir_get_coefficient`

```c
real_t fir_get_coefficient(fir_t* fir, uint32_t index);
```

Give one coefficient.

### `fir_process_sample`

```c
real_t fir_process_sample(fir_t* fir, real_t sample);
```

Give the filtered value of one sample. The filter keeps the sample in its
history, thus the next call sees it.

### `fir_process_block`

```c
void fir_process_block(fir_t* fir, const real_t* input, real_t* output, uint32_t size);
```

Filter a block of samples. The input and the output may be the same list.

### `fir_reset`

```c
void fir_reset(fir_t* fir);
```

Set every sample of the history to zero. The filter then behaves as a filter
that has seen no sample yet.

### `fir_get_gain`

```c
real_t fir_get_gain(fir_t* fir, real_t frequency);
```

Give the size of the answer of the filter at the given frequency, which is a
part of the sample rate. A value of 1 says that the frequency passes
unchanged, and a value of 0 says that the filter stops it.

### `fir_free`

```c
void fir_free(fir_t* fir);
```

Release the memory of a filter that came from fir_alloc. This function does
nothing for a filter that came from fir_static_alloc.
