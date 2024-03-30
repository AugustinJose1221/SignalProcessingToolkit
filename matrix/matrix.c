#include <matrix/matrix.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

matrix_t matrix_alloc(uint32_t m, uint32_t n)
{
    assert(m > 0);
    assert(n > 0);

    matrix_t matrix;

    matrix.m = m;
    matrix.n = n;
    matrix.elem = (float*)malloc(sizeof(float)*m*n);
    matrix.dynamic_alloc = true;

    return matrix;
}

matrix_t matrix_static_alloc(uint32_t m, uint32_t n, float* elem)
{
    assert(m > 0);
    assert(n > 0);
    assert(elem != NULL);

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
    assert(matrix != NULL);
    assert(i >= 0 && i < matrix->m);
    assert(j >= 0 && j < matrix->n);
    matrix->elem[(i*matrix->n)+j] = value;
}

float matrix_get_element(matrix_t* matrix, uint32_t i, uint32_t j)
{
    assert(matrix != NULL);
    assert(i >= 0 && i < matrix->m);
    assert(j >= 0 && j < matrix->n);

    return matrix->elem[(i*matrix->n)+j];
}

matrix_t matrix_get_nth_row(matrix_t* matrix, uint32_t row_index)
{
    assert(matrix != NULL);
    assert(row_index >= 0 && row_index < matrix->m);

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
    assert(matrix != NULL);
    assert(col_index >= 0 && col_index < matrix->n);

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
    assert(matrix != NULL);

    matrix_t order;

    order = matrix_alloc(1, 2);
    matrix_add_element(&order, 0, 0, matrix->m);
    matrix_add_element(&order, 0, 1, matrix->n);

    return order;
}

float matrix_trace(matrix_t* matrix)
{
    assert(matrix != NULL);
    assert(matrix_is_square(matrix));

    float trace = 0;

    for(int i = 0; i < matrix->m; i++)
    {
        trace += matrix_get_element(matrix, i, i);
    }

    return trace;
}

float matrix_determinant(matrix_t* matrix)
{
    assert(matrix != NULL);
    assert(matrix_is_square(matrix));

    float determinant = 0;
    float intermediate = 0;
    float sign = -1;
    int row_index;
    int col_index;
    matrix_t inner_matrix;

    if(matrix->m == 2)
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
    assert(size > 0);

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
    assert(m > 0);
    assert(n > 0);

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
    assert(a != NULL);
    assert(b != NULL);
    
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
    assert(matrix != NULL);

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
    assert(matrix != NULL);

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
    assert(matrix != NULL);
    assert(matrix_is_square(matrix));

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
    assert(a != NULL);
    assert(b != NULL);

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
    assert(a != NULL);
    assert(b != NULL);

    matrix_t order_a;
    matrix_t order_b;
    matrix_t sum;
    float elem_sum;

    order_a = matrix_get_order(a);
    order_b = matrix_get_order(b);

    assert(matrix_is_equal(&order_a, &order_b));
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
    assert(matrix != NULL);

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
    assert(a != NULL);
    assert(b != NULL);
    assert(matrix_is_multipliable(a, b));

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
    assert(matrix != NULL);

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

void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...))
{
    assert(matrix != NULL);

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