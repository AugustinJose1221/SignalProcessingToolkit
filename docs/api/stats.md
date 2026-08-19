# stats

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Measures of a list of samples. Declared in `sptk/util/stats.h`.

[Back to the index](../API.md) | [How the util modules work](../../sptk/util/README.md)

## Macros

### `STATS_MAD_TO_DEVIATION`

```c
#define STATS_MAD_TO_DEVIATION      1.4826f
```

What the median absolute deviation must be multiplied by to estimate the
standard deviation of samples that follow a normal spread.

The number is 1/0.6745, because for a normal spread the median absolute
deviation is 0.6745 of the deviation.

## Functions

### `stats_sum`

```c
float stats_sum(const float* data, uint32_t size);
```

Give the sum of the samples.

### `stats_mean`

```c
float stats_mean(const float* data, uint32_t size);
```

Give the mean of the samples.

### `stats_variance`

```c
float stats_variance(const float* data, uint32_t size);
```

Give the variance of the samples, divided by the number of samples.

This is the variance of the list as it stands. To estimate the variance of
the thing the list was drawn FROM, multiply by size/(size-1).

### `stats_deviation`

```c
float stats_deviation(const float* data, uint32_t size);
```

Give the standard deviation, which is the root of the variance.

### `stats_rms`

```c
float stats_rms(const float* data, uint32_t size);
```

Give the root of the mean of the squares.

This is not the deviation. The root mean square holds the mean inside it,
thus for a signal that sits at 100 and wanders by 1 it gives about 100. The
deviation gives 1. Take this one for the power of a signal and the other one
for how much the signal moves.

### `stats_min`

```c
float stats_min(const float* data, uint32_t size);
```

Give the smallest sample.

### `stats_max`

```c
float stats_max(const float* data, uint32_t size);
```

Give the largest sample.

### `stats_median`

```c
float stats_median(float* data, uint32_t size);
```

Give the median of the samples.

THIS FUNCTION REORDERS THE LIST. It has to put the samples in order to find
the middle one, and it does that in the memory of the caller so that it
needs none of its own. Copy the list first if the order matters.

For a list of an even size the median lies between the two middle samples,
and the function gives their mean.

### `stats_percentile`

```c
float stats_percentile(float* data, uint32_t size, float part);
```

Give the sample below which the given part of the list stands. A part of 0.5
gives the median, 0.25 the first quarter, 0.9 the ninth tenth.

THIS FUNCTION REORDERS THE LIST, for the same reason as stats_median.

Where the part falls between two samples, the answer lies between them in
the same measure.

### `stats_mad`

```c
float stats_mad(const float* data, uint32_t size, float* work);
```

Give the median absolute deviation: the median of how far each sample stands
from the median of the list.

The work list must hold as many float values as the data list. The function
writes into it and leaves the data list as it was, thus this one function
does not reorder what the caller gave it.
