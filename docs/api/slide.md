# slide

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

One frequency, answered at every sample. Declared in `ffitt/transform/slide.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `SLIDE_DAMPING`

```c
#define SLIDE_DAMPING       REAL_C(0.9999)
```

The damping a watcher gets unless it asks for another.

### `SLIDE_WINDOW_COUNT`

```c
#define SLIDE_WINDOW_COUNT(size)        (size)
```

How many values the memory of a watcher must hold, for the caller who gives
that memory rather than taking it from the heap. The window is real values;
the totals and the turning factors are complex.

### `SLIDE_BIN_COUNT`

```c
#define SLIDE_BIN_COUNT(count)          (count)
```

### `SLIDE_TURN_COUNT`

```c
#define SLIDE_TURN_COUNT(count)         (count)
```

## Types

### `slide_t`

```c
typedef struct{
    ringbuf_t history;          // The window, so that what left can be taken
    cnum_t* total;              // One running total for each frequency watched
    cnum_t* turn;               // The turning factor of each, damping included
    real_t departing;           // What the sample that left is multiplied by
    real_t damping;             // A shade below one, which keeps it stable
    uint32_t size;              // The window, which is the size of the bins
    uint32_t count;             // How many frequencies are watched
    uint32_t seen;              // How many samples have arrived
    bool dynamic_alloc;         // True if the memory comes from the heap
}slide_t;
```

## Functions

### `slide_is_valid_size`

```c
bool slide_is_valid_size(uint32_t size);
```

The bins are of a transform of this many samples, thus the size decides both
the window and where the bins fall. It must be at least two.

### `slide_is_valid_damping`

```c
bool slide_is_valid_damping(real_t damping);
```

True if the damping is one this module will take. It must be above nothing
and not above one. A damping of exactly one switches the mending off, and
the header above says what that costs.

### `slide_alloc`

```c
slide_t slide_alloc(uint32_t size, uint32_t count);
```

Give a watcher of the given number of frequencies over a window of the given
size. The memory comes from the heap. Give it to slide_free.

Nothing is watched until slide_watch says which bins, thus a watcher fresh
from here answers nothing.

### `slide_static_alloc`

```c
slide_t slide_static_alloc(uint32_t size, uint32_t count, real_t* window, cnum_t* total, cnum_t* turn);
```

Give a watcher that uses the memory the caller holds. The window must hold
SLIDE_WINDOW_COUNT values, and the two complex lists SLIDE_BIN_COUNT and
SLIDE_TURN_COUNT. This function takes no memory from the heap.

### `slide_design`

```c
bool slide_design(slide_t* slide, real_t damping);
```

Say how quickly an error fades. Give SLIDE_DAMPING unless the header above
gives a reason to give another. This forgets every total, because a change
of damping changes what the totals mean.

Give false if the damping is one slide_is_valid_damping refuses.

### `slide_watch`

```c
bool slide_watch(slide_t* slide, uint32_t index, uint32_t bin);
```

Watch the given bin of the transform with the given watcher.

The bin runs from 0 to the size, and slide_bin_frequency says what frequency
each one stands at. A bin above half the size is the mirror of one below it
and says nothing new about a real signal.

Give false if either number is outside what this watcher holds.

### `slide_bin_frequency`

```c
real_t slide_bin_frequency(const slide_t* slide, uint32_t bin, real_t sample_rate);
```

The frequency that a bin stands at, in the same unit as the sample rate.

### `slide_reset`

```c
void slide_reset(slide_t* slide);
```

Forget every sample and every total. What is watched is kept.

### `slide_process_sample`

```c
void slide_process_sample(slide_t* slide, real_t sample);
```

Give one sample to the watcher. Every total moves.

### `slide_process_block`

```c
void slide_process_block(slide_t* slide, const real_t* input, uint32_t count);
```

Give a whole block, one sample at a time. The totals afterwards are those of
the last sample of the block.

### `slide_is_full`

```c
bool slide_is_full(const slide_t* slide);
```

True once as many samples have arrived as the window holds.

READ THIS BEFORE THE ANSWER. Until the window is full the totals are of a
window that is partly nothing, thus they are low, and how low depends on how
far through the filling they are. They are not wrong so much as not yet
about anything.

### `slide_get`

```c
cnum_t slide_get(const slide_t* slide, uint32_t index);
```

The running total of one watcher, as a complex number.

### `slide_magnitude`

```c
real_t slide_magnitude(const slide_t* slide, uint32_t index);
```

How large the wave at one watcher's bin is, which is the size of its total.

### `slide_free`

```c
void slide_free(slide_t* slide);
```

Release the memory of a watcher that came from slide_alloc. This does
nothing for one that came from slide_static_alloc, thus a call for either
kind is safe.
