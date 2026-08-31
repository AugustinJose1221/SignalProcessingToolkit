# stats

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Measures of a list of samples. Declared in `ffitt/util/stats.h`.

[Back to the index](../API.md) | [How the util modules work](../../ffitt/util/README.md)

## Macros

### `STATS_MAD_TO_DEVIATION`

```c
#define STATS_MAD_TO_DEVIATION      REAL_C(1.4826)
```

What the median absolute deviation must be multiplied by to estimate the
standard deviation of samples that follow a normal spread.

The number is 1/0.6745, because for a normal spread the median absolute
deviation is 0.6745 of the deviation.

## Functions

### `stats_sum`

```c
real_t stats_sum(const real_t* data, uint32_t size);
```

Give the sum of the samples.

### `stats_mean`

```c
real_t stats_mean(const real_t* data, uint32_t size);
```

Give the mean of the samples.

### `stats_variance`

```c
real_t stats_variance(const real_t* data, uint32_t size);
```

Give the variance of the samples, divided by the number of samples.

This is the variance of the list as it stands. To estimate the variance of
the thing the list was drawn FROM, multiply by size/(size-1).

### `stats_deviation`

```c
real_t stats_deviation(const real_t* data, uint32_t size);
```

Give the standard deviation, which is the root of the variance.

### `stats_rms`

```c
real_t stats_rms(const real_t* data, uint32_t size);
```

Give the root of the mean of the squares.

This is not the deviation. The root mean square holds the mean inside it,
thus for a signal that sits at 100 and wanders by 1 it gives about 100. The
deviation gives 1. Take this one for the power of a signal and the other one
for how much the signal moves.

### `stats_min`

```c
real_t stats_min(const real_t* data, uint32_t size);
```

Give the smallest sample.

### `stats_max`

```c
real_t stats_max(const real_t* data, uint32_t size);
```

Give the largest sample.

### `stats_median`

```c
real_t stats_median(real_t* data, uint32_t size);
```

Give the median of the samples.

THIS FUNCTION REORDERS THE LIST. It has to put the samples in order to find
the middle one, and it does that in the memory of the caller so that it
needs none of its own. Copy the list first if the order matters.

For a list of an even size the median lies between the two middle samples,
and the function gives their mean.

### `stats_percentile`

```c
real_t stats_percentile(real_t* data, uint32_t size, real_t part);
```

Give the sample below which the given part of the list stands. A part of 0.5
gives the median, 0.25 the first quarter, 0.9 the ninth tenth.

THIS FUNCTION REORDERS THE LIST, for the same reason as stats_median.

Where the part falls between two samples, the answer lies between them in
the same measure.

### `stats_mad`

```c
real_t stats_mad(const real_t* data, uint32_t size, real_t* work);
```

Give the median absolute deviation: the median of how far each sample stands
from the median of the list.

The work list must hold as many float values as the data list. The function
writes into it and leaves the data list as it was, thus this one function
does not reorder what the caller gave it.
