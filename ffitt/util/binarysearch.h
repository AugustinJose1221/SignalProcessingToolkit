#ifndef BINARYSEARCH_H
#define BINARYSEARCH_H

#include <stdio.h>
#include <stdint.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// Give the index of the first value of the list that is not less than the
// given value. The values of the list must rise.
//
// The result is always an index that the caller can use. If every value of the
// list is less than the given value, the result is the index of the last
// value. Thus a caller that reads the list at the result never reads memory
// after the end of the list.
uint32_t binarysearch_get_index(real_t* data, real_t value, uint32_t size);

#endif//BINARYSEARCH_H
