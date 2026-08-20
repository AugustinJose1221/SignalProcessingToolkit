# savgol

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The filter of Savitzky and Golay. Declared in `sptk/filter/savgol.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Types

### `savgol_t`

```c
typedef struct{
    uint32_t window;            // The number of samples of the window, odd
    uint32_t order;             // The order of the polynomial
    uint32_t derivative;        // Which derivative the filter gives
    real_t* coefficient;         // One coefficient for each sample of the window
    bool dynamic_alloc;         // True if the memory comes from the heap
}savgol_t;
```

## Functions

### `savgol_alloc`

```c
savgol_t savgol_alloc(uint32_t window);
```

Give a filter for the given window. The memory comes from the heap. Give the
filter to savgol_free when you no longer need it.

### `savgol_static_alloc`

```c
savgol_t savgol_static_alloc(uint32_t window, real_t* coefficient);
```

Give a filter that uses the memory at coefficient, which must hold as many
float values as the window. This function takes no memory from the heap.

### `savgol_is_valid`

```c
bool savgol_is_valid(uint32_t window, uint32_t order, uint32_t derivative);
```

True if the window and the order fit together: the window must be odd and
larger than the order, and the derivative must not be above the order.

### `savgol_design`

```c
bool savgol_design(savgol_t* savgol, uint32_t order, uint32_t derivative);
```

Build the coefficients of the filter.

A derivative of 0 gives the smoothed signal. A derivative of 1 gives the
first derivative, 2 the second one, and so on. The order must be below the
size of the window, and the derivative must not be above the order.

This function gets memory from the heap for the matrices of the least
squares. It runs one time, before the filter reads any sample. The filter
itself gets no memory.

Give false if the window and the order do not fit together, or if the
matrix of the least squares has no inverse.

### `savgol_get_coefficient`

```c
real_t savgol_get_coefficient(savgol_t* savgol, uint32_t index);
```

Give one coefficient of the filter.

### `savgol_apply`

```c
real_t savgol_apply(savgol_t* savgol, const real_t* window);
```

Give the filtered value at the middle of the given window of samples. The
list must hold as many samples as the window of the filter.

### `savgol_process_block`

```c
void savgol_process_block(savgol_t* savgol, const real_t* input, real_t* output, uint32_t size);
```

Filter a whole signal.

The function writes as many values as the signal holds. Near the two ends
there are not enough samples for a whole window, thus the function repeats
the first and the last sample to fill it. The input and the output must not
be the same list.

The result of a derivative is for one step of the sample. Divide it by the
time between two samples to get a derivative for the time.

### `savgol_free`

```c
void savgol_free(savgol_t* savgol);
```

Release the memory of a filter that came from savgol_alloc. This function
does nothing for a filter that came from savgol_static_alloc.
