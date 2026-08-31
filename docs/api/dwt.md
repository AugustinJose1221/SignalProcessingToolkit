# dwt

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The discrete wavelet transform. Declared in `ffitt/transform/dwt.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `DWT_MAX_COEFFICIENT_COUNT`

```c
#define DWT_MAX_COEFFICIENT_COUNT   4u
```

The largest number of coefficients that a wavelet of this module holds.

## Types

### `dwt_t`

```c
typedef struct{
    dwt_wavelet_t wavelet;      // Which wavelet the transform uses
    uint32_t length;            // The number of coefficients of that wavelet
    real_t low[DWT_MAX_COEFFICIENT_COUNT];   // The filter of the approximation
    real_t high[DWT_MAX_COEFFICIENT_COUNT];  // The filter of the detail
}dwt_t;
```

## Functions

### `dwt_init`

```c
dwt_t dwt_init(dwt_wavelet_t wavelet);
```

Give a transform that uses the given wavelet. This function takes no memory.
The whole state lies inside the structure.

### `dwt_is_valid_size`

```c
bool dwt_is_valid_size(uint32_t size, uint32_t levels);
```

True if a signal of the given size can go through the given number of
levels. Each level halves the size, thus the size must divide by two that
many times, and the size of the last level must still hold at least two
samples.

### `dwt_forward`

```c
void dwt_forward(dwt_t* dwt, const real_t* signal, uint32_t size, real_t* approximation, real_t* detail);
```

Take one level of the transform.

The signal holds size values. The function writes size/2 values into the
approximation and size/2 values into the detail. The size must be even, and
the three lists must not be the same memory.

### `dwt_inverse`

```c
void dwt_inverse(dwt_t* dwt, const real_t* approximation, const real_t* detail, uint32_t size, real_t* signal);
```

Take one level of the inverse transform.

The approximation and the detail hold size/2 values each, and the function
writes size values into the signal. The size must be even.

### `dwt_forward_multi`

```c
void dwt_forward_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels, real_t* work);
```

Take several levels of the transform, one after the other.

The function writes the result over the signal. After the call the first
size/(2^levels) values hold the approximation of the last level. The values
after it hold the detail of the last level, then the detail of the level
before it, and so on up to the detail of the first level, which fills the
second half of the list.

The work buffer must hold as many values as the signal. The function gets no
memory.

### `dwt_inverse_multi`

```c
void dwt_inverse_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels, real_t* work);
```

Take several levels of the inverse transform. The list holds the result of
dwt_forward_multi, and the function writes the signal over it. The work
buffer must hold as many values as the signal.

### `dwt_threshold`

```c
void dwt_threshold(real_t* data, uint32_t size, real_t limit);
```

Set every value of the list whose size is below the limit to zero.

Use this function on the detail values of a transform to take noise out of a
signal. Give it the part of the list that holds the details, and not the
approximation.
