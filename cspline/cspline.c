#ifndef TEST
#include <cspline/cspline.h>
#include <common/defs.h>
#include <utils/binarysearch/binarysearch.h>
#include <math.h>
#else
#include "cspline.h"
#include "binarysearch.h"
#include "defs.h"
#include <math.h>
#endif

static void load_coordinates(cspline_t* cspline, float* x, float* y);
static void calculate_derivatives(cspline_t* cspline, cspline_mempool_t mempool);
static void initialize_state_buffers(cspline_t* cspline, cspline_mempool_t mempool);
static void update_state_buffers(cspline_t* cspline, cspline_mempool_t mempool);
static void update_coefficients(cspline_t* cspline, cspline_mempool_t mempool);

cspline_t cspline_alloc(uint32_t size)
{
    ASSERT(size != 0);

    cspline_t cspline;
    
    cspline.size = size;
    cspline.x = (float*)malloc(size*sizeof(float));
    cspline.y = (float*)malloc(size*sizeof(float));
    cspline.b = (float*)malloc(size*sizeof(float));
    cspline.c = (float*)malloc((size-1)*sizeof(float));
    cspline.d = (float*)malloc((size-1)*sizeof(float));
    cspline.dynamic_alloc = true;
    
    return cspline;
}

cspline_t cspline_static_alloc(uint32_t size, float** membank)
{
    ASSERT(size != 0);
    ASSERT(membank != NULL);

    cspline_t cspline;

    cspline.size = size;
    cspline.x = (float*)membank[0]; // Buffer of size N
    cspline.y = (float*)membank[1]; // Buffer of size N
    cspline.b = (float*)membank[2]; // Buffer of size N
    cspline.c = (float*)membank[3]; // Buffer of size N-1
    cspline.d = (float*)membank[4]; // Buffer of size N-1
    cspline.dynamic_alloc = false;

    return cspline;
}

cspline_mempool_t cspline_alloc_mempool(uint32_t size)
{
    ASSERT(size != 0);

    cspline_mempool_t mempool;

    mempool.d = (float*)malloc(size*sizeof(float));
    mempool.b = (float*)malloc(size*sizeof(float));
    mempool.q = (float*)malloc((size-1)*sizeof(float));
    mempool.dp = (float*)malloc((size-1)*sizeof(float));
    mempool.dx = (float*)malloc((size-1)*sizeof(float));
    mempool.dynamic_alloc = true;

    return mempool;
}

cspline_mempool_t cspline_static_alloc_mempool(float** membank)
{
    ASSERT(membank != NULL);

    cspline_mempool_t mempool;

    mempool.d = (float*)membank[0];     // Buffer of size N
    mempool.b = (float*)membank[1];     // Buffer of size N
    mempool.q = (float*)membank[2];     // Buffer of size N-1
    mempool.dp = (float*)membank[3];    // Buffer of size N-1
    mempool.dx = (float*)membank[4];    // Buffer of size N-1
    mempool.dynamic_alloc = false;

    return mempool;
}

void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, float* x, float* y)
{
    ASSERT(cspline != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);
    ASSERT(mempool.d != NULL);  
    ASSERT(mempool.b != NULL);  
    ASSERT(mempool.q != NULL);  
    ASSERT(mempool.dp != NULL); 
    ASSERT(mempool.dx != NULL); 

    load_coordinates(cspline, x, y);
    calculate_derivatives(cspline, mempool);
    initialize_state_buffers(cspline, mempool);
    update_state_buffers(cspline, mempool);
    update_coefficients(cspline, mempool);
}

void cspline_update_size(cspline_t* cspline, uint32_t size)
{
    cspline->size = size;
}

float cspline_get_interpolated_point(cspline_t* cspline, float x)
{
    int i = binarysearch_get_index(cspline->x, x, cspline->size);
    float dx = x - cspline->x[i];
    float y = cspline->y[i] + (cspline->b[i]*dx) + (cspline->c[i]*pow(dx,2)) + (cspline->d[i]*pow(dx,3));
    return y;
}

void cspline_free(cspline_t cspline)
{
    if(cspline.dynamic_alloc)
    {
        free(cspline.x);
        free(cspline.y);
        free(cspline.b);
        free(cspline.c);
        free(cspline.d);
    }
}

void cspline_free_mempool(cspline_mempool_t mempool)
{
    if(mempool.dynamic_alloc)
    {
        free(mempool.d);
        free(mempool.b);
        free(mempool.q);
        free(mempool.dp);
        free(mempool.dx);
    }
}

static void load_coordinates(cspline_t* cspline, float* x, float* y)
{
    ASSERT(cspline != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    for(int index = 0; index < cspline->size; index++)
    {
        cspline->x[index] = x[index];
        cspline->y[index] = y[index];
    }
}

static void calculate_derivatives(cspline_t* cspline, cspline_mempool_t mempool)
{
    ASSERT(cspline != NULL);
    ASSERT(mempool.dx != NULL);
    ASSERT(mempool.dp != NULL);

    for(int index = 0; index < cspline->size - 1; index++)
    {
        mempool.dx[index] = cspline->x[index+1] - cspline->x[index];                     
        ASSERT(mempool.dx[index] > 0);
        mempool.dp[index] = (cspline->y[index+1] - cspline->y[index])/mempool.dx[index];            
    }
}

static void initialize_state_buffers(cspline_t* cspline, cspline_mempool_t mempool)
{
    ASSERT(cspline != NULL);
    ASSERT(mempool.d != NULL);
    ASSERT(mempool.b != NULL);
    ASSERT(mempool.q != NULL);
    ASSERT(mempool.dp != NULL);

    mempool.d[0] = 2;
    mempool.d[cspline->size-1] = 2;
    mempool.b[0] = 3*mempool.dp[0];
    mempool.b[cspline->size-1] =  3*mempool.dp[cspline->size-2];
    mempool.q[0] = 1;
}

static void update_state_buffers(cspline_t* cspline, cspline_mempool_t mempool)
{
    ASSERT(cspline != NULL);
    ASSERT(mempool.d != NULL);
    ASSERT(mempool.b != NULL);
    ASSERT(mempool.q != NULL);
    ASSERT(mempool.dp != NULL);
    ASSERT(mempool.dx != NULL);

    for(int index = 0; index < cspline->size-2; index++)
    {
        mempool.d[index+1] = ((2*mempool.dx[index])/mempool.dx[index+1]) + 2;
        mempool.b[index+1] = 3*(mempool.dp[index]+(mempool.dp[index+1]*mempool.dx[index]/mempool.dx[index+1]));
        mempool.q[index+1] = mempool.dx[index]/mempool.dx[index+1];
    }

    for(int index = 1; index < cspline->size; index++)
    {
        mempool.d[index] -= mempool.q[index-1]/mempool.d[index-1];
        mempool.b[index] -= mempool.b[index-1]/mempool.d[index-1];
    }
}

static void update_coefficients(cspline_t* cspline, cspline_mempool_t mempool)
{
    ASSERT(cspline != NULL);
    ASSERT(mempool.d != NULL);
    ASSERT(mempool.b != NULL);
    ASSERT(mempool.q != NULL);
    ASSERT(mempool.dp != NULL);
    ASSERT(mempool.dx != NULL);

    cspline->b[cspline->size-1] = mempool.b[cspline->size-1] / mempool.d[cspline->size-1];

    for(int index = cspline->size-2; index >= 0; --index)
    {
        cspline->b[index] = (mempool.b[index] - (mempool.q[index]*cspline->b[index+1]))/mempool.d[index];
    }

    for(int index = 0; index < cspline->size-1; index++)
    {
        cspline->c[index] = ((-2*cspline->b[index]) - cspline->b[index+1] + (3*mempool.dp[index]))/mempool.dx[index];
        cspline->d[index] = (cspline->b[index] + cspline->b[index+1] - (2*mempool.dp[index]))/(mempool.dx[index]*mempool.dx[index]);
    }
}