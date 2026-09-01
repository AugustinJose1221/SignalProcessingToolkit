#include <perf/conformation/support.h>
#include <perf/conformation/eigen/eigen.h>

#include <ffitt/linalg/matrix.h>

#include <math.h>
#include <stdlib.h>

#include <gsl/gsl_eigen.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>

// The eigenvalues of this library set against those of the GNU Scientific
// Library.
//
// NEITHER LIBRARY PROMISES AN ORDER. This one says the values come back with
// the vector of column k belonging to value k, and the other gives them in
// whatever order the rotations left them. Comparing them one by one as they
// stand would therefore fail on a difference of order and not of value, thus
// both sets are sorted before they are read.
//
// The matrix is made symmetric on purpose. eigen_solve is for a symmetric
// matrix, and the other library is asked for the symmetric road as well.

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static int compare_real(const void* left, const void* right)
{
    real_t a = *(const real_t*)left;
    real_t b = *(const real_t*)right;

    if(a < b)
    {
        return -1;
    }

    return (a > b) ? 1 : 0;
}

static bool eigenvalue_check(uint32_t size)
{
    matrix_t mine = matrix_alloc(size, size);
    gsl_matrix* theirs = gsl_matrix_alloc(size, size);
    real_t* my_values = (real_t*)malloc(sizeof(real_t) * size);
    gsl_vector* their_values = gsl_vector_alloc(size);
    gsl_eigen_symmv_workspace* work = gsl_eigen_symmv_alloc(size);
    gsl_matrix* their_vectors = gsl_matrix_alloc(size, size);
    real_t* sorted_theirs = (real_t*)malloc(sizeof(real_t) * size);
    bool flag = true;

    // A symmetric matrix, the same on both sides of the diagonal.
    for(uint32_t i = 0; i < size; i++)
    {
        for(uint32_t j = i; j < size; j++)
        {
            real_t value = random_between(REAL_C(-4.0), REAL_C(4.0));

            matrix_add_element(&mine, i, j, value);
            matrix_add_element(&mine, j, i, value);
            gsl_matrix_set(theirs, i, j, (double)value);
            gsl_matrix_set(theirs, j, i, (double)value);
        }
    }

    if(!eigen_solve(&mine, my_values, NULL))
    {
        flag = false;
    }
    else
    {
        gsl_eigen_symmv(theirs, their_values, their_vectors, work);

        for(uint32_t k = 0; k < size; k++)
        {
            sorted_theirs[k] = (real_t)gsl_vector_get(their_values, k);
        }

        qsort(my_values, size, sizeof(real_t), compare_real);
        qsort(sorted_theirs, size, sizeof(real_t), compare_real);

        for(uint32_t k = 0; k < size; k++)
        {
            if(!CONFORMATION_IS_NEAR(my_values[k], sorted_theirs[k]))
            {
                flag = false;
                break;
            }
        }
    }

    matrix_free(&mine);
    gsl_matrix_free(theirs);
    gsl_matrix_free(their_vectors);
    gsl_vector_free(their_values);
    gsl_eigen_symmv_free(work);
    free(my_values);
    free(sorted_theirs);

    return flag;
}

void run_eigen_conformation_tests(void)
{
    static const uint32_t sizes[] = {2u, 3u, 4u, 5u, 6u};

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(eigenvalue_check(sizes[k]),
                             "Eigen Values Test");
    }
}
