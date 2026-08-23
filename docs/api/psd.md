# psd

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Power at each frequency. Declared in `sptk/transform/psd.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Types

### `psd_t`

```c
typedef struct{
    uint32_t block;             // Samples in one block
    uint32_t overlap;           // Samples that two blocks share
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* spectrum;           // The transform of that block, block values
    fft_t fft;                  // The transform
    real_t window_power;        // The sum of the squares of the window
    bool dynamic_alloc;         // True if the memory comes from the heap
}psd_t;
```

## Functions

### `psd_is_valid_block`

```c
bool psd_is_valid_block(uint32_t block);
```

True if a block of this size can be used. The block must be a size that the
transform can take, which is a power of two.

### `psd_alloc`

```c
psd_t psd_alloc(uint32_t block);
```

Give an estimator for blocks of the given size. The memory comes from the
heap. Give it to psd_free when you no longer need it.

### `psd_static_alloc`

```c
psd_t psd_static_alloc(uint32_t block, real_t* window, real_t* windowed, cnum_t* spectrum, fft_t fft);
```

Give an estimator that uses the memory of the caller. The three lists must
hold as many values as the block, and the transform must be made for the
same block. This function takes no memory from the heap.

### `psd_design`

```c
bool psd_design(psd_t* psd, uint32_t overlap, window_kind_t kind, real_t parameter);
```

Choose the overlap and the window.

The overlap must be below the block. Half the block is the usual choice.
The parameter belongs to the window and is ignored where the window takes
none; window_takes_a_parameter says which do.

Give false if the overlap is not below the block or the window is unknown.

### `psd_bin_count`

```c
uint32_t psd_bin_count(const psd_t* psd);
```

How many values the answer holds, which is half the block plus one.

### `psd_block_count`

```c
uint32_t psd_block_count(const psd_t* psd, uint32_t size);
```

How many blocks a signal of the given size will be cut into.

### `psd_bin_frequency`

```c
real_t psd_bin_frequency(const psd_t* psd, uint32_t bin, real_t sample_rate);
```

The frequency that a bin stands at, in the units of the sample rate.

### `psd_bin_width`

```c
real_t psd_bin_width(const psd_t* psd, real_t sample_rate);
```

The width of one bin, in the units of the sample rate.

This is how finely the answer can be read. A tone that stands between two
bins is seen in both of them and exactly in neither.

### `psd_estimate`

```c
bool psd_estimate(psd_t* psd, const real_t* data, uint32_t size, real_t sample_rate, real_t* output);
```

Work out the density. The output holds psd_bin_count values, and each one is
power for each hertz at the frequency that psd_bin_frequency gives.

Give false if the signal is shorter than one block, or if the estimator has
not been designed.

### `psd_band_power`

```c
real_t psd_band_power(const psd_t* psd, const real_t* density, real_t sample_rate, real_t low, real_t high);
```

Add up the density over a band, which gives the power in that band.

This is what a density is for: the number for one bin means little on its
own, and the area under a stretch of the curve is the power that the signal
holds between those two frequencies.

### `psd_free`

```c
void psd_free(psd_t* psd);
```

Release the memory of an estimator that came from psd_alloc. This function
does nothing for one that came from psd_static_alloc.
