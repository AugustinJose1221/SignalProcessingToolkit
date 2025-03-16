#ifndef TEST
#include <matrix/matrix.h>
#include <common/defs.h>
#else
#include "matrix.h"
#include "defs.h"
#endif



matrix_t matrix_alloc(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    matrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = (float*)malloc(sizeof(float)*m*n);
    matrix.dynamic_alloc = true;

    return matrix;
}

matrix_t matrix_static_alloc(uint32_t m, uint32_t n, float* elem)
{
    ASSERT(m > 0);
    ASSERT(n > 0);
    ASSERT(elem != NULL);

    matrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = elem;
    matrix.dynamic_alloc = false;

    return matrix;
}

// Operations

void matrix_add_element(matrix_t* matrix, uint32_t i, uint32_t j, float value)
{
    ASSERT(matrix != NULL);
    ASSERT(i >= 0 && i < matrix->m);
    ASSERT(j >= 0 && j < matrix->n);
    matrix->elem[(i*matrix->n)+j] = value;
}

float matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j)
{
    ASSERT(matrix != NULL);
    ASSERT(i >= 0 && i < matrix->m);
    ASSERT(j >= 0 && j < matrix->n);

    return matrix->elem[(i*matrix->n)+j];
}

matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index)
{
    ASSERT(matrix != NULL);
    ASSERT(row_index >= 0 && row_index < matrix->m);

    matrix_t row_matrix;

    row_matrix = matrix_alloc(1, matrix->n);

    for(int i = 0; i < matrix->n; i++)
    {
       matrix_add_element(&row_matrix, 0, i, matrix_get_element(matrix, row_index, i));
    }

    return row_matrix;
}

matrix_t matrix_get_nth_col(matrix_t* matrix, uint32_t col_index)
{
    ASSERT(matrix != NULL);
    ASSERT(col_index >= 0 && col_index < matrix->n);

    matrix_t col_matrix;

    col_matrix = matrix_alloc(matrix->m, 1);

    for(int i = 0; i < matrix->n; i++)
    {
        matrix_add_element(&col_matrix, i, 0, matrix_get_element(matrix, i, col_index));
    }

    return col_matrix;
}

matrix_t matrix_get_order(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    matrix_t order;

    order = matrix_alloc(1, 2);
    matrix_add_element(&order, 0, 0, matrix->m);
    matrix_add_element(&order, 0, 1, matrix->n);

    return order;
}

float matrix_trace(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    float trace = 0;

    for(int i = 0; i < matrix->m; i++)
    {
        trace += matrix_get_element(matrix, i, i);
    }

    return trace;
}

float matrix_determinant(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    float determinant = 0;
    float intermediate = 0;
    float sign = -1;
    int row_index;
    int col_index;
    matrix_t inner_matrix;

    if(matrix->m == 3)
    {
        determinant = (matrix_get_element(matrix, 0, 0)*((matrix_get_element(matrix, 1, 1) * matrix_get_element(matrix, 2, 2)) - (matrix_get_element(matrix, 2, 1)*matrix_get_element(matrix, 1, 2))))
                       - (matrix_get_element(matrix, 0, 1)*((matrix_get_element(matrix, 1, 0) * matrix_get_element(matrix, 2, 2)) - (matrix_get_element(matrix, 2, 0)*matrix_get_element(matrix, 1, 2)))) 
                       + (matrix_get_element(matrix, 0, 2)*((matrix_get_element(matrix, 1, 0) * matrix_get_element(matrix, 2, 1)) - (matrix_get_element(matrix, 2, 0)*matrix_get_element(matrix, 1, 1)))); 
        return determinant;
    }
    else if(matrix->m == 2)
    {
        determinant = (matrix_get_element(matrix, 0, 0) * matrix_get_element(matrix, 1, 1)) 
                    - (matrix_get_element(matrix, 0, 1) * matrix_get_element(matrix, 1, 0));
        return determinant;
    }
    else if(matrix->m == 1)
    {
        return matrix_get_element(matrix, 0, 0);
    }
    else
    {
        inner_matrix = matrix_alloc(matrix->m-1, matrix->n-1);

        for(int i = 0; i < matrix->n; i++)
        {
            row_index = 0;
            for(int j = 0; j < matrix->m; j++)
            {
                col_index = 0;
                if(j == 0)
                {
                    continue;
                }
                for(int k = 0; k < matrix->n; k++)
                {
                    if(k == i)
                    {
                        continue;
                    }
                    matrix_add_element(&inner_matrix, row_index, col_index, matrix_get_element(matrix, j, k));
                    col_index++;
                }
                row_index++;
            }
            sign *= -1;
            intermediate = matrix_get_element(matrix, 0, i) * matrix_determinant(&inner_matrix);
            determinant += sign * intermediate;
            matrix_free(&inner_matrix);
        }

        return determinant;
    }
}

// Create

matrix_t matrix_create_unit_matrix(uint32_t size)
{
    ASSERT(size > 0);

    matrix_t matrix;

    matrix = matrix_alloc(size, size);

    for(int i = 0; i < size; i++)
    {
        for(int j = 0; j < size; j++)
        {
            if(i == j)
            {
                matrix_add_element(&matrix, i, j, 1);
            }
            else
            {
                matrix_add_element(&matrix, i, j, 0);
            }
        }
    }

    return matrix;
}

matrix_t matrix_create_zero_matrix(uint32_t m, uint32_t n)
{
    ASSERT(m > 0);
    ASSERT(n > 0);

    matrix_t matrix;

    matrix = matrix_alloc(m, n);

    for(int i = 0; i < m; i++)
    {
        for(int j = 0; j < n; j++)
        {
            matrix_add_element(&matrix, i, j, 0.0);
        }
    }

    return matrix;
}

// Arithmetic

bool matrix_is_equal(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    
    if(a->m != b->m || a->n != b->n)
    {
        return false;
    }
    else
    {
        for(int i = 0; i < a->m; i++)
        {
            for(int j = 0; j < b->n; j++)
            {
                if(matrix_get_element(a, i, j) != matrix_get_element(b, i, j))
                {
                    return false;
                }
            }
        }

        return true;
    }
}

bool matrix_is_square(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(matrix->m == matrix->n)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool matrix_is_zero(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    for(int i = 0; i < matrix->m; i++)
    {
        for(int j = 0; j < matrix->n; j++)
        {
            if(matrix_get_element(matrix, i, j) != 0)
            {
                return false;
            }
        }
    }
    return true;
}

bool matrix_is_unit(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT(matrix_is_square(matrix));

    for(int i = 0; i < matrix->m; i++)
    {
        for(int j = 0; j < matrix->n; j++)
        {
            if(i == j)
            {
                if(matrix_get_element(matrix, i, j) != 1)
                {
                    return false;
                }
            }
            else
            {
                if(matrix_get_element(matrix, i, j) != 0)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool matrix_is_multipliable(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    matrix_t order_a;
    matrix_t order_b;
    bool status = false;

    order_a = matrix_get_order(a);
    order_b = matrix_get_order(b);

    if(matrix_get_element(&order_a, 0, 1) == matrix_get_element(&order_b, 0, 0))
    {
        status = true;
    }
    
    matrix_free(&order_a);
    matrix_free(&order_b);

    return status;
}

matrix_t matrix_add(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    matrix_t order_a;
    matrix_t order_b;
    matrix_t sum;
    float elem_sum;

    order_a = matrix_get_order(a);
    order_b = matrix_get_order(b);

    ASSERT(matrix_is_equal(&order_a, &order_b));
    matrix_free(&order_a);
    matrix_free(&order_b);

    sum = matrix_alloc(a->m, a->n);

    for(int i = 0; i < a->m; i++)
    {
        for(int j = 0; j < a->n; j++)
        {
            elem_sum = matrix_get_element(a, i, j) + matrix_get_element(b, i, j);
            matrix_add_element(&sum, i, j, elem_sum);
        }
    }

    return sum;
}

matrix_t matrix_multiply_scalar(matrix_t* matrix, float scalar)
{
    ASSERT(matrix != NULL);

    matrix_t product;
    float elem_product;

    product = matrix_alloc(matrix->m, matrix->n);

    for(int i = 0; i < matrix->m; i++)
    {
        for(int j = 0; j < matrix->n; j++)
        {
            elem_product = matrix_get_element(matrix, i, j) * scalar;
            matrix_add_element(&product, i, j, elem_product);
        }
    }

    return product;
}

matrix_t matrix_multiply(matrix_t* a, matrix_t* b)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(matrix_is_multipliable(a, b));

    matrix_t product;
    float elem_product = 0;

    product = matrix_alloc(a->m, b->n);

    for(int i = 0; i < a->m; i++)
    {
        for(int j = 0; j < b->n; j++)
        {
            elem_product = 0;
            for(int k = 0; k < a->n; k++)
            {
                elem_product += matrix_get_element(a, i, k)*matrix_get_element(b, k, j);
            }
            matrix_add_element(&product, i, j, elem_product);
        }
    }

    return product;
}

matrix_t matrix_transpose(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    matrix_t transpose;

    transpose = matrix_alloc(matrix->n, matrix->m);

    for(int i = 0; i < matrix->m; i++)
    {
        for(int j = 0; j < matrix->n; j++)
        {
            matrix_add_element(&transpose, j, i, matrix_get_element(matrix, i, j));
        }
    }

    return transpose;
}

matrix_t matrix_inverse(matrix_t* matrix) 
{
    ASSERT(matrix_is_square(matrix));
    
    uint32_t n = matrix->m;
    matrix_t augmented = matrix_create_zero_matrix(n, 2 * n);
    matrix_t inverse = matrix_create_zero_matrix(n, n);

    for (uint32_t i = 0; i < n; i++) 
    {
        for (uint32_t j = 0; j < n; j++) 
        {
            matrix_add_element(&augmented, i, j, matrix_get_element(matrix, i, j));
            matrix_add_element(&augmented, i, j + n, (i == j) ? 1.0f : 0.0f);
        }
    }
    
    // Perform Gaussian-Jordan elimination
    for (uint32_t i = 0; i < n; i++) 
    {
        float pivot = matrix_get_element(&augmented, i, i);
        if (pivot == 0) // Matrix is singular
        {
            matrix_free(&augmented);
            matrix_free(&inverse);
            matrix_t zero = matrix_create_zero_matrix(matrix->m, matrix->n);
            return zero;
        }
        
        // Normalize the pivot row
        for (uint32_t j = 0; j < 2 * n; j++) 
        {
            matrix_add_element(&augmented, i, j, matrix_get_element(&augmented, i, j) / pivot);
        }
        
        // Eliminate other rows
        for (uint32_t k = 0; k < n; k++) 
        {
            if (k != i) 
            {
                float factor = matrix_get_element(&augmented, k, i);
                for (uint32_t j = 0; j < 2 * n; j++) 
                {
                    matrix_add_element(&augmented, k, j, matrix_get_element(&augmented, k, j) - factor * matrix_get_element(&augmented, i, j));
                }
            }
        }
    }
    
    // Extract the inverse matrix from the augmented matrix
    for (uint32_t i = 0; i < n; i++) 
    {
        for (uint32_t j = 0; j < n; j++) 
        {
            matrix_add_element(&inverse, i, j, matrix_get_element(&augmented, i, j + n));
        }
    }
    
    matrix_free(&augmented);
    return inverse;
}

void matrix_copy(matrix_t* src, matrix_t* dest)
{
    ASSERT(src != NULL);
    ASSERT(dest != NULL);
    ASSERT(src->m == dest->m);
    ASSERT(src->n == dest->n);
    for(int i = 0; i < src->m; i++)
    {
        for(int j = 0; j < src->n; j++)
        {
            matrix_add_element(dest, i, j, matrix_get_element(src, i, j));
        }
    }
    
}

void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...))
{
    ASSERT(matrix != NULL);

    int (*print_func)(const char*, ...);

    if(func != NULL)
    {
        print_func = func;
    }
    else
    {
        print_func = printf;
    }

    for(int i = 0; i < matrix->m; i++)
    {
        for(int j = 0; j < matrix->n; j++)
        {
            print_func("%f\t", matrix_get_element(matrix, i, j));
        }
        print_func("\n");
    }
}

void matrix_free(matrix_t* matrix)
{
    if(matrix->dynamic_alloc)
    {
        free(matrix->elem);
        matrix->dynamic_alloc = false;
    }
}