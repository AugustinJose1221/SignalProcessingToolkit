#include <perf/conformation/vector/vector.h>
#include <perf/conformation/vector/support.h>

static void run_vector_dot_product_static_conformation(void)
{
    vector_t A0, B0;
    float dot_product0;
    VECTOR_INIT(A0, 3, ((float[]){1, 2, 3}));
    VECTOR_INIT(B0, 3, ((float[]){4, 5, 6}));
    dot_product0 = vector_dot_product(&A0, &B0);
    VALUE_CHECK_EQUAL_CASE(dot_product0, 32.0f, "Vector Static Dot Product Test");
    VECTOR_FREE(A0);
    VECTOR_FREE(B0);

    vector_t A1, B1;
    float dot_product1;
    VECTOR_INIT(A1, 4, ((float[]){-1.5f, 2.5f, -3.5f, 4.5f}));
    VECTOR_INIT(B1, 4, ((float[]){-4.5f, 3.5f, -2.5f, 1.5f}));
    dot_product1 = vector_dot_product(&A1, &B1);
    VALUE_CHECK_EQUAL_CASE(dot_product1, 31.0f, "Vector Static Dot Product Test");
    VECTOR_FREE(A1);
    VECTOR_FREE(B1);

    vector_t A2, B2;
    float dot_product2;
    VECTOR_INIT(A2, 2, ((float[]){7.5f, -8.5f}));
    VECTOR_INIT(B2, 2, ((float[]){-9.5f, 10.5f}));
    dot_product2 = vector_dot_product(&A2, &B2);
    VALUE_CHECK_EQUAL_CASE(dot_product2, -160.5f, "Vector Static Dot Product Test");
}

static void run_vector_dot_product_dynamic_conformation(void)
{
    support_init();
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(2, -10.0, 10.0), "Vector Dynamic Dot Product Test");
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(3, -1.0, 100.0), "Vector Dynamic Dot Product Test");
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(4, -5.0, 5.0), "Vector Dynamic Dot Product Test");
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(2, -15.0, 5.0), "Vector Dynamic Dot Product Test");
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(5, -1.0, 5.0), "Vector Dynamic Dot Product Test");
    FLAG_CHECK_TRUE_CASE(support_vector_dot_product_check(3, -4.0, 55.0), "Vector Dynamic Dot Product Test");
}

static void run_vector_norm_static_conformation(void)
{
    vector_t A0;
    float norm0;
    VECTOR_INIT(A0, 3, ((float[]){1, 2, 2}));
    norm0 = vector_norm(&A0);
    VALUE_CHECK_EQUAL_CASE(norm0, 3.0f, "Vector Static Norm Test");
    VECTOR_FREE(A0);

    vector_t A1;
    float norm1;
    VECTOR_INIT(A1, 4, ((float[]){-1.0f, 1.0f, -1.0f, 1.0f}));
    norm1 = vector_norm(&A1);
    VALUE_CHECK_EQUAL_CASE(norm1, 2.0f, "Vector Static Norm Test");
    VECTOR_FREE(A1);   
}

static void run_vector_norm_dynamic_conformation(void)
{
    support_init();
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(2, -10.0, 10.0), "Vector Dynamic Norm Test");
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(3, -1.0, 100.0), "Vector Dynamic Norm Test");
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(4, -5.0, 5.0), "Vector Dynamic Norm Test");
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(2, -15.0, 5.0), "Vector Dynamic Norm Test");
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(5, -1.0, 5.0), "Vector Dynamic Norm Test");
    FLAG_CHECK_TRUE_CASE(support_vector_norm_check(3, -4.0, 55.0), "Vector Dynamic Norm Test");
}

void run_vector_static_conformation_tests(void)
{
    run_vector_dot_product_static_conformation();
    run_vector_norm_static_conformation();
}

void run_vector_dynamic_conformation_tests(void)
{
    run_vector_dot_product_dynamic_conformation();
    run_vector_norm_dynamic_conformation();
}