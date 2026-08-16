# cspline

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Cubic splines. Declared in `sptk/interpolate/cspline.h`.

[Back to the index](../API.md)

## Types

### `cspline_t`

A cubic spline through a set of points.

A spline gives a smooth curve through the given points. Between two
neighbouring points it follows a polynomial of the third power, and at each
point the curve has no step and no corner.

The spline holds the points and the three coefficients of each interval. The
arrays b, c and d hold one value for each interval, thus they hold one value
less than the number of points.

```c
typedef struct{
    uint32_t size;
    float* x;
    float* y;
    float* b;
    float* c;
    float* d;
    bool dynamic_alloc;
}cspline_t;
```

### `cspline_mempool_t`

The memory that cspline_init needs while it calculates the coefficients.

This memory is separate from the spline, because several splines can share
one memory pool, one after the other. The pool holds nothing that the spline
needs after cspline_init gives back.

```c
typedef struct{
    float* dx;
    float* dp;
    float* d;
    float* b;
    float* q;
    bool dynamic_alloc;
}cspline_mempool_t;
```

## Functions

### `cspline_alloc`

```c
cspline_t cspline_alloc(uint32_t size);
```

Give a spline for the given number of points. The memory comes from the
heap. Give the spline to cspline_free when you no longer need it.

### `cspline_static_alloc`

```c
cspline_t cspline_static_alloc(uint32_t size, float** membank);
```

Give a spline that uses the memory that the caller holds. The parameter
membank is a list of five pointers. Each of them must hold room for as many
float values as the given size. This function takes no memory from the heap.

### `cspline_alloc_mempool`

```c
cspline_mempool_t cspline_alloc_mempool(uint32_t size);
```

Give a memory pool for a spline of the given number of points. The memory
comes from the heap. Give the pool to cspline_free_mempool.

### `cspline_static_alloc_mempool`

```c
cspline_mempool_t cspline_static_alloc_mempool(float** membank);
```

Give a memory pool that uses the memory that the caller holds. The parameter
membank is a list of five pointers. This function takes no memory from the
heap.

### `cspline_init`

```c
void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, float* x, float* y);
```

Calculate the coefficients of the spline for the given points.

The values of x must rise, and no two of them may be the same. The lists x
and y must hold as many values as the size of the spline. The memory pool
must be as large as the spline. The pool holds nothing after the call.

### `cspline_update_size`

```c
void cspline_update_size(cspline_t* cspline, uint32_t size);
```

Change the number of points that the spline uses. The new size must not be
larger than the size that the allocation gave. Call cspline_init after this
function, because the coefficients belong to the points of the old size.

### `cspline_get_interpolated_point`

```c
float cspline_get_interpolated_point(cspline_t* cspline, float x);
```

Give the value of the curve at the position x.

A position between the first knot and the last knot gives a point on the
curve. A position outside that range gives the polynomial of the nearest
interval, which moves away from the points quickly. Call cspline_init before
this function.

### `cspline_free`

```c
void cspline_free(cspline_t cspline);
```

Release the memory of a spline that came from cspline_alloc. This function
does nothing for a spline that came from cspline_static_alloc.

The parameter is the structure itself and not a pointer to it, thus the
caller must not use the structure after the call.

### `cspline_free_mempool`

```c
void cspline_free_mempool(cspline_mempool_t mempool);
```

Release the memory of a pool that came from cspline_alloc_mempool. This
function does nothing for a pool that came from cspline_static_alloc_mempool.
