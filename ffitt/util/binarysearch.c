// This file is left out of the build when FFITT_NO_UTIL is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_UTIL

#ifndef TEST
#include <ffitt/util/binarysearch.h>
#include <ffitt/core/defs.h>
#else
#include "binarysearch.h"
#include "defs.h"
#endif


// Give the index of the first value that is not less than the given value.
//
// If every value of the list is less than the given value, the search stops at
// size. That is not an index of the list, and a caller that reads the list at
// that place reads memory after the end of the list. Thus the result stays
// below size, and it is always an index that the caller can use.
uint32_t binarysearch_get_index(real_t* data, real_t value, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);

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

    if (left >= size)
    {
        left = size - 1;
    }

    return left;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int binarysearch_is_not_in_this_build_t;

#endif//FFITT_NO_UTIL
