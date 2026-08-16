# fir

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Filters with a finite impulse response. Declared in `sptk/filter/fir.h`.

[Back to the index](../API.md)

## Types

### `fir_t`

```c
typedef struct{
    uint32_t length;            // The number of coefficients
    float* coefficient;         // The coefficients
    float* history;             // The last samples, length of them
    uint32_t position;          // Where the next sample goes in the history
    bool dynamic_alloc;         // True if the memory comes from the heap
}fir_t;
```

## Functions

### `fir_alloc`

```c
fir_t fir_alloc(uint32_t length);
```

Give a filter with the given number of coefficients. The memory comes from
the heap, and every coefficient and every sample of the history holds zero.
Give the filter to fir_free when you no longer need it.

### `fir_static_alloc`

```c
fir_t fir_static_alloc(uint32_t length, float* coefficient, float* history);
```

Give a filter that uses the memory that the caller holds. Both lists must
hold as many float values as the given length. This function takes no
memory from the heap.

### `fir_design_low_pass`

```c
void fir_design_low_pass(fir_t* fir, float cutoff);
```

Build the coefficients of a filter that lets the low frequencies pass. The
cutoff is a part of the sample rate, and it must lie between 0 and 0.5.

### `fir_design_high_pass`

```c
void fir_design_high_pass(fir_t* fir, float cutoff);
```

Build the coefficients of a filter that lets the high frequencies pass.

### `fir_design_band_pass`

```c
void fir_design_band_pass(fir_t* fir, float low_cutoff, float high_cutoff);
```

Build the coefficients of a filter that lets a band of frequencies pass. The
low cutoff must be smaller than the high cutoff, and both must lie between 0
and 0.5.

### `fir_set_coefficient`

```c
void fir_set_coefficient(fir_t* fir, uint32_t index, float value);
```

Write one coefficient. Use this function to give the filter a set of
coefficients that another program calculated.

### `fir_get_coefficient`

```c
float fir_get_coefficient(fir_t* fir, uint32_t index);
```

Give one coefficient.

### `fir_process_sample`

```c
float fir_process_sample(fir_t* fir, float sample);
```

Give the filtered value of one sample. The filter keeps the sample in its
history, thus the next call sees it.

### `fir_process_block`

```c
void fir_process_block(fir_t* fir, const float* input, float* output, uint32_t size);
```

Filter a block of samples. The input and the output may be the same list.

### `fir_reset`

```c
void fir_reset(fir_t* fir);
```

Set every sample of the history to zero. The filter then behaves as a filter
that has seen no sample yet.

### `fir_get_gain`

```c
float fir_get_gain(fir_t* fir, float frequency);
```

Give the size of the answer of the filter at the given frequency, which is a
part of the sample rate. A value of 1 says that the frequency passes
unchanged, and a value of 0 says that the filter stops it.

### `fir_free`

```c
void fir_free(fir_t* fir);
```

Release the memory of a filter that came from fir_alloc. This function does
nothing for a filter that came from fir_static_alloc.
