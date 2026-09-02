#include <perf/conformation/support.h>
#include <perf/conformation/matrix/support.h>
#include <math.h>
#include <stdlib.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>

void support_fill_random_matrix_single(matrix_t *mat, gsl_matrix_float *gsl_mat, int rows, int cols, float min, float max) 
{
    for (int i = 0; i < rows; i++) 
    {
        for (int j = 0; j < cols; j++) 
        {
            float value = min + ((float)rand() / RAND_MAX) * (max - min);
            matrix_add_element(mat, i, j, value);
            gsl_matrix_float_set(gsl_mat, i, j, value);
        }
    }
}

void support_fill_random_matrix_double(matrix_t *mat, gsl_matrix *gsl_mat, int rows, int cols, float min, float max) 
{
    for (int i = 0; i < rows; i++) 
    {
        for (int j = 0; j < cols; j++) 
        {
            float value = min + ((float)rand() / RAND_MAX) * (max - min);
            matrix_add_element(mat, i, j, value);
            gsl_matrix_set(gsl_mat, i, j, value);
        }
    }
}

bool support_matrix_addition_check(int rows, int cols, float min, float max)
{
    matrix_t A;
    matrix_t B;
    matrix_t sum;
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(rows, cols);
    gsl_matrix_float *gsl_B = gsl_matrix_float_alloc(rows, cols);

    A = matrix_alloc(rows, cols);
    B = matrix_alloc(rows, cols);

    support_fill_random_matrix_single(&A, gsl_A, rows, cols, min, max);
    support_fill_random_matrix_single(&B, gsl_B, rows, cols, min, max);
    
    sum = matrix_add(&A, &B);
    gsl_matrix_float_add(gsl_A, gsl_B);

    bool flag = true;
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
        {
            real_t expected = (real_t)gsl_matrix_float_get(gsl_A, i, j);
            real_t computed = matrix_get_element(&sum, i, j);
            if(!CONFORMATION_IS_NEAR(expected, computed))
            {
                flag = false;
                break;
            }
        }
        if(!flag)
        {
            break;
        }
    }

    matrix_free(&sum);
    matrix_free(&B);
    matrix_free(&A);
    gsl_matrix_float_free(gsl_A);
    gsl_matrix_float_free(gsl_B);

    return flag;
}

bool support_matrix_scalar_multiplication_check(int rows, int cols, float min, float max)
{
    matrix_t A;
    matrix_t scaled;
    float scalar;

    A = matrix_alloc(rows, cols);
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(rows, cols);

    scalar = min + ((float)rand() / RAND_MAX) * (max - min);
    support_fill_random_matrix_single(&A, gsl_A, rows, cols, min, max);

    scaled = matrix_multiply_scalar(&A, scalar);
    gsl_matrix_float_scale(gsl_A, scalar);

    bool flag = true;
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
        {
            real_t expected = (real_t)gsl_matrix_float_get(gsl_A, i, j);
            real_t computed = matrix_get_element(&scaled, i, j);
            if(!CONFORMATION_IS_NEAR(expected, computed))
            {
                flag = false;
                break;
            }
        }
        if(!flag)
        {
            break;
        }
    }

    matrix_free(&A);
    matrix_free(&scaled);
    gsl_matrix_float_free(gsl_A);

    return flag;
}

bool support_matrix_multiplication_check(int rows_a, int cols_a, int rows_b, int cols_b, float min, float max)
{
    matrix_t A;
    matrix_t B;
    matrix_t product;

    A = matrix_alloc(rows_a, cols_a);
    B = matrix_alloc(rows_b, cols_b);
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(rows_a, cols_a);
    gsl_matrix_float *gsl_B = gsl_matrix_float_alloc(cols_a, cols_b);
    gsl_matrix_float *gsl_product = gsl_matrix_float_alloc(rows_a, cols_b);

    support_fill_random_matrix_single(&A, gsl_A, rows_a, cols_a, min, max);
    support_fill_random_matrix_single(&B, gsl_B, rows_b, cols_b, min, max);

    product = matrix_multiply(&A, &B);
    gsl_blas_sgemm(CblasNoTrans, CblasNoTrans, 1.0, gsl_A, gsl_B, 0.0, gsl_product);

    bool flag = true;
    for(int i = 0; i < rows_a; i++)
    {
        for(int j = 0; j < cols_b; j++)
        {
            real_t expected = (real_t)gsl_matrix_float_get(gsl_product, i, j);
            real_t computed = matrix_get_element(&product, i, j);
            if(!CONFORMATION_IS_NEAR(expected, computed))
            {
                flag = false;
                break;
            }
        }
        if(!flag)
        {
            break;
        }
    }

    matrix_free(&product);
    matrix_free(&B);
    matrix_free(&A);
    gsl_matrix_float_free(gsl_A);
    gsl_matrix_float_free(gsl_B);
    gsl_matrix_float_free(gsl_product);

    return flag;
}

bool support_matrix_transpose_check(int rows, int cols, float min, float max)
{
    matrix_t A;
    matrix_t transpose;

    A = matrix_alloc(rows, cols);
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(rows, cols);
    gsl_matrix_float *gsl_transposed = gsl_matrix_float_alloc(cols, rows);

    support_fill_random_matrix_single(&A, gsl_A, rows, cols, min, max);

    transpose = matrix_transpose(&A);
    
    for (int i = 0; i < rows; i++) 
    {
        for (int j = 0; j < cols; j++) 
        {
            gsl_matrix_float_set(gsl_transposed, j, i, gsl_matrix_float_get(gsl_A, i, j));
        }
    }
    
    bool flag = true;
    for(int i = 0; i < cols; i++)
    {
        for(int j = 0; j < rows; j++)
        {
            real_t expected = (real_t)gsl_matrix_float_get(gsl_transposed, i, j);
            real_t computed = matrix_get_element(&transpose, i, j);
            if(!CONFORMATION_IS_NEAR(expected, computed))
            {
                flag = false;
                break;
            }
        }
        if(!flag)
        {
            break;
        }
    }

    matrix_free(&transpose);
    matrix_free(&A);
    gsl_matrix_float_free(gsl_A);
    gsl_matrix_float_free(gsl_transposed);
    
    return flag;
}

bool support_matrix_inverse_check(int size, float min, float max)
{
    matrix_t A;
    matrix_t inverse;

    A = matrix_alloc(size, size);
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(size, size);
    gsl_matrix_float *gsl_inverse = gsl_matrix_float_alloc(size, size);
    gsl_permutation *perm = gsl_permutation_alloc(size);
    int signum;

    support_fill_random_matrix_single(&A, gsl_A, size, size, min, max);
    
    inverse = matrix_inverse(&A);

    if(matrix_is_zero(&inverse))
    {
        matrix_free(&A);
        matrix_free(&inverse);
        gsl_matrix_float_free(gsl_A);
        gsl_matrix_float_free(gsl_inverse);
        gsl_permutation_free(perm);
        return true;
    }

    gsl_matrix *double_A = gsl_matrix_alloc(size, size);
    gsl_matrix *double_inverse = gsl_matrix_alloc(size, size);

    for (int i = 0; i < size; i++) 
    {
        for (int j = 0; j < size; j++) 
        {
            gsl_matrix_set(double_A, i, j, (double)gsl_matrix_float_get(gsl_A, i, j));
        }
    }
    
    gsl_linalg_LU_decomp(double_A, perm, &signum);
    gsl_linalg_LU_invert(double_A, perm, double_inverse);

    for (int i = 0; i < size; i++) 
    {
        for (int j = 0; j < size; j++) 
        {
            gsl_matrix_float_set(gsl_inverse, i, j, (float)gsl_matrix_get(double_inverse, i, j));
        }
    }

    bool flag = true;
    for(int i = 0; i < size; i++)
    {
        for(int j = 0; j < size; j++)
        {
            real_t expected = (real_t)gsl_matrix_float_get(gsl_inverse, i, j);
            real_t computed = matrix_get_element(&inverse, i, j);
            if(!CONFORMATION_IS_NEAR(expected, computed))
            {
                flag = false;
                break;
            }
        }
        if(!flag)
        {
            break;
        }
    }
    
    matrix_free(&inverse);
    matrix_free(&A);
    gsl_matrix_float_free(gsl_A);
    gsl_matrix_float_free(gsl_inverse);
    gsl_matrix_free(double_A);
    gsl_matrix_free(double_inverse);
    gsl_permutation_free(perm);

    return flag;
}

bool support_matrix_determinant_check(int size, float min, float max)
{
    matrix_t A;

    A = matrix_alloc(size, size);
    gsl_matrix_float *gsl_A = gsl_matrix_float_alloc(size, size);
    gsl_matrix *gsl_A_double = gsl_matrix_alloc(size, size);
    gsl_permutation *perm = gsl_permutation_alloc(size);
    int signum;

    support_fill_random_matrix_single(&A, gsl_A, size, size, min, max);

    for (int i = 0; i < size; i++) 
    {
        for (int j = 0; j < size; j++) 
        {
            gsl_matrix_set(gsl_A_double, i, j, gsl_matrix_float_get(gsl_A, i, j));
        }
    }

    real_t my_det = matrix_determinant(&A);

    gsl_matrix *LU = gsl_matrix_alloc(size, size);
    
    for (int i = 0; i < size; i++) 
    {
        for (int j = 0; j < size; j++) 
        {
            gsl_matrix_set(LU, i, j, gsl_matrix_get(gsl_A_double, i, j));
        }
    }

    gsl_linalg_LU_decomp(LU, perm, &signum);
    real_t gsl_det = (real_t)gsl_linalg_LU_det(LU, signum);

    // A DETERMINANT IS WEIGHED AGAINST HOW LARGE IT IS, NOT AGAINST ONE.
    //
    // This asked for the two to stand within 1.0 of each other, whatever the
    // determinant was. A determinant of order one would then pass however
    // wrong it was, and this check could not fail for the small matrices it is
    // given.
    //
    // A determinant is a sum of products, thus it is large when its elements
    // are, and the digits thrown away when those products cancel do not become
    // small merely because the sum did. The room allowed follows the larger of
    // the two readings.
    bool flag = CONFORMATION_IS_NEAR(my_det, gsl_det);
    
    matrix_free(&A);
    gsl_matrix_float_free(gsl_A);
    gsl_matrix_free(gsl_A_double);
    gsl_matrix_free(LU);
    gsl_permutation_free(perm);

    return flag;
}