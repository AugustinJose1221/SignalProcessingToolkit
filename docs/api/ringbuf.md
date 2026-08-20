# ringbuf

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

A buffer of the last samples. Declared in `sptk/core/ringbuf.h`.

[Back to the index](../API.md) | [How the core modules work](../../sptk/core/README.md)

## Types

### `ringbuf_t`

```c
typedef struct{
    real_t* data;                // The samples
    uint32_t size;              // How many samples the buffer can hold
    uint32_t head;              // Where the next sample goes
    uint32_t count;             // How many samples it holds now
    bool dynamic_alloc;         // True if the memory comes from the heap
}ringbuf_t;
```

## Functions

### `ringbuf_alloc`

```c
ringbuf_t ringbuf_alloc(uint32_t size);
```

Give a buffer that holds the given number of samples. The memory comes from
the heap. Give the buffer to ringbuf_free when you no longer need it.

### `ringbuf_static_alloc`

```c
ringbuf_t ringbuf_static_alloc(uint32_t size, real_t* data);
```

Give a buffer that uses the memory at data, which must hold as many float
values as the given size. This function takes no memory from the heap.

### `ringbuf_reset`

```c
void ringbuf_reset(ringbuf_t* ringbuf);
```

Forget every sample. The buffer keeps its memory and its size.

### `ringbuf_put`

```c
void ringbuf_put(ringbuf_t* ringbuf, real_t sample);
```

Put one sample in. When the buffer is full this takes the place of the
oldest sample, which is then gone.

### `ringbuf_get`

```c
real_t ringbuf_get(const ringbuf_t* ringbuf, uint32_t age);
```

Give the sample of the given age. An age of 0 is the newest sample.

Give 0 for an age that the buffer does not hold, either because that many
samples have not arrived yet or because the age is not below the size.

### `ringbuf_count`

```c
uint32_t ringbuf_count(const ringbuf_t* ringbuf);
```

Give how many samples the buffer holds now. This rises to the size and then
stays there.

### `ringbuf_is_full`

```c
bool ringbuf_is_full(const ringbuf_t* ringbuf);
```

True when the buffer holds as many samples as its size, thus when the next
sample will push one out.

### `ringbuf_copy`

```c
uint32_t ringbuf_copy(const ringbuf_t* ringbuf, real_t* output);
```

Write the samples into a flat list, the oldest first and the newest last.

The list must hold as many float values as the buffer holds samples, which
ringbuf_count gives. Give this list to a transform, to a median, or to any
other function that wants a block in order.

Give how many samples were written.

### `ringbuf_free`

```c
void ringbuf_free(ringbuf_t* ringbuf);
```

Release the memory of a buffer that came from ringbuf_alloc. This function
does nothing for a buffer that came from ringbuf_static_alloc.
