#include <imf/imf.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

imf_t imf_alloc(uint32_t size)
{
    assert(size > 0);

    imf_t imf;

    imf.size = size;
    imf.x = (float*)malloc(sizeof(float)*size);
    imf.y = (float*)malloc(sizeof(float)*size);
    imf.dynamic_alloc = true;

    return imf;
}

imf_t imf_static_alloc(uint32_t size, float* x, float* y)
{
    assert(size > 0);
    assert(x != NULL);
    assert(y != NULL);

    imf_t imf;

    imf.size = size;
    imf.x = x;
    imf.y = y;
    imf.dynamic_alloc = false;

    return imf;
}

void imf_printf(imf_t* imf, int (*func)(const char*, ...))
{
    assert(imf != NULL);
    
    int (*print_func)(const char*, ...);

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(int index = 0; index < imf->size; index++)
    {
        print_func("%f, %f\n", imf->x[index], imf->y[index]);
    }
}

void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, int (*func)(const char*, ...))
{
    assert(imf != NULL);
    assert(num_of_imf > 0);
    assert(size > 0);

    int (*print_func)(const char*, ...);

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(int index = 0; index < size; index++)
    {
        for(int imf_index = 0; imf_index < num_of_imf; imf_index++)
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