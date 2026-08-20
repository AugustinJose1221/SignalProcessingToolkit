#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// A buffer that holds the last samples and forgets the rest.
//
// A program that reads a signal as it arrives needs the samples that came
// before the one in hand. A filter needs them, a detector that looks back for a
// peak needs them, and a transform needs a whole block of them. But the signal
// never ends, thus the program cannot keep it all.
//
// This buffer holds a fixed number of the newest samples. When it is full, a
// new sample takes the place of the oldest one. Nothing is copied and nothing
// is moved: only one position changes. Thus putting a sample in costs the same
// whether the buffer holds ten samples or ten thousand.
//
// WHAT IT IS FOR
//
// As a DELAY LINE. Put the sample that arrived, then take the sample from a
// number of steps ago:
//
//     ringbuf_put(&line, sample);
//     float delayed = ringbuf_get(&line, 48u);
//
// As a WINDOW for a block. A transform, a filter of Savitzky and Golay, and a
// median all want a flat list of samples in order. ringbuf_copy writes one,
// oldest first.
//
// As HISTORY to look back over. A detector that fires on one sample often has
// to find where the event really stood, which is a number of samples behind.
//
// THE AGE OF A SAMPLE
//
// ringbuf_get takes an age and not a position. An age of 0 is the newest
// sample, 1 the one before it, and so on. That way the meaning of a number
// does not change as the buffer fills, which a position would.
//
// The size is any number above zero. It does not have to be a power of two.

typedef struct{
    real_t* data;                // The samples
    uint32_t size;              // How many samples the buffer can hold
    uint32_t head;              // Where the next sample goes
    uint32_t count;             // How many samples it holds now
    bool dynamic_alloc;         // True if the memory comes from the heap
}ringbuf_t;

// Give a buffer that holds the given number of samples. The memory comes from
// the heap. Give the buffer to ringbuf_free when you no longer need it.
ringbuf_t ringbuf_alloc(uint32_t size);

// Give a buffer that uses the memory at data, which must hold as many float
// values as the given size. This function takes no memory from the heap.
ringbuf_t ringbuf_static_alloc(uint32_t size, real_t* data);

// Forget every sample. The buffer keeps its memory and its size.
void ringbuf_reset(ringbuf_t* ringbuf);

// Put one sample in. When the buffer is full this takes the place of the
// oldest sample, which is then gone.
void ringbuf_put(ringbuf_t* ringbuf, real_t sample);

// Give the sample of the given age. An age of 0 is the newest sample.
//
// Give 0 for an age that the buffer does not hold, either because that many
// samples have not arrived yet or because the age is not below the size.
real_t ringbuf_get(const ringbuf_t* ringbuf, uint32_t age);

// Give how many samples the buffer holds now. This rises to the size and then
// stays there.
uint32_t ringbuf_count(const ringbuf_t* ringbuf);

// True when the buffer holds as many samples as its size, thus when the next
// sample will push one out.
bool ringbuf_is_full(const ringbuf_t* ringbuf);

// Write the samples into a flat list, the oldest first and the newest last.
//
// The list must hold as many float values as the buffer holds samples, which
// ringbuf_count gives. Give this list to a transform, to a median, or to any
// other function that wants a block in order.
//
// Give how many samples were written.
uint32_t ringbuf_copy(const ringbuf_t* ringbuf, real_t* output);

// Release the memory of a buffer that came from ringbuf_alloc. This function
// does nothing for a buffer that came from ringbuf_static_alloc.
void ringbuf_free(ringbuf_t* ringbuf);

#endif//RINGBUF_H
