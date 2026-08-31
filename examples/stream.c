// Keep a window that crosses the blocks a converter delivers.
//
// A converter interrupts every 10 ms with 320 new samples. Nothing about the
// work lines up with that: a mean over the last 500 samples spans two blocks
// and a bit, and a detector that fires on one sample often needs to look back
// 200 samples to find where the event really stood, which may be in the block
// before last.
//
// Holding the whole recording is not possible, because the signal never ends.
// Copying the last N samples to the front of an array at every block is
// possible and wasteful: for a window of 500 that is 500 values moved for
// every 320 that arrive, for ever.
//
// A ring buffer moves nothing. A new sample takes the place of the oldest, and
// only one position changes. Putting a sample in costs the same whether the
// buffer holds ten samples or ten thousand.
//
// THE AGE, NOT THE POSITION. ringbuf_get takes an age: 0 is the newest sample,
// 1 the one before it. A position would change its meaning as the buffer
// fills; an age does not.
//
// This example also shows what NOT to reach for. A mean over a sliding window
// is most often written as a filter whose coefficients are all the same, and
// that costs one multiplication for each coefficient for each sample. movavg
// gives the same answer in a fixed time.
//
// TO PORT THIS: replace next_block with your own interrupt or read.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_STREAM_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/filter/movavg.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE     REAL_C(32000.0)
#define BLOCK           320u
#define BLOCKS          40u
#define WINDOW          500u
#define LOOK_BACK       200u

// The energy of the signal with the machine quiet, measured once when the
// machine was commissioned. A threshold worked out from the signal itself
// would be raised by the very event it is meant to catch, because the event
// sits inside the window that measures the quiet.
#define QUIET_ENERGY    REAL_C(0.005)

// Where the event is put into the signal, and how wide it is. It is placed so
// that its peak falls near the end of one block and the detector fires in the
// next one, which is the case a program that kept only one block could not
// answer at all.
#define EVENT_START     6040u
#define EVENT_WIDTH     60u
#define EVENT_PEAK      (EVENT_START + (EVENT_WIDTH / 2u))
#define PI              REAL_C(3.14159265358979323846)

static real_t block[BLOCK];
static uint32_t seed = 1u;

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;
    return ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// REPLACE THIS with your own interrupt or read. It fills one block of BLOCK
// samples, as a converter would.
static void next_block(uint32_t number)
{
    for(uint32_t index = 0; index < BLOCK; index++)
    {
        uint32_t whole = (number * BLOCK) + index;
        real_t time = (real_t)whole / SAMPLE_RATE;

        // A quiet signal with a sharp event in the middle of the run.
        real_t value = REAL_C(0.1) * rough();

        if((whole >= EVENT_START) && (whole < (EVENT_START + EVENT_WIDTH)))
        {
            value += REAL_SIN(PI * (real_t)(whole - EVENT_START)
                              / (real_t)EVENT_WIDTH);
        }

        block[index] = value + (REAL_C(0.05)
                                * REAL_SIN(REAL_C(2.0) * PI * REAL_C(50.0)
                                           * time));
    }
}

int main(void)
{
    // The buffer must hold everything the work looks back over, which is the
    // longer of the window and the look back.
    ringbuf_t history = ringbuf_alloc(WINDOW);
    movavg_t level = movavg_alloc(WINDOW);

    // The detector adds up the energy over a shorter window. A window must
    // fill before it can say anything, thus a detector of this shape ALWAYS
    // fires after the event and never on it. That is why the look back exists.
    movavg_t energy = movavg_alloc(LOOK_BACK / 2u);

    printf("A converter gives %u samples every 10 ms.\n", BLOCK);
    printf("The work needs a mean over the last %u samples and a look back\n",
           WINDOW);
    printf("of %u samples. Neither fits inside one block.\n\n", LOOK_BACK);

    printf("The ring buffer holds %u samples and moves nothing when a sample\n",
           WINDOW);
    printf("arrives. Copying the window to the front of an array instead\n");
    printf("would move %u values for every %u that arrive, for ever.\n\n",
           WINDOW, BLOCK);

    printf("The signal is quiet, with one sharp event in it. A detector adds\n");
    printf("up the energy over %u samples and fires at the TOP of that,\n",
           LOOK_BACK / 2u);
    printf("which stands well after the event itself.\n\n");

    uint32_t found_at = 0;
    uint32_t fired_at = 0;
    real_t largest_seen = REAL_C(0.0);
    real_t previous_loudness = REAL_C(0.0);
    uint32_t whole = 0;

    for(uint32_t number = 0; number < BLOCKS; number++)
    {
        next_block(number);

        for(uint32_t index = 0; index < BLOCK; index++)
        {
            real_t sample = block[index];

            ringbuf_put(&history, sample);
            movavg_process_sample(&level, sample);
            real_t loudness = movavg_process_sample(&energy, sample * sample);

            // Fire at the TOP of the energy and not on its way up. The top is
            // where the window sits over the whole event, thus it stands well
            // after the event itself. That delay is not a fault to be tuned
            // away; it is what a window costs, and the look back is how it is
            // paid for.
            bool rising_stopped =
                (previous_loudness > (REAL_C(20.0) * QUIET_ENERGY))
                && (loudness <= previous_loudness);

            if(movavg_is_full(&level) && rising_stopped && (found_at == 0u))
            {
                // It fired. Where did the event really stand? Look back over
                // the history, which reaches into the blocks before this one.
                uint32_t best_age = 0;
                real_t largest = REAL_C(0.0);

                for(uint32_t age = 0; age < LOOK_BACK; age++)
                {
                    real_t old = ringbuf_get(&history, age);
                    if(old > largest) { largest = old; best_age = age; }
                }

                found_at = whole - best_age;
                fired_at = whole;
                largest_seen = largest;

                printf("The detector fired at sample %u, in block %u.\n",
                       whole, number);
                printf("Looking back %u samples, the event really stood at\n",
                       best_age);
                printf("sample %u, which arrived in block %u.\n\n",
                       found_at, found_at / BLOCK);
            }

            previous_loudness = loudness;
            whole++;
        }
    }

    printf("The peak of the event was put in at sample %u. The detector\n",
           EVENT_PEAK);
    printf("found it at %u, and its size there was %.3f.\n\n",
           found_at, largest_seen);

    if((found_at / BLOCK) != (fired_at / BLOCK))
    {
        printf("The event and the sample that fired the detector fell in\n");
        printf("DIFFERENT blocks: block %u and block %u. A program that kept\n",
               found_at / BLOCK, fired_at / BLOCK);
        printf("only the block in hand could not have found the event at all,\n");
        printf("and one that kept two blocks would only just have reached it.\n\n");
    }

    printf("Two ways to write the same mean, and what each costs for one\n");
    printf("sample with a window of %u:\n", WINDOW);
    printf("  a filter whose coefficients are all the same : %u multiplications\n",
           WINDOW);
    printf("  movavg                                       : 1 add, 1 subtract\n");
    printf("\nBoth give the same answer. Below a window of about 16 the plain\n");
    printf("filter is faster, because the bookkeeping costs more than four\n");
    printf("multiplications do. From there upwards it is not close.\n");

    ringbuf_free(&history);
    movavg_free(&level);
    movavg_free(&energy);

    return 0;
}

#endif//RUN_EXAMPLE
