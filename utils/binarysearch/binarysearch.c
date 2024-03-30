#include <utils/binarysearch/binarysearch.h>
#include <assert.h>

uint32_t binarysearch_get_index(float* data, float value, uint32_t size)
{
    assert(data != NULL);
    assert(size > 0);
    assert(value >= data[0] && value <= data[size - 1]);

    int i = 0;
    int j = size - 1;

    while(j-i > 1)
    {
        int mid = (i+j)/2;
        if(value > data[mid])
        {
            i = mid;
        }
        else
        {
            j = mid;
        }
    }

    return i;
}