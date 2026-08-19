# movavg

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The mean of the last samples. Declared in `sptk/filter/movavg.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `MOVAVG_REFRESH`

```c
#define MOVAVG_REFRESH      4096u
```

How many samples pass before the totals are worked out again from the whole
window. The reason stands at movavg_process_sample.

## Types

### `movavg_t`

```c
typedef struct{
    ringbuf_t window;           // The samples of the window
    double total;               // The running sum of the samples
    double square_total;        // The running sum of the squares
    uint32_t since_refresh;     // Samples since the totals were built again
}movavg_t;
```

## Functions

### `movavg_alloc`

```c
movavg_t movavg_alloc(uint32_t size);
```

Give a filter with a window of the given size. The memory comes from the
heap. Give the filter to movavg_free when you no longer need it.

### `movavg_static_alloc`

```c
movavg_t movavg_static_alloc(uint32_t size, float* data);
```

Give a filter that uses the memory at data, which must hold as many float
values as the given size. This function takes no memory from the heap.

### `movavg_reset`

```c
void movavg_reset(movavg_t* movavg);
```

Forget every sample and every total.

### `movavg_process_sample`

```c
float movavg_process_sample(movavg_t* movavg, float sample);
```

Put one sample in and give the mean of the window as it now stands.

While the window is still filling, the mean is taken over the samples that
have arrived and not over the whole size. Thus the answer is right from the
first sample and does not start low.

### `movavg_process_block`

```c
void movavg_process_block(movavg_t* movavg, const float* input, float* output, uint32_t size);
```

Filter a whole block. The input and the output may be the same list.

### `movavg_get_mean`

```c
float movavg_get_mean(const movavg_t* movavg);
```

Give the mean of the window without putting a sample in.

### `movavg_get_rms`

```c
float movavg_get_rms(const movavg_t* movavg);
```

Give the root of the mean of the squares of the window.

This follows the level of the signal and not how much it moves. For a signal
that sits at 100 and wanders by 1 it gives about 100, where the deviation
gives 1. Take this one for the energy or the power of a signal.

### `movavg_get_deviation`

```c
float movavg_get_deviation(const movavg_t* movavg);
```

Give the standard deviation of the window.

THIS ONE READS THE WHOLE WINDOW, thus it costs one pass and not a fixed
time. The other two are held as running totals and this cannot be, for a
reason worth knowing:

A deviation from running totals would be the mean of the squares less the
square of the mean. Those two numbers are nearly equal whenever the signal
sits far from zero, and their difference is the answer. A reading that sits
at 8 000 000 and moves by 1 gives two numbers near 64 000 000 000 000 whose
difference is 1, and that difference is lost.

Reading the window and taking the mean away from each sample first has no
such trouble. It costs a pass over the window, thus call it when the answer
is wanted and not for every sample.

### `movavg_count`

```c
uint32_t movavg_count(const movavg_t* movavg);
```

Give how many samples the window holds now.

### `movavg_is_full`

```c
bool movavg_is_full(const movavg_t* movavg);
```

True when the window holds as many samples as its size.

### `movavg_free`

```c
void movavg_free(movavg_t* movavg);
```

Release the memory of a filter that came from movavg_alloc. This function
does nothing for a filter that came from movavg_static_alloc.
