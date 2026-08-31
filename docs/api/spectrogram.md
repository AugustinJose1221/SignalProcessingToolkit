# spectrogram

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

What the short pieces mean. Declared in `ffitt/transform/spectrogram.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `SPECTROGRAM_FLOOR_DECIBEL`

```c
#define SPECTROGRAM_FLOOR_DECIBEL   (-REAL_C(200.0))
```

The lowest value the decibel unit gives.

A bin holding nothing has no logarithm. 200 decibels below a reference of 1
is far below anything a measurement can reach at either width, thus the
floor cannot hide a real reading, and it keeps the answer to numbers that
arithmetic and pictures can use.

## Functions

### `spectrogram_is_valid_kind`

```c
bool spectrogram_is_valid_kind(spectrogram_kind_t kind);
```

True if the unit is one this module knows.

### `spectrogram_value_count`

```c
uint32_t spectrogram_value_count(const stft_t* stft, uint32_t frame_count);
```

How many values a spectrogram of this many frames holds.

### `spectrogram_build`

```c
bool spectrogram_build(const stft_t* stft, const cnum_t* frames, uint32_t frame_count, spectrogram_kind_t kind, real_t sample_rate, real_t* output, uint32_t room);
```

Turn the frames of a short-time transform into one real number for each bin.

The frames are what stft_forward gave, and the output holds
spectrogram_value_count values laid out the same way: the bin b of the frame
f sits at (f * STFT_BIN_COUNT(block)) + b.

The sample rate is used by SPECTROGRAM_DENSITY only, and it is ignored by
the other three.

Give false if the transform has not been designed, if the unit is unknown,
if there are no frames, or if the room is too small.

### `spectrogram_largest`

```c
real_t spectrogram_largest(const real_t* values, uint32_t count);
```

Give the largest value in a spectrogram, which is what a picture is usually
drawn against.

### `spectrogram_against_the_largest`

```c
bool spectrogram_against_the_largest(const real_t* values, uint32_t count, real_t* output);
```

Turn a spectrogram of decibels into one measured from its own largest value,
so that the largest reads 0 and everything else is below it.

This is how a spectrogram is nearly always drawn, because the reading that
matters is which parts are loud AGAINST THE REST and not against a reference
that the recording never knew about. The output may be the input.

Give false if the values are not decibels, which the caller must know, or if
there are none.
