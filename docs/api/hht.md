# hht

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The Hilbert-Huang transform. Declared in `sptk/transform/hht.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Functions

### `hht_transform_imf`

```c
void hht_transform_imf(fft_t* fft, imf_t* imf, cnum_t* work, real_t* amplitude, real_t* frequency, real_t sample_rate);
```

Give the amplitude and the frequency at each point of time, for one
intrinsic mode function.

The function writes size values into the amplitude list, and size-1 values
into the frequency list, because a frequency needs two points of the phase.
The work buffer must hold size complex numbers. The function gets no memory.

The size must be the same as the size of the transform, and it must be a
power of two.

### `hht_transform`

```c
void hht_transform(fft_t* fft, imf_t* imf, uint32_t count, cnum_t* work, real_t* amplitude, real_t* frequency, real_t sample_rate);
```

Give the amplitude and the frequency for a list of intrinsic mode
functions, one after the other.

The lists amplitude and frequency hold the result of each function one after
the other. Thus the amplitude list must hold count*size values, and the
frequency list must hold count*(size-1) values. The work buffer must hold
size complex numbers.

### `hht_mean_frequency`

```c
real_t hht_mean_frequency(const real_t* amplitude, const real_t* frequency, uint32_t size);
```

Give the mean frequency of one intrinsic mode function, where each point
counts as much as the square of its amplitude.

A point with a small amplitude holds a phase that noise moves easily. This
mean gives such a point little weight, thus it describes the function better
than a plain mean does.
