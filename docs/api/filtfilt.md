# filtfilt

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Filtering with no delay. Declared in `sptk/filter/filtfilt.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Functions

### `filtfilt_padding`

```c
uint32_t filtfilt_padding(uint32_t filter_size, uint32_t size);
```

How many samples are carried past each end, for a filter of the given size.

Three times the length of the filter is enough for the answer to have
settled. Where the signal is shorter than that, as much as there is is used.

### `filtfilt_iir_gain`

```c
real_t filtfilt_iir_gain(iir_t* iir, real_t frequency);
```

Give what a filter really does at a frequency when it is run both ways,
which is the square of what it does in one pass.

Use this rather than iir_get_gain when designing for filtfilt. At the cutoff
a filter passes 0.707; run both ways it passes 0.5.

### `filtfilt_fir_gain`

```c
real_t filtfilt_fir_gain(fir_t* fir, real_t frequency);
```

The same for a filter with a finite impulse response.

### `filtfilt_iir`

```c
bool filtfilt_iir(iir_t* iir, const real_t* input, real_t* output, uint32_t size);
```

Filter a whole signal both ways with a filter that has feedback.

The output holds as many samples as the input, and output[k] lines up with
input[k] with no delay between them. The input and the output may be the
same list.

The state of the filter is used and changed. Its coefficients are not.

Give false if the signal is too short to filter, which is when it holds
fewer samples than the filter has state.

### `filtfilt_fir`

```c
bool filtfilt_fir(fir_t* fir, const real_t* input, real_t* output, uint32_t size);
```

Filter a whole signal both ways with a filter that has no feedback.

A filter with a middle coefficient already delays every frequency by the
same time, thus running it both ways is not needed to keep the shape. It is
still useful to make the edges of the band twice as steep without a longer
filter, and to leave no delay at all to correct for.

Give false if the signal holds fewer samples than the filter is long.
