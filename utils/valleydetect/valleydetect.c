#ifndef TEST
#include <utils/valleydetect/valleydetect.h>
#include <common/defs.h>
#else
#include "valleydetect.h"
#include "defs.h"
#endif//TEST

uint32_t valleydetect_get_valley(float* input, float* index_buffer, float* valley_buffer, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(index_buffer != NULL);
    ASSERT(size > 0);

    uint32_t valleycount = 0;

    if(size > 2)
    {
        for(int index = 1; index < size-1; index++)
        {
            if(input[index] < input[index-1] && input[index] < input[index+1])
            {
                valley_buffer[valleycount] = input[index];
                index_buffer[valleycount++] = (float)index;
            }
        }
    }

    return valleycount;
}