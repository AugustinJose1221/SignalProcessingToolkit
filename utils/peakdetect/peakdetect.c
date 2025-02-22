#ifndef TEST
#include <utils/peakdetect/peakdetect.h>
#include <common/defs.h>
#else
#include "peakdetect.h"
#include "defs.h"
#endif

uint32_t peakdetect_get_peaks(float* input, float* index_buffer, float* peak_buffer, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(index_buffer != NULL);
    ASSERT(size > 0);

    uint32_t peakcount = 0;

    if(size > 2)
    {
        for(int index = 1; index < size-1; index++)
        {
            if(input[index] > input[index-1] && input[index] > input[index+1])
            {
                peak_buffer[peakcount] = input[index];
                index_buffer[peakcount] = (float)index;
                peakcount++;
            }
        }
    }

    return peakcount;
}