# csd

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

What two signals have in common. Declared in `sptk/transform/csd.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Macros

### `CSD_SMALLEST_BLOCK_COUNT`

```c
#define CSD_SMALLEST_BLOCK_COUNT    8u
```

### `CSD_BIN_COUNT`

```c
#define CSD_BIN_COUNT(block)        (((block)/2) + 1)
```

How many values an answer holds, which is half the block and one more.

## Types

### `csd_t`

```c
typedef struct{
    uint32_t block;             // Samples in one block
    uint32_t overlap;           // Samples that two blocks share
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* first;              // The transform of a block of the first signal
    cnum_t* second;             // The transform of a block of the second
    cnum_t* cross;              // What the two share, one for each bin
    real_t* first_power;        // What the first holds, one for each bin
    real_t* second_power;       // What the second holds, one for each bin
    fft_t fft;                  // The transform
    real_t window_power;        // The sum of the squares of the window
    bool designed;              // True once csd_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}csd_t;
```

## Functions

### `csd_is_valid_block`

```c
bool csd_is_valid_block(uint32_t block);
```

True if a block of this size can be used, which means a power of two.

### `csd_alloc`

```c
csd_t csd_alloc(uint32_t block);
```

Give an estimator for blocks of the given size. The memory comes from the
heap. Give it to csd_free when it is no longer needed.

### `csd_static_alloc`

```c
csd_t csd_static_alloc(uint32_t block, real_t* window, real_t* windowed, cnum_t* first, cnum_t* second, cnum_t* cross, real_t* first_power, real_t* second_power, fft_t fft);
```

Give an estimator that uses the memory of the caller, taking nothing from
the heap.

The lists window, windowed, first and second must each hold as many values
as the block. The lists cross, first_power and second_power must each hold
CSD_BIN_COUNT(block) values. The transform must be made for the same block.

### `csd_design`

```c
bool csd_design(csd_t* csd, uint32_t overlap, window_kind_t kind, real_t parameter);
```

Choose the overlap and the window. Half the block is the usual overlap.

Give false if the overlap is not below the block or the window is unknown.

### `csd_block_count`

```c
uint32_t csd_block_count(const csd_t* csd, uint32_t size);
```

How many blocks a signal of the given size will be cut into.

### `csd_bin_frequency`

```c
real_t csd_bin_frequency(const csd_t* csd, uint32_t bin, real_t sample_rate);
```

The frequency that a bin stands at, in the units of the sample rate.

### `csd_estimate`

```c
bool csd_estimate(csd_t* csd, const real_t* first, const real_t* second, uint32_t size, real_t sample_rate, cnum_t* output);
```

Work out the cross spectral density of two signals.

The output holds CSD_BIN_COUNT complex numbers. The size of each one says
how much the two signals share at that frequency, and its angle says how far
the second lags behind the first there.

The two signals must hold the same number of samples and must have been
measured at the same moments.

This takes no memory: the working room came with the estimator.

Give false if the estimator has not been designed, or if the signals are
shorter than CSD_SMALLEST_BLOCK_COUNT blocks.

### `csd_coherence`

```c
bool csd_coherence(csd_t* csd, const real_t* first, const real_t* second, uint32_t size, real_t* output);
```

Work out how much of one signal is explained by the other at each frequency,
from 0 to 1.

The output holds CSD_BIN_COUNT values. Read the header before believing any
of them: a reading near 1 divided by the number of blocks is what two
unrelated signals give, and it is evidence of nothing.

Give false for the same reasons as csd_estimate.

### `csd_transfer`

```c
bool csd_transfer(csd_t* csd, const real_t* first, const real_t* second, uint32_t size, cnum_t* output);
```

Work out what one signal does to the other at each frequency: the gain and
the phase shift of whatever lies between them.

The output holds CSD_BIN_COUNT complex numbers, being the cross spectrum
divided by the density of the first signal. This is the estimate that is
blind to noise ADDED TO THE SECOND signal, which is the usual case: the
noise of a sensor at the output does not bend the answer, while noise on the
input does.

LOOK AT THE COHERENCE BESIDE IT. Where the coherence is low the gain here is
still a number, and it is a number about nothing. A transfer estimate
without a coherence beside it is not a measurement.

Give false for the same reasons as csd_estimate.

### `csd_free`

```c
void csd_free(csd_t* csd);
```

Release the memory of an estimator that came from csd_alloc. This does
nothing for one that came from csd_static_alloc.
