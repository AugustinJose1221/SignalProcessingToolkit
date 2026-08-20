#ifndef TEST
#include <sptk/util/valleydetect.h>
#include <sptk/core/defs.h>
#else
#include "valleydetect.h"
#include "defs.h"
#endif//TEST

uint32_t valleydetect_get_valley(real_t* input, real_t* index_buffer, real_t* valley_buffer, uint32_t size)
{
    ASSERT(input != NULL);
    ASSERT(index_buffer != NULL);
    ASSERT(size > 0);

    uint32_t valleycount = 0;

    if(size > 2)
    {
        for(uint32_t index = 1; index < size-1; index++)
        {
            if(input[index] < input[index-1] && input[index] < input[index+1])
            {
                valley_buffer[valleycount] = input[index];
                index_buffer[valleycount++] = (real_t)index;
            }
        }
    }

    return valleycount;
}
