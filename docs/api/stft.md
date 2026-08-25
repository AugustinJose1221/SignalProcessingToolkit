# stft

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The transform in short pieces. Declared in `sptk/transform/stft.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Macros

### `STFT_SMALLEST_WEIGHT_PART`

```c
#define STFT_SMALLEST_WEIGHT_PART   REAL_C(0.001)
```

The smallest weight a sample may carry, as a part of the largest, before
stft_can_rebuild says no.

Putting the pieces back divides each sample by the weight the windows laid
on it. A sample carrying almost no weight is almost not there, and dividing
it back up lifts whatever rounding it holds by the same amount. This holds
that lift to a thousand.

### `STFT_BIN_COUNT`

```c
#define STFT_BIN_COUNT(block)       (((block)/2) + 1)
```

How many bins one frame holds, which is half the block and one more.

## Types

### `stft_t`

```c
typedef struct{
    uint32_t block;             // Samples in one block, a power of two
    uint32_t hop;               // Samples from the start of one block to the next
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* spectrum;           // The transform of that block, block values
    fft_t fft;                  // The transform
    bool designed;              // True once stft_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}stft_t;
```

## Functions

### `stft_is_valid_block`

```c
bool stft_is_valid_block(uint32_t block);
```

True if a block of this size can be used, which means a power of two, since
the transform underneath takes nothing else.

### `stft_is_valid_hop`

```c
bool stft_is_valid_hop(uint32_t block, uint32_t hop);
```

True if this hop can be used with this block. The hop must be from 1 up to
the block itself.

### `stft_frame_count`

```c
uint32_t stft_frame_count(uint32_t size, uint32_t block, uint32_t hop);
```

How many frames a signal of the given size gives.

Only whole blocks are taken. A signal shorter than one block gives none, and
the samples at the end that do not fill a block are not transformed. Where
they matter, add zeros to the signal until they do.

### `stft_signal_size`

```c
uint32_t stft_signal_size(uint32_t frames, uint32_t block, uint32_t hop);
```

How many samples come back from this many frames.

This is the room stft_inverse needs, and it is NOT the size of the signal
that went in: the samples at the end that did not fill a whole block are not
there to come back.

### `stft_alloc`

```c
stft_t stft_alloc(uint32_t block);
```

Give a transform for blocks of the given size. The memory comes from the
heap. Give it to stft_free when it is no longer needed.

A transform whose block is 0 came back because the block is not one the
module takes.

### `stft_static_alloc`

```c
stft_t stft_static_alloc(uint32_t block, real_t* window, real_t* windowed, cnum_t* spectrum, fft_t fft);
```

Give a transform that uses the memory the caller holds, taking nothing from
the heap. The two lists of real values and the list of complex numbers must
each hold as many values as the block, and the transform must be made for
the same block.

### `stft_design`

```c
bool stft_design(stft_t* stft, uint32_t hop, window_kind_t kind, real_t parameter);
```

Choose the hop and the window.

The hop is the distance from the start of one block to the start of the
next, thus a hop of half the block means the blocks overlap by half. Half is
the usual choice and rebuilds exactly with every window here.

The parameter belongs to the window and is ignored where the window takes
none; window_takes_a_parameter says which do.

Give false if the hop is not from 1 to the block, or the window is unknown.

### `stft_can_rebuild`

```c
bool stft_can_rebuild(const stft_t* stft);
```

True if the window and the hop cover every sample well enough to put the
signal back together.

Call this once after stft_design rather than finding out from a rebuilt
signal that is quietly wrong at the ends of every block.

### `stft_forward`

```c
bool stft_forward(stft_t* stft, const real_t* signal, uint32_t size, cnum_t* output, uint32_t room);
```

Cut the signal into blocks, window each one and transform it.

The output holds stft_frame_count times STFT_BIN_COUNT complex numbers, and
room says how many it can hold. The frames lie one after another.

Give false if the transform has not been designed, if the signal is shorter
than one block, or if the room is too small.

### `stft_solid_range`

```c
bool stft_solid_range(const stft_t* stft, uint32_t frame_count, uint32_t* first, uint32_t* count);
```

Give the stretch of the output where the windows covered the samples fully,
and where the answer is therefore exact.

The first index and the number of samples are written out. Outside that
stretch stft_inverse writes zero, because the samples at the two ends of the
signal are covered by fewer blocks than the ones in the middle and cannot be
recovered. Where the ends matter, put a block of zeros before the signal and
another after it.

Give false if the transform has not been designed, if there are no frames,
or IF THERE ARE TOO FEW FRAMES FOR ANY SAMPLE TO BE COVERED FULLY. That last
one is easy to meet by accident: a sample in the middle is under as many
blocks as fit across it, thus the block divided by the hop is the fewest
frames that can leave any sample solid at all. A block of 8 at a hop of 2
needs 4 frames, and 3 frames leave nothing to give back.

### `stft_inverse`

```c
bool stft_inverse(stft_t* stft, const cnum_t* frames, uint32_t frame_count, real_t* output, uint32_t room, real_t* weight);
```

Put the frames back together into a signal.

Each frame is brought back to samples, windowed a second time and added
where it belongs, and then every sample is divided by the weight the windows
laid on it. Windowing a second time is what keeps the joins from showing
when the frames have been changed in between, which is the usual reason for
taking a signal apart at all.

The output holds stft_signal_size values and the weight buffer holds the
same, and it loses its content. Outside the stretch that stft_solid_range
gives, the output is set to zero.

Give false if the transform has not been designed, if stft_can_rebuild is
false, if the room is too small, or if stft_solid_range gives false because
there are too few frames for any sample to be covered fully. Nothing could
be given back in that last case, thus nothing is: the answer would be a
buffer of zeros wearing the look of a signal.

### `stft_bin_frequency`

```c
real_t stft_bin_frequency(const stft_t* stft, uint32_t bin, real_t sample_rate);
```

The frequency that a bin stands at, in the units of the sample rate.

### `stft_frame_time`

```c
real_t stft_frame_time(const stft_t* stft, uint32_t frame, real_t sample_rate);
```

The time that a frame stands at, in seconds, taken at the MIDDLE of its
block.

The middle and not the start, because a window weighs the middle of its
block most heavily and that is where the answer of that frame really sits.

### `stft_free`

```c
void stft_free(stft_t* stft);
```

Release the memory of a transform that came from stft_alloc. This does
nothing for one that came from stft_static_alloc.
