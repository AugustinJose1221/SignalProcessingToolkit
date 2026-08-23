# hampel

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Replacing only the samples that are wrong. Declared in `sptk/filter/hampel.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `HAMPEL_THRESHOLD`

```c
#define HAMPEL_THRESHOLD    REAL_C(3.0)
```

How many deviations away a sample must stand before it is called wrong.

### `HAMPEL_SCALE`

```c
#define HAMPEL_SCALE        REAL_C(1.4826)
```

What the median absolute deviation must be multiplied by to stand beside a
standard deviation, for samples that follow a normal spread.

## Types

### `hampel_t`

```c
typedef struct{
    medfilt_t middle;           // The window, held in order
    ringbuf_t history;          // The same samples in the order they arrived
    real_t* distance;           // Working room, one for each sample of the window
    real_t threshold;           // How many deviations count as too far
    uint32_t replaced;          // How many samples have been replaced
    uint32_t seen;              // How many samples have arrived
    bool dynamic_alloc;         // True if the memory comes from the heap
}hampel_t;
```

## Functions

### `hampel_is_valid_window`

```c
bool hampel_is_valid_window(uint32_t window);
```

True if a window of this size can be used. It must be odd and at least 3: an
odd window has a true middle, and a window of one has no neighbours to judge
a sample against.

### `hampel_alloc`

```c
hampel_t hampel_alloc(uint32_t window);
```

Give a filter with a window of the given size. The memory comes from the
heap. Give the filter to hampel_free when you no longer need it.

### `hampel_static_alloc`

```c
hampel_t hampel_static_alloc(uint32_t window, real_t* sorted, real_t* ordered, real_t* history, real_t* distance);
```

Give a filter that uses the memory of the caller. Each of the four lists
must hold as many values as the window. This function takes no memory from
the heap.

### `hampel_set_threshold`

```c
bool hampel_set_threshold(hampel_t* hampel, real_t threshold);
```

Set how many deviations away a sample must stand before it is replaced.
Give false if the threshold is not above zero.

### `hampel_reset`

```c
void hampel_reset(hampel_t* hampel);
```

Forget every sample and every count.

### `hampel_delay`

```c
uint32_t hampel_delay(const hampel_t* hampel);
```

How many samples the answer comes behind the input, which is half the
window.

### `hampel_process_sample`

```c
real_t hampel_process_sample(hampel_t* hampel, real_t sample, bool* was_replaced);
```

Put one sample in and give the answer for the sample that arrived
hampel_delay samples ago.

While the window is still filling the filter gives back the samples as they
came, because it cannot yet judge them. Set was_replaced to say whether the
sample it gives back was changed; give NULL if that is not wanted.

### `hampel_process_block`

```c
uint32_t hampel_process_block(hampel_t* hampel, const real_t* input, real_t* output, uint32_t size);
```

Clean a whole signal. The output holds as many samples as the input, and the
two may be the same list.

This puts the delay right, thus output[k] is the answer for input[k]. The
samples at the two ends have no neighbours on one side; they are passed
through as they arrived rather than judged against a window that is not
there.

Give how many samples were replaced.

### `hampel_replaced_count`

```c
uint32_t hampel_replaced_count(const hampel_t* hampel);
```

How many samples this filter has replaced since it was last reset.

Read this. It is the measure of how much was wrong with the signal, and a
number that climbs is a fault in the wiring and not in the filter. A
recording where one sample in fifty is replaced is a recording to look at
rather than to trust.

### `hampel_free`

```c
void hampel_free(hampel_t* hampel);
```

Release the memory of a filter that came from hampel_alloc. This function
does nothing for one that came from hampel_static_alloc.
