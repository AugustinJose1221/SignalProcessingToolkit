# farrow

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Delaying by a part of a sample. Declared in `sptk/filter/farrow.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `FARROW_LARGEST_ORDER`

```c
#define FARROW_LARGEST_ORDER    8u
```

### `FARROW_TAP_COUNT`

```c
#define FARROW_TAP_COUNT(order)     ((order) + 1u)
```

How many samples a filter of this order works from, and how many values each
of its two lists must hold.

### `FARROW_WEIGHT_COUNT`

```c
#define FARROW_WEIGHT_COUNT(order)  (FARROW_TAP_COUNT(order) \
```

## Types

### `farrow_t`

```c
typedef struct{
    ringbuf_t history;          // The samples the answer is worked out from
    real_t* weight;             // The Farrow matrix, (order+1) by (order+1)
    real_t* working;            // One running total for each power of the part
    uint32_t order;             // The order of the curve laid through the samples
    real_t delay;               // The delay asked for, in samples
    bool dynamic_alloc;         // True if the memory comes from the heap
}farrow_t;
```

## Functions

### `farrow_is_valid_order`

```c
bool farrow_is_valid_order(uint32_t order);
```

True if this is an order the filter can be built at.

An order of nothing would take one sample and give it back, which is no delay
at all. The bound above is where the curve laid through the samples begins to
swing between them more than it follows them.

### `farrow_smallest_delay`

```c
real_t farrow_smallest_delay(uint32_t order);
```

The smallest delay a filter of this order can apply, in samples, which is
half its order. The reason is in the header above.

### `farrow_largest_delay`

```c
real_t farrow_largest_delay(uint32_t order);
```

The largest, which stands one sample past the smallest. For more than that,
take the whole samples with a ringbuf and leave the part to this.

### `farrow_is_valid_delay`

```c
bool farrow_is_valid_delay(const farrow_t* farrow, real_t delay);
```

True if this filter can apply this delay.

### `farrow_alloc`

```c
farrow_t farrow_alloc(uint32_t order);
```

Give a filter of the given order. The memory comes from the heap. Give it to
farrow_free when you no longer need it.

It starts at the smallest delay it can apply, which is half its order.

### `farrow_static_alloc`

```c
farrow_t farrow_static_alloc(uint32_t order, real_t* history, real_t* weight, real_t* working);
```

Give a filter that uses the memory the caller holds. The history must hold
FARROW_TAP_COUNT(order) values, the weights FARROW_WEIGHT_COUNT(order), and
the working room FARROW_TAP_COUNT(order). This takes nothing from the heap.

### `farrow_set_delay`

```c
bool farrow_set_delay(farrow_t* farrow, real_t delay);
```

Choose the delay, in samples.

THE DELAY MAY BE CHANGED AT ANY SAMPLE AND THE ANSWER DOES NOT JUMP, which is
the whole reason this is built the way it is. The weights are polynomials in
the part of a sample, worked out once when the filter is built; changing the
delay changes only the number those polynomials are read at. A filter that
worked its weights out afresh would cost far more and would still be this.

Give false and leave the filter as it was if the delay is not one
farrow_is_valid_delay accepts.

### `farrow_get_delay`

```c
real_t farrow_get_delay(const farrow_t* farrow);
```

Give the delay the filter is applying, in samples.

### `farrow_process_sample`

```c
real_t farrow_process_sample(farrow_t* farrow, real_t sample);
```

Give one sample and take the delayed answer.

The first FARROW_TAP_COUNT(order) samples come out of a filter that has not
yet seen enough of the signal to work from. They are not wrong so much as
unfinished, and a measurement should start after them.

### `farrow_process_block`

```c
bool farrow_process_block(farrow_t* farrow, const real_t* input, real_t* output, uint32_t count);
```

Run a block through. The input and the output may be the same list.

### `farrow_reset`

```c
void farrow_reset(farrow_t* farrow);
```

Forget every sample seen so far. The order and the delay are kept.

### `farrow_free`

```c
void farrow_free(farrow_t* farrow);
```

Give back the memory that farrow_alloc took.
