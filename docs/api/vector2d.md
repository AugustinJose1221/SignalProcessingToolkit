# vector2d

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Vectors with two values. Declared in `sptk/linalg/vector2d.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Functions

### `vector2d_alloc`

```c
vector_t vector2d_alloc();
```

Give a vector with two values. The memory comes from the heap. Give the
vector to vector_free when you no longer need it.

### `vector2d_static_alloc`

```c
vector_t vector2d_static_alloc(float* mempool);
```

Give a vector with two values that uses the memory at mempool. That memory
must hold two float values. This function takes no memory from the heap.

### `vector2d_add_point_at_index`

```c
void vector2d_add_point_at_index(vector_t* vector, uint32_t index, float data);
```

Write a value into the vector at the given index. The index must be 0 or 1.

### `vector2d_add_from_array`

```c
void vector2d_add_from_array(vector_t* vector, float* data);
```

Write two values from an array into the vector.

### `vector2d_printf`

```c
void vector2d_printf(vector_t* vector, int (*func)(const char *, ...));
```

Write the vector, one value for each line. Give NULL as the function to
write with printf.

### `vector2d_get`

```c
float vector2d_get(vector_t* vector, uint32_t index);
```

Give the value of the vector at the given index. The index must be 0 or 1.

### `vector2d_dot_product`

```c
float vector2d_dot_product(vector_t* x, vector_t* y);
```

Give the dot product of the two vectors.

### `vector2d_norm`

```c
float vector2d_norm(vector_t* x);
```

Give the length of the vector.
