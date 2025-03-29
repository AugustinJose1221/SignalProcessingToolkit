#include <perf/conformation/support.h>

void run_matrix_addition_static_conformation(void)
{
    matrix_t A0, B0, sum0, correctSum0;
    MATRIX_INIT(A0, 2, 3, ((float[]){0, 1, 2, 1, 2, 3}));
    MATRIX_INIT(B0, 2, 3, ((float[]){0, 1, 2, 1, 2, 3}));
    MATRIX_INIT(correctSum0, 2, 3, ((float[]){0, 2, 4, 2, 4, 6}));
    sum0 = matrix_add(&A0, &B0);
    MATRIX_CHECK_EQUAL_CASE(sum0, correctSum0, "Matrix Test 1");
    MATRIX_FREE(sum0);
    MATRIX_FREE(correctSum0);
    MATRIX_FREE(B0);
    MATRIX_FREE(A0);

    matrix_t A1, B1, sum1, correctSum1;
    MATRIX_INIT(A1, 3, 3, ((float[]){1, 2, 3, 4, 5, 6, 7, 8, 9}));
    MATRIX_INIT(B1, 3, 3, ((float[]){9, 8, 7, 6, 5, 4, 3, 2, 1}));
    MATRIX_INIT(correctSum1, 3, 3, ((float[]){10, 10, 10, 10, 10, 10, 10, 10, 10}));
    sum1 = matrix_add(&A1, &B1);
    MATRIX_CHECK_EQUAL_CASE(sum1, correctSum1, "Matrix Test 2");
    MATRIX_FREE(sum1);
    MATRIX_FREE(correctSum1);
    MATRIX_FREE(B1);
    MATRIX_FREE(A1);

    matrix_t A2, B2, sum2, correctSum2;
    MATRIX_INIT(A2, 3, 2, ((float[]){1, 2, 3, 4, 5, 6}));
    MATRIX_INIT(B2, 3, 2, ((float[]){6, 5, 4, 3, 2, 1}));
    MATRIX_INIT(correctSum2, 3, 2, ((float[]){7, 7, 7, 7, 7, 7}));
    sum2 = matrix_add(&A2, &B2);
    MATRIX_CHECK_EQUAL_CASE(sum2, correctSum2, "Matrix Test 3");
    MATRIX_FREE(sum2);
    MATRIX_FREE(correctSum2);
    MATRIX_FREE(B2);
    MATRIX_FREE(A2);

    matrix_t A3, B3, sum3, correctSum3;
    MATRIX_INIT(A3, 2, 2, ((float[]){0, -1, -2, 3}));
    MATRIX_INIT(B3, 2, 2, ((float[]){4, -3, 2, -3}));
    MATRIX_INIT(correctSum3, 2, 2, ((float[]){4, -4, 0, 0}));
    sum3 = matrix_add(&A3, &B3);
    MATRIX_CHECK_EQUAL_CASE(sum3, correctSum3, "Matrix Test 4");
    MATRIX_FREE(sum3);
    MATRIX_FREE(correctSum3);
    MATRIX_FREE(B3);
    MATRIX_FREE(A3);

    matrix_t A4, B4, sum4, correctSum4;
    MATRIX_INIT(A4, 4, 4, ((float[]){1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}));
    MATRIX_INIT(B4, 4, 4, ((float[]){16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}));
    MATRIX_INIT(correctSum4, 4, 4, ((float[]){17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17}));
    sum4 = matrix_add(&A4, &B4);
    MATRIX_CHECK_EQUAL_CASE(sum4, correctSum4, "Matrix Test 5");
    MATRIX_FREE(sum4);
    MATRIX_FREE(correctSum4);
    MATRIX_FREE(B4);
    MATRIX_FREE(A4);
}

void run_matrix_multiply_scalar_conformation()
{
    matrix_t A0, scalar0, product0, correctProduct0;
    MATRIX_INIT(A0, 2, 3, ((float[]){1, 2, 3, 4, 5, 6}));
    scalar0 = matrix_multiply_scalar(&A0, 2);
    MATRIX_INIT(correctProduct0, 2, 3, ((float[]){2, 4, 6, 8, 10, 12}));
    MATRIX_CHECK_EQUAL_CASE(scalar0, correctProduct0, "Matrix Test 6");
    MATRIX_FREE(scalar0);
    MATRIX_FREE(correctProduct0);
    MATRIX_FREE(A0);

    matrix_t A1, scalar1, product1, correctProduct1;
    MATRIX_INIT(A1, 3, 2, ((float[]){1.5f, -2.5f, 3.5f, -4.5f, 5.5f, -6.5f}));
    scalar1 = matrix_multiply_scalar(&A1, -2);
    MATRIX_INIT(correctProduct1, 3, 2, ((float[]){-3.0f, 5.0f, -7.0f, 9.0f, -11.0f, 13.0f}));
    MATRIX_CHECK_EQUAL_CASE(scalar1, correctProduct1, "Matrix Test 7");
    MATRIX_FREE(scalar1);
    MATRIX_FREE(correctProduct1);
    MATRIX_FREE(A1);

    matrix_t A2, scalar2, product2, correctProduct2;
    MATRIX_INIT(A2, 4, 4, ((float[]){1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}));
    scalar2 = matrix_multiply_scalar(&A2, 0.5f);
    MATRIX_INIT(correctProduct2, 4, 4, ((float[]){0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 6.5f, 7.0f, 7.5f, 8.0f}));
    MATRIX_CHECK_EQUAL_CASE(scalar2, correctProduct2, "Matrix Test 8");
    MATRIX_FREE(scalar2);
    MATRIX_FREE(correctProduct2);
    MATRIX_FREE(A2);

    matrix_t A3, scalar3, product3, correctProduct3;
    MATRIX_INIT(A3, 2, 2, ((float[]){0, -1, -2, 3}));
    scalar3 = matrix_multiply_scalar(&A3, 3);
    MATRIX_INIT(correctProduct3, 2, 2, ((float[]){0, -3, -6, 9}));
    MATRIX_CHECK_EQUAL_CASE(scalar3, correctProduct3, "Matrix Test 9");
    MATRIX_FREE(scalar3);
    MATRIX_FREE(correctProduct3);
    MATRIX_FREE(A3);
}

void run_matrix_multiply_conformation()
{
    matrix_t A0, B0, product0, correctProduct0;
    MATRIX_INIT(A0, 2, 3, ((float[]){1, 2, 3, 4, 5, 6}));
    MATRIX_INIT(B0, 3, 2, ((float[]){7, 8, 9, 10, 11, 12}));
    MATRIX_INIT(correctProduct0, 2, 2, ((float[]){58, 64, 139, 154}));
    product0 = matrix_multiply(&A0, &B0);
    MATRIX_CHECK_EQUAL_CASE(product0, correctProduct0, "Matrix Test 10");
    MATRIX_FREE(product0);
    MATRIX_FREE(correctProduct0);
    MATRIX_FREE(B0);
    MATRIX_FREE(A0);

    matrix_t A1, B1, product1;
    MATRIX_INIT(A1, 3, 3, ((float[]){1.5f, -2.5f, 3.5f,
                                      -4.5f, 5.5f, -6.5f,
                                      -7.5f, -8.5f, -9.5f}));
    MATRIX_INIT(B1, 3, 3, ((float[]){-1.5f,-2.5f,-3.5f,
                                      -4.5f,-5.5f,-6.5f,
                                      -7.5f,-8.5f,-9.5f}));
    product1 = matrix_multiply(&A1,&B1);
    MATRIX_FREE(product1);
    MATRIX_FREE(B1);
    MATRIX_FREE(A1);
}

int main()
{
    run_matrix_addition_static_conformation();
    run_matrix_multiply_scalar_conformation();
    run_matrix_multiply_conformation();
    return 0;
}