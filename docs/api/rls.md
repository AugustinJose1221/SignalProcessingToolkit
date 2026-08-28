# rls

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

A filter that solves least squares at every sample. Declared in `sptk/filter/rls.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `RLS_MATRIX_SIZE`

```c
#define RLS_MATRIX_SIZE(length)     ((length) * (length))
```

How many real values the matrix of a filter of this length needs.

### `RLS_LARGEST_FORGETTING`

```c
#define RLS_LARGEST_FORGETTING      REAL_C(1.0)
```

The largest forgetting factor, which is to forget nothing at all.

### `RLS_SMALLEST_FORGETTING`

```c
#define RLS_SMALLEST_FORGETTING     REAL_C(0.9)
```

### `RLS_DEFAULT_DOUBT`

```c
#define RLS_DEFAULT_DOUBT           REAL_C(100.0)
```

## Types

### `rls_t`

```c
typedef struct{
    ringbuf_t history;          // The last samples of the reference
    real_t* coefficient;        // What the filter has learned so far
    real_t* inverse;            // The correlation matrix turned round
    real_t* gain;               // Working room, one for each coefficient
    real_t* carried;            // Working room, one for each coefficient
    uint32_t length;            // How many coefficients
    real_t forgetting;          // How much of the past to keep
    real_t doubt;               // What the matrix starts at
    bool healthy;               // False once the matrix has stopped being real
    bool dynamic_alloc;         // True if the memory comes from the heap
}rls_t;
```

## Functions

### `rls_is_valid_forgetting`

```c
bool rls_is_valid_forgetting(real_t forgetting);
```

True if this forgetting factor can be used.

### `rls_alloc`

```c
rls_t rls_alloc(uint32_t length);
```

Give a filter of the given length. The memory comes from the heap, and there
is a great deal of it: read the table in the header before choosing a
length. Give the filter to rls_free when it is no longer needed.

### `rls_static_alloc`

```c
rls_t rls_static_alloc(uint32_t length, real_t* coefficient, real_t* inverse, real_t* gain, real_t* carried, real_t* history);
```

Give a filter that uses the memory the caller holds, taking nothing from the
heap.

The coefficient, gain and carried lists must each hold length values, the
history must hold length values, and the inverse must hold
RLS_MATRIX_SIZE(length) of them.

### `rls_design`

```c
bool rls_design(rls_t* rls, real_t forgetting, real_t doubt);
```

Choose how much of the past to keep, and how strongly to believe the first
samples.

Give RLS_DEFAULT_DOUBT for the doubt unless there is a reason not to. A
forgetting factor of 1 suits anything that does not change; below 1 the past
fades and the filter can follow something that moves.

This also clears the filter, thus it is where a run begins.

Give false if the forgetting factor is outside RLS_SMALLEST_FORGETTING to
RLS_LARGEST_FORGETTING, or the doubt is not above nothing.

### `rls_process_sample`

```c
real_t rls_process_sample(rls_t* rls, real_t reference, real_t wanted);
```

Put one sample through the filter and let it learn from what it got wrong.

The reference is what the filter is given and the wanted value is what it
should have produced. The answer is what the filter DID produce, before it
learned.

AS WITH THE ADAPTIVE MODULE, THE ERROR IS OFTEN THE ANSWER: where the filter
is learning noise so that it can be taken away, what is left over is the
signal. rls_error gives it.

WHAT IS LEFT OVER IS NOT THE SIGNAL EXACTLY, and it is worth knowing by how
much. The filter is estimating a response from measurements that hold the
signal too, and the signal it cannot see acts as noise on that estimate. The
more of the past it keeps, the better the estimate and the less is left.

Measured, a filter of 8 taking away interference whose size is 0.35 beside a
signal of size 1:

    forgetting        1.000      0.999      0.990
    what is left      0.030      0.033      0.117
    which is         -21 dB     -20 dB      -9 dB of the interference

A FADING PAST COSTS CANCELLATION. Keep as much of it as the thing being
learned allows: fade only as fast as that thing really moves.

### `rls_error`

```c
real_t rls_error(rls_t* rls, real_t reference, real_t wanted);
```

The same, giving what is left over rather than what the filter produced.

### `rls_process_block`

```c
bool rls_process_block(rls_t* rls, const real_t* reference, const real_t* wanted, real_t* output, real_t* error, uint32_t count);
```

Run a whole block through, learning from every sample of it.

The output takes what the filter made of the reference and the error takes
what is left when that has been taken away. Either may be NULL.

THE ERROR IS ALMOST ALWAYS THE ANSWER, for the reason adaptive gives: the
output is the interference as the filter learned it and the error is what
remains.

Give false if the filter holds no memory, or if the matrix has stopped being
a real one part way through. A block that fails part way has still changed
the filter, thus rls_is_healthy is the thing to read afterwards.

### `rls_is_healthy`

```c
bool rls_is_healthy(const rls_t* rls);
```

True while the matrix the filter carries still describes a real spread.

ASK THIS. A filter that has fallen apart still answers, and its answers are
nonsense. Once it gives false the filter cannot recover on its own; call
rls_design again to begin afresh.

### `rls_get_coefficient`

```c
real_t rls_get_coefficient(const rls_t* rls, uint32_t index);
```

Give one coefficient of what the filter has learned.

### `rls_reset`

```c
void rls_reset(rls_t* rls);
```

Clear everything the filter has learned and set the matrix back to the doubt
it was designed with.

### `rls_free`

```c
void rls_free(rls_t* rls);
```

Release the memory of a filter that came from rls_alloc. This does nothing
for one that came from rls_static_alloc.
