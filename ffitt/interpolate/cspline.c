#ifndef TEST
#include <ffitt/interpolate/cspline.h>
#include <ffitt/core/defs.h>
#include <ffitt/util/binarysearch.h>
#include <math.h>
#else
#include "cspline.h"
#include "binarysearch.h"
#include "defs.h"
#include <math.h>
#endif

static void load_coordinates(cspline_t* cspline, real_t* x, real_t* y);
static void calculate_derivatives(cspline_t* cspline, cspline_mempool_t mempool);
static void initialize_state_buffers(cspline_t* cspline, cspline_mempool_t mempool);
static void update_state_buffers(cspline_t* cspline, cspline_mempool_t mempool);
static void update_coefficients(cspline_t* cspline, cspline_mempool_t mempool);

cspline_t cspline_alloc(uint32_t size)
{
    ASSERT(size != 0);

    cspline_t cspline;
    
    cspline.size = size;
    cspline.x = (real_t*)malloc(size*sizeof(real_t));
    cspline.y = (real_t*)malloc(size*sizeof(real_t));
    cspline.b = (real_t*)malloc(size*sizeof(real_t));
    cspline.c = (real_t*)malloc((size-1)*sizeof(real_t));
    cspline.d = (real_t*)malloc((size-1)*sizeof(real_t));
    cspline.dynamic_alloc = true;

    if((cspline.x == NULL) || (cspline.y == NULL) || (cspline.b == NULL)
       || (cspline.c == NULL) || (cspline.d == NULL))
    {
        cspline_free(cspline);

        cspline.x = NULL;
        cspline.y = NULL;
        cspline.b = NULL;
        cspline.c = NULL;
        cspline.d = NULL;
        cspline.size = 0;
        cspline.dynamic_alloc = false;
    }

    return cspline;
}

cspline_t cspline_static_alloc(uint32_t size, real_t** membank)
{
    ASSERT(size != 0);
    ASSERT(membank != NULL);

    cspline_t cspline;

    cspline.size = size;
    cspline.x = (real_t*)membank[0]; // Buffer of size N
    cspline.y = (real_t*)membank[1]; // Buffer of size N
    cspline.b = (real_t*)membank[2]; // Buffer of size N
    cspline.c = (real_t*)membank[3]; // Buffer of size N-1
    cspline.d = (real_t*)membank[4]; // Buffer of size N-1
    cspline.dynamic_alloc = false;

    return cspline;
}

cspline_mempool_t cspline_alloc_mempool(uint32_t size)
{
    ASSERT(size != 0);

    cspline_mempool_t mempool;

    mempool.d = (real_t*)malloc(size*sizeof(real_t));
    mempool.b = (real_t*)malloc(size*sizeof(real_t));
    mempool.q = (real_t*)malloc((size-1)*sizeof(real_t));
    mempool.dp = (real_t*)malloc((size-1)*sizeof(real_t));
    mempool.dx = (real_t*)malloc((size-1)*sizeof(real_t));
    mempool.dynamic_alloc = true;

    if((mempool.d == NULL) || (mempool.b == NULL) || (mempool.q == NULL)
       || (mempool.dp == NULL) || (mempool.dx == NULL))
    {
        cspline_free_mempool(mempool);

        mempool.d = NULL;
        mempool.b = NULL;
        mempool.q = NULL;
        mempool.dp = NULL;
        mempool.dx = NULL;
        mempool.dynamic_alloc = false;
    }

    return mempool;
}

cspline_mempool_t cspline_static_alloc_mempool(real_t** membank)
{
    ASSERT(membank != NULL);

    cspline_mempool_t mempool;

    mempool.d = (real_t*)membank[0];     // Buffer of size N
    mempool.b = (real_t*)membank[1];     // Buffer of size N
    mempool.q = (real_t*)membank[2];     // Buffer of size N-1
    mempool.dp = (real_t*)membank[3];    // Buffer of size N-1
    mempool.dx = (real_t*)membank[4];    // Buffer of size N-1
    mempool.dynamic_alloc = false;

    return mempool;
}

void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, real_t* x, real_t* y)
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

real_t cspline_get_interpolated_point(cspline_t* cspline, real_t x)
{
    // The binary search gives the first knot that is not less than x. The
    // coefficients b, c and d belong to the interval that starts at the knot
    // on the left of x. Thus the search result must move one knot to the left.
    // The arrays c and d hold size-1 elements, one for each interval, thus the
    // index must stay below size-1.
    uint32_t i = binarysearch_get_index(cspline->x, x, cspline->size);

    if(i > 0)
    {
        i--;
    }
    if(i > cspline->size - 2)
    {
        i = cspline->size - 2;
    }

    real_t dx = x - cspline->x[i];
    real_t y = cspline->y[i] + (cspline->b[i]*dx) + (cspline->c[i]*REAL_POW(dx,2)) + (cspline->d[i]*REAL_POW(dx,3));
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

static void load_coordinates(cspline_t* cspline, real_t* x, real_t* y)
{
    ASSERT(cspline != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    for(uint32_t index = 0; index < cspline->size; index++)
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

    for(uint32_t index = 0; index < cspline->size - 1; index++)
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

    for(uint32_t index = 0; index < cspline->size-2; index++)
    {
        mempool.d[index+1] = ((2*mempool.dx[index])/mempool.dx[index+1]) + 2;
        mempool.b[index+1] = 3*(mempool.dp[index]+(mempool.dp[index+1]*mempool.dx[index]/mempool.dx[index+1]));
        mempool.q[index+1] = mempool.dx[index]/mempool.dx[index+1];
    }

    for(uint32_t index = 1; index < cspline->size; index++)
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

    for(uint32_t index = 0; index < cspline->size-1; index++)
    {
        cspline->c[index] = ((-2*cspline->b[index]) - cspline->b[index+1] + (3*mempool.dp[index]))/mempool.dx[index];
        cspline->d[index] = (cspline->b[index] + cspline->b[index+1] - (2*mempool.dp[index]))/(mempool.dx[index]*mempool.dx[index]);
    }
}
