# window

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Windows for a transform. Declared in `sptk/transform/window.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Functions

### `window_is_valid_kind`

```c
bool window_is_valid_kind(window_kind_t kind);
```

True if the module knows this kind of window.

### `window_takes_a_parameter`

```c
bool window_takes_a_parameter(window_kind_t kind);
```

True if this kind of window takes a parameter. A window that takes one needs
window_build_with; a window that does not takes window_build.

### `window_build`

```c
void window_build(real_t* window, uint32_t size, window_kind_t kind);
```

Write the values of a window into the list, which must hold as many float
values as the given size.

The window is symmetric: the first value and the last value are the same.
That is what a transform wants. A window for building a filter wants the
same thing, thus this module serves both.

A size of 1 gives the single value 1.

### `window_build_with`

```c
void window_build_with(real_t* window, uint32_t size, window_kind_t kind, real_t parameter);
```

Write the values of a window that takes a parameter.

For WINDOW_TUKEY the parameter is the part of the window that falls, from 0
to 1. At 0 the window is rectangular and nothing falls; at 1 it is a Hann
window and everything falls. At 0.5 the middle half stays as it is.

For WINDOW_KAISER the parameter is beta, which is 0 or more. A larger beta
gives lower side lobes and a wider main lobe. Measured, for a window of 64:

    beta          0     2     4    5.65    6     8    8.6    10    12
    side lobe   -13   -19   -31    -42   -44   -58   -63    -74   -90  dB

Thus 0 gives a rectangular window and about 6 gives a window near Blackman.
Use window_kaiser_beta to get beta from the stop band that a filter needs.

A kind that takes no parameter ignores it, thus this function can always
stand in for window_build.

### `window_value`

```c
real_t window_value(uint32_t index, uint32_t size, window_kind_t kind, real_t parameter);
```

Give one value of a window, without building the whole of it. This suits a
caller that has no room to hold the window, and one that builds a window
into another list as it goes.

### `window_kaiser_beta`

```c
real_t window_kaiser_beta(real_t stop_band_decibel);
```

Give the beta of a Kaiser window for a FILTER whose stop band must lie the
given number of decibels down. Give a positive number: 60 means 60 dB down.
This is the rule of Kaiser.

READ WHAT THIS NUMBER IS, because it is easy to take it for the other one.
It is the stop band of a filter that is BUILT with the window. It is NOT the
level of the side lobes of the window itself, and the two are far apart.
Measured:

    asked for       26    45    60    81    87    99   dB of stop band
    beta            2.0   4.0   5.7   8.0   8.6  10.0
    window lobes   -19   -31   -42   -58   -63   -74   dB

The side lobes of the window always lie about 18 to 27 dB higher than the
stop band that the same beta gives a filter. A reader who wants a window
whose own side lobes lie 60 dB down needs a beta near 8.2, not near 5.7.
Take beta from the table above the declaration of window_build_with for
that, and take this function only for designing a filter.

### `window_coherent_gain`

```c
real_t window_coherent_gain(const real_t* window, uint32_t size);
```

Give the coherent gain of a window, which is the mean of its values.

A tone that stands exactly on a bin comes out of the transform at this part
of its true height. Divide the height of a peak by this number to read the
height of the tone. A rectangular window gives 1, a Hann window gives 0.5.

### `window_noise_gain`

```c
real_t window_noise_gain(const real_t* window, uint32_t size);
```

Give the noise gain of a window, which is the root of the mean of the
squares of its values.

Noise, unlike a tone, does not stand on one bin. It comes out at this part
of its true size. Divide by this number to read the size of the noise. A
rectangular window gives 1, a Hann window gives about 0.61.

### `window_noise_bandwidth`

```c
real_t window_noise_bandwidth(const real_t* window, uint32_t size);
```

Give the equivalent noise bandwidth of a window, in bins.

This is how many bins of noise a single bin holds after the window. A
rectangular window gives 1.0, a Hann window gives 1.5. A measurement of the
density of noise divides by this, and by the width of a bin.

### `window_apply`

```c
void window_apply(const real_t* window, const real_t* input, real_t* output, uint32_t size);
```

Multiply a block of samples by a window. The input and the output may be the
same list.
