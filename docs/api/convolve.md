# convolve

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Sliding one signal along another. Declared in `sptk/transform/convolve.h`.

[Back to the index](../API.md) | [How the transform modules work](../../sptk/transform/README.md)

## Functions

### `convolve_is_valid_mode`

```c
bool convolve_is_valid_mode(convolve_mode_t mode);
```

True if the module knows this mode.

### `convolve_output_size`

```c
uint32_t convolve_output_size(uint32_t signal_size, uint32_t shape_size, convolve_mode_t mode);
```

How many values the answer holds, or 0 if there is no answer to give.

CONVOLVE_VALID gives 0 when the shape is longer than the signal, because
there is then no place where the shape lies wholly inside.

### `convolve_direct`

```c
bool convolve_direct(const real_t* signal, uint32_t signal_size, const real_t* shape, uint32_t shape_size, real_t* output, convolve_mode_t mode);
```

Slide the shape along the signal, the plain way.

The output must hold convolve_output_size values. The input and the output
must not be the same list.

Give false if either size is nothing, if the mode is unknown, or if there is
no answer to give.

### `convolve_transform_size`

```c
uint32_t convolve_transform_size(uint32_t signal_size, uint32_t shape_size);
```

Give the size of the transform that the fast way needs, or 0 if no transform
of a size it can use is large enough.

The transform must be at least as long as the whole answer, so that the two
ends cannot wrap round and add to each other.

### `convolve_by_transform`

```c
bool convolve_by_transform(const real_t* signal, uint32_t signal_size, const real_t* shape, uint32_t shape_size, real_t* output, convolve_mode_t mode, fft_t* fft, cnum_t* first, cnum_t* second, real_t* work);
```

Slide the shape along the signal, using the transform.

This gives the same answer as convolve_direct, to the last digit the width
can hold, and costs far less for a long shape.

The caller gives everything it needs, thus this module takes no memory of
its own and works on a target with no heap:

  fft      made for convolve_transform_size, by fft_alloc or fft_static_alloc
  first    that many complex values
  second   that many complex values
  work     that many real values

Give false for the same reasons as convolve_direct, or if the transform that
was given is not of the right size.
