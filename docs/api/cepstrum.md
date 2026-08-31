# cepstrum

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Finding what repeats in a spectrum. Declared in `ffitt/transform/cepstrum.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `CEPSTRUM_FLOOR`

```c
#define CEPSTRUM_FLOOR      REAL_C(1.0e-6)
```

## Types

### `cepstrum_t`

```c
typedef struct{
    fft_t fft;                  // The transform, taken twice
    cnum_t* work;               // Room for one spectrum
    real_t* window;             // The window laid on the block
    real_t* windowed;           // The block after the window
    uint32_t size;              // The block, a power of two
    bool dynamic_alloc;         // True if the memory comes from the heap
}cepstrum_t;
```

## Functions

### `cepstrum_is_valid_size`

```c
bool cepstrum_is_valid_size(uint32_t size);
```

True if this is a block the cepstrum can be taken of, which is whatever the
transform can take.

### `cepstrum_alloc`

```c
cepstrum_t cepstrum_alloc(uint32_t size);
```

Give a cepstrum of the given block size. The memory comes from the heap. Give
it to cepstrum_free when you no longer need it.

### `cepstrum_static_alloc`

```c
cepstrum_t cepstrum_static_alloc(uint32_t size, cnum_t* work, real_t* window, real_t* windowed, fft_t fft);
```

Give one that uses the memory the caller holds. The work must hold as many
complex numbers as the size, and the window and the windowed block as many
real values. This takes nothing from the heap.

### `cepstrum_real`

```c
bool cepstrum_real(cepstrum_t* cepstrum, const real_t* input, real_t* output);
```

Work out the real cepstrum of a block.

The output holds as many values as the block. The value at place k says how
much the spectrum ripples with a beat that fits k times into the block, thus k
is a number of samples and is called a quefrency.

The first few places hold the SHAPE of the spectrum rather than anything
repeating in it -- how the loudness falls away with frequency, which for a
voice is the shape of the mouth. Reading a period from them finds the shape
and not the note, thus cepstrum_best_quefrency starts past them.

Give false if the size is not one cepstrum_is_valid_size accepts.

### `cepstrum_best_quefrency`

```c
uint32_t cepstrum_best_quefrency(const real_t* cepstrum, uint32_t size, uint32_t low, uint32_t high, real_t* strength);
```

Give the quefrency between low and high where the cepstrum stands highest,
which is the period of whatever repeats in the spectrum.

THE LOW BOUND MUST BE SET AND IT MATTERS. Below about a twentieth of the block
the cepstrum holds the shape of the spectrum, which is always large and always
there. A search that started at nothing would find that shape every time and
call it a note.

The strength says how far the peak stands above the ordinary run of the range.
IT MUST BE READ, and the header above says what it can and cannot tell apart.
Give NULL if it is not wanted.

Give 0 and a strength of 0 if the range does not fit inside the block.

### `cepstrum_free`

```c
void cepstrum_free(cepstrum_t* cepstrum);
```

Give back the memory that cepstrum_alloc took.
