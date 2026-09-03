# resample

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Changing the rate of a signal. Declared in `ffitt/filter/resample.h`.

[Back to the index](../API.md) | [How the filter modules work](../../ffitt/filter/README.md)

## Macros

### `RESAMPLE_DECIMATOR_MEMPOOL_SIZE`

```c
#define RESAMPLE_DECIMATOR_MEMPOOL_SIZE(length)         (3u * (length))
```

How many values the memory of a decimator must hold, for the caller who
gives that memory rather than taking it from the heap.

The filter keeps its coefficients and its history, which is the length
twice, and the resampler keeps the samples at the input rate, which is the
length once more.

### `RESAMPLE_INTERPOLATOR_MEMPOOL_SIZE`

```c
#define RESAMPLE_INTERPOLATOR_MEMPOOL_SIZE(factor, length) \
```

How many values the memory of an interpolator must hold.

The filter keeps the same two lists. The history is shorter here: one input
sample feeds every factor-th coefficient, thus only the length divided by
the factor, rounded up, is ever read.

## Types

### `resample_t`

```c
typedef struct{
    fir_t filter;               // The filter that keeps the aliases out
    ringbuf_t history;          // The last samples at the input rate
    uint32_t factor;            // How many samples in for each one out
    uint32_t phase;             // Where the next output falls
    bool dynamic_alloc;         // True if the memory comes from the heap
}resample_t;
```

## Functions

### `resample_advised_length`

```c
uint32_t resample_advised_length(uint32_t factor);
```

How many coefficients a filter needs for a given factor, as a rule of thumb.

This gives a turn of about a fifth of the new rate, and a stop band about
60 dB down. It is a starting point and not a law: a caller who needs a
sharper edge gives a longer filter, and one who can accept a softer edge
saves work with a shorter one.

THE 60 dB IS THE STOP BAND AND NOT THE EDGE OF IT. A frequency just above
half the new rate does not sit in the stop band at all: it sits on the turn,
where the filter is still on its way down. Measured, swept finely right up
to the edge:

    factor      worst rejection near the edge
       2               52.7 dB
       3               53.2 dB
       4               53.0 dB
       5               53.7 dB
       8               53.9 dB

About 53 dB for every factor, and better everywhere further in. A caller who
needs the full 60 dB right at the edge gives a longer filter.

### `resample_is_valid_factor`

```c
bool resample_is_valid_factor(uint32_t factor);
```

True if this factor can be used. It must be 2 or more; a factor of 1 changes
nothing and a factor of 0 means nothing.

### `resample_alloc_decimator`

```c
resample_t resample_alloc_decimator(uint32_t factor, uint32_t length);
```

Give a decimator that keeps one sample for each factor, with a filter of the
given length. The memory comes from the heap.

Give resample_advised_length(factor) for the length unless there is a reason
to give another. The length must be odd, so that the filter has a middle and
delays every frequency by the same time.

### `resample_alloc_interpolator`

```c
resample_t resample_alloc_interpolator(uint32_t factor, uint32_t length);
```

Give an interpolator that makes factor samples for each one, with a filter
of the given length. The memory comes from the heap.

### `resample_static_alloc_decimator`

```c
resample_t resample_static_alloc_decimator(uint32_t factor, uint32_t length, real_t* mempool);
```

Give a decimator that uses the memory the caller holds, which must hold as
many values as RESAMPLE_DECIMATOR_MEMPOOL_SIZE gives for the same length.
This function takes no memory from the heap and cannot fail for want of it.

THE COEFFICIENTS ARE STILL WORKED OUT HERE, thus this call does the same
arithmetic as the one that takes memory from the heap. It is the memory that
the caller has taken over and not the design.

A decimator built this way is given to resample_free like any other, and
that call then does nothing, thus one road serves both kinds.

### `resample_static_alloc_interpolator`

```c
resample_t resample_static_alloc_interpolator(uint32_t factor, uint32_t length, real_t* mempool);
```

Give an interpolator that uses the memory the caller holds, which must hold
as many values as RESAMPLE_INTERPOLATOR_MEMPOOL_SIZE gives for the same
factor and length. This function takes no memory from the heap.

### `resample_reset`

```c
void resample_reset(resample_t* resample);
```

Forget every sample. The filter keeps its coefficients.

### `resample_decimate`

```c
bool resample_decimate(resample_t* resample, real_t sample, real_t* output);
```

Put one sample into a decimator.

Give true when an output sample is ready, and write it into output. That
happens once for each factor samples put in.

### `resample_interpolate`

```c
uint32_t resample_interpolate(resample_t* resample, real_t sample, real_t* output);
```

Put one sample into an interpolator and write the factor samples that come
out of it.

The output must hold as many values as the factor. Give how many were
written, which is always the factor.

### `resample_decimate_block`

```c
uint32_t resample_decimate_block(resample_t* resample, const real_t* input, real_t* output, uint32_t size);
```

Run a whole block through a decimator. The output must have room for
size/factor samples. Give how many were written.

### `resample_interpolate_block`

```c
uint32_t resample_interpolate_block(resample_t* resample, const real_t* input, real_t* output, uint32_t size);
```

Run a whole block through an interpolator. The output must have room for
size*factor samples. Give how many were written.

### `resample_delay`

```c
uint32_t resample_delay(const resample_t* resample);
```

How many samples the answer comes behind the input, counted at the OUTPUT
rate.

A filter with a middle delays every frequency by half its length. For a
decimator that is half the length divided by the factor, because the output
samples are further apart.

### `resample_free`

```c
void resample_free(resample_t* resample);
```

Release the memory of a resampler that came from one of the alloc functions.
