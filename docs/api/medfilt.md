# medfilt

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The median of the last samples. Declared in `ffitt/filter/medfilt.h`.

[Back to the index](../API.md) | [How the filter modules work](../../ffitt/filter/README.md)

## Types

### `medfilt_t`

```c
typedef struct{
    ringbuf_t window;           // The samples in the order they arrived
    real_t* sorted;              // The same samples, in order of value
    bool dynamic_alloc;         // True if the memory comes from the heap
}medfilt_t;
```

## Functions

### `medfilt_alloc`

```c
medfilt_t medfilt_alloc(uint32_t size);
```

Give a filter with a window of the given size. The memory comes from the
heap. Give the filter to medfilt_free when you no longer need it.

### `medfilt_static_alloc`

```c
medfilt_t medfilt_static_alloc(uint32_t size, real_t* window, real_t* sorted);
```

Give a filter that uses the memory of the caller. Both lists must hold as
many float values as the given size. This function takes no memory from the
heap.

### `medfilt_reset`

```c
void medfilt_reset(medfilt_t* medfilt);
```

Forget every sample.

### `medfilt_process_sample`

```c
real_t medfilt_process_sample(medfilt_t* medfilt, real_t sample);
```

Put one sample in and give the median of the window as it now stands.

While the window is still filling, the median is taken over the samples that
have arrived and not over the whole size.

### `medfilt_process_block`

```c
void medfilt_process_block(medfilt_t* medfilt, const real_t* input, real_t* output, uint32_t size);
```

Filter a whole block. The input and the output may be the same list.

### `medfilt_get_median`

```c
real_t medfilt_get_median(medfilt_t* medfilt);
```

Give the median of the window without putting a sample in.

### `medfilt_get_percentile`

```c
real_t medfilt_get_percentile(medfilt_t* medfilt, real_t part);
```

Give the value below which the given part of the window stands. A part of
0.5 gives the median, 0.25 the first quarter.

The window is already held in order, thus this costs nothing more than the
median does. It suits a caller that watches how far a signal spreads as well
as where its middle stands.

### `medfilt_count`

```c
uint32_t medfilt_count(const medfilt_t* medfilt);
```

Give how many samples the window holds now.

### `medfilt_is_full`

```c
bool medfilt_is_full(const medfilt_t* medfilt);
```

True when the window holds as many samples as its size.

### `medfilt_free`

```c
void medfilt_free(medfilt_t* medfilt);
```

Release the memory of a filter that came from medfilt_alloc. This function
does nothing for a filter that came from medfilt_static_alloc.
