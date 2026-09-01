#ifndef TEST
#include <ffitt/core/ringbuf.h>
#include <ffitt/core/defs.h>
#else
#include "ringbuf.h"
#include "defs.h"
#endif

ringbuf_t ringbuf_alloc(uint32_t size)
{
    ASSERT(size > 0);

    ringbuf_t ringbuf;

    ringbuf.data = (real_t*)malloc(sizeof(real_t)*size);
    ringbuf.size = size;
    ringbuf.dynamic_alloc = true;

    ringbuf_reset(&ringbuf);

    return ringbuf;
}

ringbuf_t ringbuf_static_alloc(uint32_t size, real_t* data)
{
    ASSERT(size > 0);
    ASSERT(data != NULL);

    ringbuf_t ringbuf;

    ringbuf.data = data;
    ringbuf.size = size;
    ringbuf.dynamic_alloc = false;

    ringbuf_reset(&ringbuf);

    return ringbuf;
}

void ringbuf_reset(ringbuf_t* ringbuf)
{
    ASSERT(ringbuf != NULL);

    ringbuf->head = 0;
    ringbuf->count = 0;

    for(uint32_t index = 0; index < ringbuf->size; index++)
    {
        ringbuf->data[index] = REAL_C(0.0);
    }
}

void ringbuf_put(ringbuf_t* ringbuf, real_t sample)
{
    ASSERT(ringbuf != NULL);

    ringbuf->data[ringbuf->head] = sample;

    // Step the head on, and turn it back to the start when it reaches the end.
    //
    // A test against the size is used here and not the remainder operator. A
    // remainder needs a division, which costs far more than a comparison on a
    // small processor, and the size does not have to be a power of two for
    // this to work.
    ringbuf->head++;
    if(ringbuf->head >= ringbuf->size)
    {
        ringbuf->head = 0;
    }

    if(ringbuf->count < ringbuf->size)
    {
        ringbuf->count++;
    }
}

real_t ringbuf_get(const ringbuf_t* ringbuf, uint32_t age)
{
    ASSERT(ringbuf != NULL);

    if(age >= ringbuf->count)
    {
        return REAL_C(0.0);
    }

    // The head stands one place past the newest sample, thus the newest sample
    // is one step back from it. The size is added before the subtraction so
    // that the number never goes below zero, which an unsigned number cannot
    // hold.
    uint32_t place = (ringbuf->head + ringbuf->size - 1u - age) % ringbuf->size;

    return ringbuf->data[place];
}

uint32_t ringbuf_count(const ringbuf_t* ringbuf)
{
    ASSERT(ringbuf != NULL);

    return ringbuf->count;
}

bool ringbuf_is_full(const ringbuf_t* ringbuf)
{
    ASSERT(ringbuf != NULL);

    return ringbuf->count >= ringbuf->size;
}

uint32_t ringbuf_copy(const ringbuf_t* ringbuf, real_t* output)
{
    ASSERT(ringbuf != NULL);
    ASSERT(output != NULL);

    // The oldest sample stands at the age of count-1, thus counting the ages
    // down writes the samples in the order that time gave them.
    for(uint32_t index = 0; index < ringbuf->count; index++)
    {
        output[index] = ringbuf_get(ringbuf, ringbuf->count - 1u - index);
    }

    return ringbuf->count;
}

void ringbuf_free(ringbuf_t* ringbuf)
{
    ASSERT(ringbuf != NULL);

    if(ringbuf->dynamic_alloc)
    {
        free(ringbuf->data);
        ringbuf->data = NULL;
        ringbuf->size = 0;
        ringbuf->count = 0;
        ringbuf->dynamic_alloc = false;
    }
}
