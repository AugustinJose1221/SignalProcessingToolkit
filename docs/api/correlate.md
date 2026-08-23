# correlate

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

How alike two signals are. Declared in `sptk/transform/correlate.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Functions

### `correlate_is_valid_scaling`

```c
bool correlate_is_valid_scaling(correlate_scaling_t scaling);
```

True if the module knows this scaling.

### `correlate_auto`

```c
bool correlate_auto(const real_t* data, uint32_t size, real_t* output, uint32_t max_lag, correlate_scaling_t scaling);
```

Correlate a signal with itself, over the lags 0 to max_lag.

The output holds max_lag+1 values, and output[k] is the answer at the lag k.
At a lag of 0 a signal always matches itself, thus output[0] is the largest
value that any lag can reach, and with CORRELATE_COEFFICIENT it is 1.

The max_lag must be below the size. A lag as large as the size leaves no
samples that overlap, thus there is nothing to correlate.

Give false if the sizes do not fit together or the scaling is unknown.

### `correlate_cross`

```c
bool correlate_cross(const real_t* a, const real_t* b, uint32_t size, real_t* output, uint32_t max_lag, correlate_scaling_t scaling);
```

Correlate one signal with another, over the lags 0 to max_lag.

The lag moves the SECOND signal later in time. Thus output[k] is large when
b holds at k samples later what a holds now, which is to say when b lags a
by k samples.

Both signals must hold the same number of samples.

Give false if the sizes do not fit together or the scaling is unknown.

### `correlate_best_lag`

```c
uint32_t correlate_best_lag(const real_t* data, uint32_t size, real_t* output, uint32_t low_lag, uint32_t high_lag, real_t* strength);
```

Give the lag between low_lag and high_lag where a signal is most like
itself, and write how strong that likeness is into strength.

This is the whole of finding a period in one call. A signal that repeats
every 100 samples gives back 100. The strength is a coefficient, thus it can
be judged: a signal that truly repeats gives something near 1, and a signal
that does not gives something near 0. Measured on a recording of a heart
inside a scanner, the artefact of the scanner gave 0.998 and the same
recording with no scanner gave 0.011.

The lag of 0 must be left out of the range, because every signal matches
itself perfectly there and that answer says nothing.

The output list must hold high_lag+1 values, and the function uses it to
work in. Give NULL for strength if the strength is not wanted.

Give 0 and a strength of 0 if the range does not fit inside the signal.

### `correlate_transform_size`

```c
uint32_t correlate_transform_size(uint32_t size);
```

Give the size of the transform that the fast method needs for a signal of
the given size, or 0 if no transform of a size it can use is large enough.

The transform must be at least twice the size of the signal, so that the two
ends of the signal cannot wrap round and correlate with each other.

### `correlate_auto_by_transform`

```c
bool correlate_auto_by_transform(const real_t* data, uint32_t size, real_t* output, uint32_t max_lag, correlate_scaling_t scaling, fft_t* fft, cnum_t* work, real_t* window);
```

Correlate a signal with itself using the transform.

This gives the same answer as correlate_auto, to the last digit that the
width can hold, and costs far less for a long signal.

IT SERVES THE THREE SCALINGS THAT ARE SUMS AND NOT THE COEFFICIENT. A
transform gives the sum at each lag and nothing else, and a coefficient
needs the mean and the energy of the samples that overlap at each lag on
their own. Give CORRELATE_COEFFICIENT to correlate_auto instead, which works
them out lag by lag.

The caller gives everything it needs, thus this module takes no memory of
its own and works on a target with no heap:

  fft      made for correlate_transform_size(size), by either fft_alloc or
           fft_static_alloc
  work     correlate_transform_size(size) complex values
  window   correlate_transform_size(size) real values

Give false if the sizes do not fit together, if the scaling is unknown or is
CORRELATE_COEFFICIENT, if no transform large enough can be made for this
size, or if the transform that was given is not of that size.
