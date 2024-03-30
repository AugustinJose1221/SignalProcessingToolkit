#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_MATRIX_EXAMPLE)

#include <matrix/matrix.h>
#include <stdio.h>

#define NUM_OF_ROWS 3
#define NUM_OF_COLS 3

int main()
{

    int count = 1;
    matrix_t matrix;

    printf("*************3x3 Matrix*************\n");
    matrix = matrix_alloc(NUM_OF_ROWS, NUM_OF_COLS);
    for(int i = 0; i < NUM_OF_ROWS; i++)
    {
        for(int j = 0; j < NUM_OF_COLS; j++)
        {
            matrix_add_element(&matrix, i, j, count);
            count++;
        }
    }

    matrix_printf(&matrix, NULL);
    matrix_free(&matrix);

    printf("\n*************Unit Matrix*************\n");
    matrix = matrix_create_unit_matrix(NUM_OF_ROWS);
    matrix_printf(&matrix, NULL);
    matrix_free(&matrix);

    printf("\n*************Zero Matrix*************\n");
    matrix = matrix_create_zero_matrix(NUM_OF_ROWS, NUM_OF_COLS);
    matrix_printf(&matrix, NULL);
    matrix_free(&matrix);

    printf("\n*************Row Matrix*************\n");
    matrix_t row_matrix;
    matrix = matrix_alloc(NUM_OF_ROWS, NUM_OF_COLS);
    count = 1;
    for(int i = 1; i <= NUM_OF_ROWS; i++)
    {
        for(int j = 1; j <= NUM_OF_COLS; j++)
        {
            matrix_add_element(&matrix, i-1, j-1, count);
            count++;
        }
    }
    row_matrix = matrix_get_nth_row(&matrix, 1);
    matrix_printf(&row_matrix, NULL);
    matrix_free(&matrix);
    matrix_free(&row_matrix);

    printf("\n*************Col Matrix*************\n");
    matrix_t col_matrix;
    matrix = matrix_alloc(NUM_OF_ROWS, NUM_OF_COLS);
    count = 1;
    for(int i = 0; i < NUM_OF_ROWS; i++)
    {
        for(int j = 0; j < NUM_OF_COLS; j++)
        {
            matrix_add_element(&matrix, i, j, count);
            count++;
        }
    }
    col_matrix = matrix_get_nth_col(&matrix, 1);
    matrix_printf(&col_matrix, NULL);
    
    matrix_t A;
    matrix_t B;
    matrix_t sum;
    matrix_t scalar_product;
    matrix_t product;
    matrix_t transpose;

    A = matrix_alloc(2, 3);
    B = matrix_alloc(2, 3);
    
    for(int i = 0; i < 2; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            matrix_add_element(&A, i, j, i+j);
            matrix_add_element(&B, i, j, i*j);
        }
    }
    printf("\n*************A*************\n");
    matrix_printf(&A, NULL);
    printf("\n*************B*************\n");
    matrix_printf(&B, NULL);

    sum = matrix_add(&A, &B);
    printf("\n*************Sum*************\n");
    matrix_printf(&sum, NULL);
    matrix_free(&sum);

    printf("\n*************Scalar Product*************\n");
    scalar_product = matrix_multiply_scalar(&A, 5);
    matrix_printf(&scalar_product, NULL);
    matrix_free(&scalar_product);
    matrix_free(&B);

    B = matrix_alloc(3, 4);
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            matrix_add_element(&B, i, j, i*j);
        }
    }

    printf("\n*************A*************\n");
    matrix_printf(&A, NULL);
    printf("\n*************B*************\n");
    matrix_printf(&B, NULL);
    printf("\n*************Product*************\n");
    product = matrix_multiply(&A, &B);
    matrix_printf(&product, NULL);

    transpose = matrix_transpose(&product);
    printf("\n*************Transpose*************\n");
    matrix_printf(&transpose, NULL);
    
    matrix_t det;
    int counter = 1;

    det = matrix_alloc(3, 3);
    matrix_add_element(&det, 0, 0, 1.2);
    matrix_add_element(&det, 0, 1, 2.7);
    matrix_add_element(&det, 0, 2, 1.46);
    matrix_add_element(&det, 1, 0, 2.19);
    matrix_add_element(&det, 1, 1, 3.33);
    matrix_add_element(&det, 1, 2, 1.05);
    matrix_add_element(&det, 2, 0, 4.12);
    matrix_add_element(&det, 2, 1, 2.5);
    matrix_add_element(&det, 2, 2, 2.89);
    printf("\n*************Determinant*************\n");
    matrix_printf(&det, NULL);
    printf("\nDeterminant: %f\n", matrix_determinant(&det));

    matrix_free(&A);
    matrix_free(&B);
    matrix_free(&transpose);
    matrix_free(&product);
    matrix_free(&matrix);
    matrix_free(&col_matrix);


    return 0;
}


#endif