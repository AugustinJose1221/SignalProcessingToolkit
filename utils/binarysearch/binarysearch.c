#ifndef TEST
#include <utils/binarysearch/binarysearch.h>
#include <common/defs.h>
#else
#include "binarysearch.h"
#include "defs.h"
#endif


uint32_t binarysearch_get_index(float* data, float value, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);
    ASSERT(value >= data[0] && value <= data[size - 1]);

    uint32_t left = 0, right = size;
    
    while (left < right) 
    {
        uint32_t mid = left + (right - left) / 2;

        if (data[mid] < value) 
        {
            left = mid + 1;
        } 
        else 
        {
            right = mid;
        }
    }
    
    return left;
}