// This file is left out of the build when FFITT_NO_DECOMPOSE is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_DECOMPOSE

#ifndef TEST
#include <ffitt/decompose/imf.h>
#include <ffitt/core/defs.h>
#else
#include "imf.h"
#include "defs.h"
#endif

imf_t imf_alloc(uint32_t size)
{
    ASSERT(size > 0);

    imf_t imf;

    imf.size = size;
    imf.x = (real_t*)malloc(sizeof(real_t)*size);
    imf.y = (real_t*)malloc(sizeof(real_t)*size);
    imf.dynamic_alloc = true;

    if((imf.x == NULL) || (imf.y == NULL))
    {
        imf_free(imf);

        imf.x = NULL;
        imf.y = NULL;
        imf.size = 0;
        imf.dynamic_alloc = false;
    }

    return imf;
}

imf_t imf_static_alloc(uint32_t size, real_t* x, real_t* y)
{
    ASSERT(size > 0);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    imf_t imf;

    imf.size = size;
    imf.x = x;
    imf.y = y;
    imf.dynamic_alloc = false;

    return imf;
}

void imf_printf(imf_t* imf, print_t func)
{
    ASSERT(imf != NULL);
    
    print_t print_func;

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(uint32_t index = 0; index < imf->size; index++)
    {
        print_func("%f, %f\n", imf->x[index], imf->y[index]);
    }
}

void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, print_t func)
{
    ASSERT(imf != NULL);
    ASSERT(num_of_imf > 0);
    ASSERT(size > 0);

    print_t print_func;

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        for(uint32_t imf_index = 0; imf_index < num_of_imf; imf_index++)
        {
            print_func("%f", imf[imf_index].y[index]);
            if(imf_index == num_of_imf - 1)
            {
                print_func("\n");
            }
            else
            {
                print_func(", ");
            }
        }
    }
}

void imf_free(imf_t imf)
{
    if(imf.dynamic_alloc)
    {
        free(imf.x);
        free(imf.y);
    }
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int imf_is_not_in_this_build_t;

#endif//FFITT_NO_DECOMPOSE
