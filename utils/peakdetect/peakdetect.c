#include <utils/peakdetect/peakdetect.h>
#include <assert.h>

uint32_t peakdetect_get_peaks(float* input, float* index_buffer, float* peak_buffer, uint32_t size)
{
    assert(input != NULL);
    assert(index_buffer != NULL);
    assert(size > 0);

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