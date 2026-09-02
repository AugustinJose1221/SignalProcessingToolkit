// WHAT THE HEADER OF ringbuf CLAIMS.
//
//   "Nothing is copied and nothing is moved: only one position changes. Thus
//    putting a sample in costs the same whether the buffer holds ten samples
//    or ten thousand."
//
// The wrong way to write this buffer moves every sample down by one at each
// new sample. That way costs the size of the buffer at each sample, thus a
// buffer of ten thousand would cost a thousand times a buffer of ten. This
// measures the two and asks that the larger one is not far dearer.
//
// HELD LOOSELY AT FOUR TIMES, NOT AT ONE. The larger buffer walks over more
// memory, thus it misses the cache more often even though it does the same
// work. Four times leaves room for that and still stands a thousand times
// away from what a buffer that moved its samples would measure.

#include <perf/cost/cost.h>

#include <ffitt/core/ringbuf.h>

#include <stdlib.h>

#define RINGBUF_SMALL_SIZE      16u
#define RINGBUF_LARGE_SIZE      16384u
#define RINGBUF_SAMPLES         1000000u
#define RINGBUF_REPEATS         7u

// The samples are worked out before the measurement, so that what is measured
// is the buffer and not the making of a random number.
static real_t ringbuf_samples[64];

static double ringbuf_time_of_one_put(uint32_t size)
{
    double seconds;
    ringbuf_t buffer = ringbuf_alloc(size);

    COST_MEASURE(seconds, RINGBUF_REPEATS,
    {
        for(uint32_t index = 0u; index < RINGBUF_SAMPLES; index++)
        {
            ringbuf_put(&buffer, ringbuf_samples[index & 63u]);
        }
    });

    cost_sink += ringbuf_get(&buffer, 0u);
    ringbuf_free(&buffer);

    return seconds / (double)RINGBUF_SAMPLES;
}

void run_ringbuf_cost_tests(void)
{
    double small_buffer;
    double large_buffer;

    for(uint32_t index = 0u; index < 64u; index++)
    {
        ringbuf_samples[index] = cost_random_value();
    }

    small_buffer = ringbuf_time_of_one_put(RINGBUF_SMALL_SIZE);
    large_buffer = ringbuf_time_of_one_put(RINGBUF_LARGE_SIZE);

    cost_claim_at_most("ringbuf",
                       "put costs the same at 16 and at 16384",
                       large_buffer / small_buffer, 4.0);
}
