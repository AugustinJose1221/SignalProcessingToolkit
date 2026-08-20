#ifndef TEST
#include <sptk/filter/dcblock.h>
#include <sptk/core/defs.h>
#else
#include "dcblock.h"
#include "defs.h"
#endif

#include <math.h>

#define DCBLOCK_PI      3.14159265358979323846

bool dcblock_is_valid_cutoff(real_t cutoff)
{
    return (cutoff >= DCBLOCK_MIN_CUTOFF) && (cutoff < REAL_C(0.5));
}

dcblock_t dcblock_init(real_t cutoff)
{
    dcblock_t dcblock;

    dcblock.level = 0.0;
    dcblock.started = false;

    if(dcblock_is_valid_cutoff(cutoff))
    {
        // One pole. For a low cutoff this number is very small, and that is
        // the point: a high pass would have to hold 0.9999 beside it and then
        // subtract the two, and at a cutoff of 0.000016, which is 0.5 Hz
        // against 32 kHz, seven digits cannot carry that difference. A single
        // pole never forms it.
        dcblock.pole = 2.0 * DCBLOCK_PI * (real_t)cutoff;

        if(dcblock.pole > 1.0)
        {
            dcblock.pole = 1.0;
        }
    }
    else
    {
        // A tracker that cannot hold its cutoff follows nothing, thus it gives
        // every sample back as it was. That is safer than following at a rate
        // that was not asked for.
        dcblock.pole = 0.0;
    }

    return dcblock;
}

real_t dcblock_process_sample(dcblock_t* dcblock, real_t sample)
{
    ASSERT(dcblock != NULL);

    // Prime on the first sample. Without this the tracker would answer the
    // first sample as a step of the whole level, and that answer would be
    // larger than the signal for a long time.
    if(!dcblock->started)
    {
        dcblock->level = (real_t)sample;
        dcblock->started = true;
    }
    else
    {
        dcblock->level += dcblock->pole * ((real_t)sample - dcblock->level);
    }

    return (real_t)((real_t)sample - dcblock->level);
}

void dcblock_process_block(dcblock_t* dcblock, const real_t* input, real_t* output,
                           uint32_t size)
{
    ASSERT(dcblock != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = dcblock_process_sample(dcblock, input[index]);
    }
}

real_t dcblock_get_level(const dcblock_t* dcblock)
{
    ASSERT(dcblock != NULL);

    return (real_t)dcblock->level;
}

void dcblock_set_level(dcblock_t* dcblock, real_t level)
{
    ASSERT(dcblock != NULL);

    dcblock->level = (real_t)level;
    dcblock->started = true;
}

void dcblock_reset(dcblock_t* dcblock)
{
    ASSERT(dcblock != NULL);

    dcblock->level = 0.0;
    dcblock->started = false;
}
