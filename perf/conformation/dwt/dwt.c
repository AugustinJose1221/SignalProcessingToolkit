#include <perf/conformation/support.h>
#include <perf/conformation/dwt/dwt.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <gsl/gsl_wavelet.h>

// The wavelet transform of this library set against the transform of the GNU
// Scientific Library.
//
// BOTH SCALE THE SAME WAY. Measured on a signal that sits at 1, one level of
// the Haar transform of this library gives 1.41421, which is the root of two,
// thus both libraries hold the transform so that it keeps the energy of the
// signal. A library that took a plain mean would give 1.
//
// ONLY THE DETAIL OF THE FIRST LEVEL IS COMPARED, AND HERE IS WHY. The other
// library always takes the transform as far as it will go and offers no way to
// stop it after a set number of levels. This library takes the number of
// levels it is told. Comparing the whole of the two answers therefore compares
// results of different depths: measured on 16 samples, the finest details
// agreed exactly while the coarser values did not, because one library had
// gone two levels and the other four.
//
// The detail of the first level is the one part both libraries work out the
// same way and hold in the same place: the second half of what the other
// library gives. It is also the part that says whether the filter and the
// throwing away of every second sample are right, which is the whole of one
// level.

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static bool detail_check(dwt_wavelet_t kind, const gsl_wavelet_type* type,
                         size_t order, uint32_t size)
{
    real_t* signal = (real_t*)malloc(sizeof(real_t) * size);
    real_t* approximation = (real_t*)malloc(sizeof(real_t) * (size / 2u));
    real_t* detail = (real_t*)malloc(sizeof(real_t) * (size / 2u));
    double* theirs = (double*)malloc(sizeof(double) * size);
    bool flag = true;

    for(uint32_t k = 0; k < size; k++)
    {
        signal[k] = random_between(REAL_C(-4.0), REAL_C(4.0));
        theirs[k] = (double)signal[k];
    }

    dwt_t dwt = dwt_init(kind);
    dwt_forward(&dwt, signal, size, approximation, detail);

    gsl_wavelet* wavelet = gsl_wavelet_alloc(type, order);
    gsl_wavelet_workspace* work = gsl_wavelet_workspace_alloc(size);

    gsl_wavelet_transform_forward(wavelet, theirs, 1, size, work);

    for(uint32_t k = 0; k < (size / 2u); k++)
    {
        if(!CONFORMATION_IS_NEAR(detail[k], theirs[(size / 2u) + k]))
        {
            flag = false;
            break;
        }
    }

    gsl_wavelet_free(wavelet);
    gsl_wavelet_workspace_free(work);
    free(signal);
    free(approximation);
    free(detail);
    free(theirs);

    return flag;
}

void run_dwt_conformation_tests(void)
{
    static const uint32_t sizes[] = {8u, 16u, 64u, 256u};

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(detail_check(DWT_HAAR, gsl_wavelet_haar, 2,
                                          sizes[k]),
                             "Wavelet Haar Detail Test");
    }

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(detail_check(DWT_DAUBECHIES4,
                                          gsl_wavelet_daubechies, 4, sizes[k]),
                             "Wavelet Daubechies Detail Test");
    }
}
