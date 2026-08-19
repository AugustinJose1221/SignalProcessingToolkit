#ifndef TEST
#include <sptk/filter/dcblock.h>
#include <sptk/core/defs.h>
#else
#include "dcblock.h"
#include "defs.h"
#endif

#include <math.h>

#define DCBLOCK_PI      3.14159265358979323846

bool dcblock_is_valid_cutoff(float cutoff)
{
    return (cutoff >= DCBLOCK_MIN_CUTOFF) && (cutoff < 0.5f);
}

dcblock_t dcblock_init(float cutoff)
{
    dcblock_t dcblock;

    dcblock.level = 0.0;
    dcblock.started = false;

    if(dcblock_is_valid_cutoff(cutoff))
    {
        // One pole, worked out in double. For a low cutoff this number is very
        // small, and that is exactly what a float cannot hold: at a cutoff of
        // 0.000016, which is 0.5 Hz against 32 kHz, the pole is 0.0001 and the
        // number that a high pass would need beside it is 0.9999. A double
        // holds both and their difference.
        dcblock.pole = 2.0 * DCBLOCK_PI * (double)cutoff;

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

float dcblock_process_sample(dcblock_t* dcblock, float sample)
{
    ASSERT(dcblock != NULL);

    // Prime on the first sample. Without this the tracker would answer the
    // first sample as a step of the whole level, and that answer would be
    // larger than the signal for a long time.
    if(!dcblock->started)
    {
        dcblock->level = (double)sample;
        dcblock->started = true;
    }
    else
    {
        dcblock->level += dcblock->pole * ((double)sample - dcblock->level);
    }

    return (float)((double)sample - dcblock->level);
}

void dcblock_process_block(dcblock_t* dcblock, const float* input, float* output,
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

float dcblock_get_level(const dcblock_t* dcblock)
{
    ASSERT(dcblock != NULL);

    return (float)dcblock->level;
}

void dcblock_set_level(dcblock_t* dcblock, float level)
{
    ASSERT(dcblock != NULL);

    dcblock->level = (double)level;
    dcblock->started = true;
}

void dcblock_reset(dcblock_t* dcblock)
{
    ASSERT(dcblock != NULL);

    dcblock->level = 0.0;
    dcblock->started = false;
}
