# dct

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Turning a signal into cosines. Declared in `ffitt/transform/dct.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `DCT_LARGEST_SIZE`

```c
#define DCT_LARGEST_SIZE    1024u
```

## Functions

### `dct_is_valid_size`

```c
bool dct_is_valid_size(uint32_t size);
```

True if this is a size the transform can be taken at. It must be at least one
and no more than the bound above, which is where the cost of working in the
square of the size stops being worth paying.

### `dct_forward`

```c
bool dct_forward(const real_t* input, real_t* output, uint32_t size);
```

Turn a signal into cosines.

The output holds as many numbers as the input. The first of them is the level
of the signal, and the rest say how much of each cosine it holds, from the
slowest upwards.

The input and the output must be different lists.

Give false if the size is not one dct_is_valid_size accepts.

### `dct_inverse`

```c
bool dct_inverse(const real_t* input, real_t* output, uint32_t size);
```

Turn cosines back into a signal.

This undoes dct_forward exactly, up to the rounding of the width. The input
and the output must be different lists.

Give false if the size is not one dct_is_valid_size accepts.

### `dct_count_for_share`

```c
uint32_t dct_count_for_share(const real_t* cosines, uint32_t size, real_t share);
```

How many of the first numbers are needed to hold the given share of a signal.

THIS IS THE NUMBER COMPRESSION IS CHOSEN BY. Give the cosines of a signal and
the share to keep, and this says how many of them carry that share. A slow
signal of 64 samples holds 0.99999 of itself in its first eight; a signal of
noise needs nearly all 64.

Give 0 if the size is not one dct_is_valid_size accepts, if the share is not
above nothing and at most one, or if the signal holds nothing at all.
