#ifndef TEST
#include <ffitt/transform/slide.h>
#include <ffitt/core/defs.h>
#else
#include "slide.h"
#include "defs.h"
#endif

#include <math.h>

#include <stdlib.h>

#define SLIDE_TWO_PI    REAL_C(6.283185307179586476925286766559)

bool slide_is_valid_size(uint32_t size)
{
    return size >= 2u;
}

bool slide_is_valid_damping(real_t damping)
{
    return (damping > REAL_C(0.0)) && (damping <= REAL_C(1.0));
}

// What the sample that leaves the window is multiplied by.
//
// A sample that arrived N steps ago has been through the damping N times since,
// thus taking it away undamped would take away more than was ever put in. This
// is the damping raised to the size, worked out once here and not at each
// sample.
static real_t slide_departing_weight(real_t damping, uint32_t size)
{
    real_t weight = REAL_C(1.0);

    for(uint32_t step = 0; step < size; step++)
    {
        weight *= damping;
    }

    return weight;
}

// The turning factor of one bin, with the damping already in it, so that the
// running total needs one multiplication and not two.
static cnum_t slide_turn_of(uint32_t bin, uint32_t size, real_t damping)
{
    real_t angle = (SLIDE_TWO_PI * (real_t)bin) / (real_t)size;

    return cnum_make(damping * REAL_COS(angle), damping * REAL_SIN(angle));
}

static void slide_build(slide_t* slide, uint32_t size, uint32_t count)
{
    slide->size = size;
    slide->count = count;
    slide->seen = 0;
    slide->damping = SLIDE_DAMPING;
    slide->departing = slide_departing_weight(SLIDE_DAMPING, size);

    for(uint32_t index = 0; index < count; index++)
    {
        slide->total[index] = cnum_zero();

        // Nothing is watched until slide_watch says so. Bin 0 turns by no
        // angle at all, thus a watcher left alone follows the level of the
        // window and never something it was not asked for.
        slide->turn[index] = slide_turn_of(0u, size, SLIDE_DAMPING);
    }
}

slide_t slide_alloc(uint32_t size, uint32_t count)
{
    ASSERT(slide_is_valid_size(size));
    ASSERT(count > 0u);

    slide_t slide;

    slide.history = ringbuf_alloc(size);
    slide.total = (cnum_t*)malloc(sizeof(cnum_t) * count);
    slide.turn = (cnum_t*)malloc(sizeof(cnum_t) * count);
    slide.dynamic_alloc = true;

    // The building below writes through both lists and the window, thus it
    // must not be reached with nothing to write to.
    if((slide.history.size == 0u) || (slide.total == NULL)
       || (slide.turn == NULL))
    {
        slide_free(&slide);

        slide.total = NULL;
        slide.turn = NULL;
        slide.size = 0;
        slide.count = 0;
        slide.dynamic_alloc = false;

        return slide;
    }

    slide_build(&slide, size, count);

    return slide;
}

slide_t slide_static_alloc(uint32_t size, uint32_t count, real_t* window,
                           cnum_t* total, cnum_t* turn)
{
    ASSERT(slide_is_valid_size(size));
    ASSERT(count > 0u);
    ASSERT(window != NULL);
    ASSERT(total != NULL);
    ASSERT(turn != NULL);

    slide_t slide;

    slide.history = ringbuf_static_alloc(size, window);
    slide.total = total;
    slide.turn = turn;
    slide.dynamic_alloc = false;

    slide_build(&slide, size, count);

    return slide;
}

bool slide_design(slide_t* slide, real_t damping)
{
    ASSERT(slide != NULL);

    if(!slide_is_valid_damping(damping))
    {
        return false;
    }

    slide->damping = damping;
    slide->departing = slide_departing_weight(damping, slide->size);

    // Every turning factor carries the damping, thus each must be made again.
    // The bin each watcher follows is kept: the angle of the factor says which
    // bin it is, and that angle does not change.
    for(uint32_t index = 0; index < slide->count; index++)
    {
        real_t was = cnum_magnitude(slide->turn[index]);

        if(was > REAL_C(0.0))
        {
            slide->turn[index] = cnum_scale(slide->turn[index],
                                            damping / was);
        }
    }

    slide_reset(slide);

    return true;
}

bool slide_watch(slide_t* slide, uint32_t index, uint32_t bin)
{
    ASSERT(slide != NULL);

    if((index >= slide->count) || (bin >= slide->size))
    {
        return false;
    }

    slide->turn[index] = slide_turn_of(bin, slide->size, slide->damping);
    slide->total[index] = cnum_zero();

    return true;
}

real_t slide_bin_frequency(const slide_t* slide, uint32_t bin,
                           real_t sample_rate)
{
    ASSERT(slide != NULL);

    return (sample_rate * (real_t)bin) / (real_t)slide->size;
}

void slide_reset(slide_t* slide)
{
    ASSERT(slide != NULL);

    ringbuf_reset(&slide->history);
    slide->seen = 0;

    for(uint32_t index = 0; index < slide->count; index++)
    {
        slide->total[index] = cnum_zero();
    }
}

void slide_process_sample(slide_t* slide, real_t sample)
{
    ASSERT(slide != NULL);

    // The sample that falls off the end of the window. Until the window is
    // full nothing has fallen off yet, thus nothing is taken away.
    real_t leaving = REAL_C(0.0);

    if(ringbuf_is_full(&slide->history))
    {
        leaving = ringbuf_get(&slide->history, slide->size - 1u);
    }

    real_t moved = sample - (slide->departing * leaving);

    for(uint32_t index = 0; index < slide->count; index++)
    {
        cnum_t stood = cnum_add(slide->total[index],
                                cnum_from_real(moved));

        slide->total[index] = cnum_multiply(stood, slide->turn[index]);
    }

    ringbuf_put(&slide->history, sample);

    if(slide->seen < slide->size)
    {
        slide->seen++;
    }
}

void slide_process_block(slide_t* slide, const real_t* input, uint32_t count)
{
    ASSERT(slide != NULL);
    ASSERT(input != NULL);

    for(uint32_t index = 0; index < count; index++)
    {
        slide_process_sample(slide, input[index]);
    }
}

bool slide_is_full(const slide_t* slide)
{
    ASSERT(slide != NULL);

    return slide->seen >= slide->size;
}

cnum_t slide_get(const slide_t* slide, uint32_t index)
{
    ASSERT(slide != NULL);

    if(index >= slide->count)
    {
        return cnum_zero();
    }

    return slide->total[index];
}

real_t slide_magnitude(const slide_t* slide, uint32_t index)
{
    return cnum_magnitude(slide_get(slide, index));
}

void slide_free(slide_t* slide)
{
    ASSERT(slide != NULL);

    ringbuf_free(&slide->history);

    if(slide->dynamic_alloc)
    {
        free(slide->total);
        free(slide->turn);
        slide->dynamic_alloc = false;
    }
}
