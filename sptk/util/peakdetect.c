#ifndef TEST
#include <sptk/util/peakdetect.h>
#include <sptk/core/defs.h>
#else
#include "peakdetect.h"
#include "defs.h"
#endif

uint32_t peakdetect_get_peaks(real_t* input, real_t* index_buffer, real_t* peak_buffer, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(index_buffer != NULL);
    ASSERT(size > 0);

    uint32_t peakcount = 0;

    if(size > 2)
    {
        for(uint32_t index = 1; index < size-1; index++)
        {
            if(input[index] > input[index-1] && input[index] > input[index+1])
            {
                peak_buffer[peakcount] = input[index];
                index_buffer[peakcount] = (real_t)index;
                peakcount++;
            }
        }
    }

    return peakcount;
}
