#include <perf/conformation/vector/support.h>
#include <math.h>
#include <gsl/gsl_vector_float.h>
#include <gsl/gsl_blas.h>

void support_fill_random_vector_single(vector_t *vec, gsl_vector_float *gsl_vec, int size, float min, float max)
{
    for (int i = 0; i < size; i++) 
    {
        float value = min + ((float)rand() / RAND_MAX) * (max - min);
        vector_add_point_at_index(vec, i, value);
        gsl_vector_float_set(gsl_vec, i, value);
    }
}

bool support_vector_dot_product_check(int size, float min, float max)
{
    vector_t A;
    vector_t B;
    float dotProduct;
    gsl_vector_float *gsl_A = gsl_vector_float_alloc(size);
    gsl_vector_float *gsl_B = gsl_vector_float_alloc(size);

    A = vector_alloc(size);
    B = vector_alloc(size);

    support_fill_random_vector_single(&A, gsl_A, size, min, max);
    support_fill_random_vector_single(&B, gsl_B, size, min, max);

    dotProduct = vector_dot_product(&A, &B);
    float gsl_dotProduct = 0.0f;
    gsl_blas_sdot(gsl_A, gsl_B, &gsl_dotProduct);

    bool flag = fabs(dotProduct - gsl_dotProduct) < 1e-3;

    vector_free(&A);
    vector_free(&B);
    gsl_vector_float_free(gsl_A);
    gsl_vector_float_free(gsl_B);

    return flag;
}

bool support_vector_norm_check(int size, float min, float max)
{
    vector_t A;
    float norm;
    float gsl_norm;
    gsl_vector_float *gsl_A = gsl_vector_float_alloc(size);

    A = vector_alloc(size);

    support_fill_random_vector_single(&A, gsl_A, size, min, max);

    norm = vector_norm(&A);
    
    gsl_norm = gsl_blas_snrm2(gsl_A);

    bool flag = fabs(norm - gsl_norm) < 1e-3;

    vector_free(&A);
    gsl_vector_float_free(gsl_A);

    return flag;
}
