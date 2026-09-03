// This file is left out of the build when FFITT_NO_UTIL is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_UTIL

#ifndef TEST
#include <ffitt/util/valleydetect.h>
#include <ffitt/core/defs.h>
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

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int valleydetect_is_not_in_this_build_t;

#endif//FFITT_NO_UTIL
