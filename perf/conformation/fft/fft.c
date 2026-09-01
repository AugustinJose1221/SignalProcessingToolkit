#include <perf/conformation/support.h>
#include <perf/conformation/fft/fft.h>

#include <ffitt/linalg/cnum.h>

#include <math.h>
#include <stdlib.h>

#include <gsl/gsl_fft_complex.h>

// The transform of this library set against the transform of the GNU
// Scientific Library.
//
// THE TWO AGREE ON WHICH WAY IS FORWARD. Measured on a unit impulse at the
// second place of a transform of four, this library gives 1, -i, -1, +i, which
// is the sum with the negative exponent, and that is what the forward
// transform of the other library gives as well. Were the two to disagree the
// answers would be conjugates of each other and every test here would fail.
//
// The other library holds a complex number as two doubles side by side in one
// array, thus a transform of n points is an array of 2n values.

#define GSL_RE(data, index)     ((data)[2u * (index)])
#define GSL_IM(data, index)     ((data)[(2u * (index)) + 1u])

// A number between min and max, from the same stream of random numbers at
// every run, so that a failure can be looked at again.
static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

// Fill a signal for this library and the same signal for the other one.
static void fill_both(cnum_t* mine, double* theirs, uint32_t size)
{
    for(uint32_t index = 0; index < size; index++)
    {
        real_t re = random_between(REAL_C(-4.0), REAL_C(4.0));
        real_t im = random_between(REAL_C(-4.0), REAL_C(4.0));

        mine[index] = cnum_make(re, im);
        GSL_RE(theirs, index) = (double)re;
        GSL_IM(theirs, index) = (double)im;
    }
}

static bool complex_forward_check(uint32_t size)
{
    fft_t fft = fft_alloc(size);
    cnum_t* mine = (cnum_t*)malloc(sizeof(cnum_t) * size);
    double* theirs = (double*)malloc(sizeof(double) * 2u * size);
    bool flag = true;

    fill_both(mine, theirs, size);

    fft_forward(&fft, mine);
    gsl_fft_complex_radix2_forward(theirs, 1, size);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t re = cnum_real(mine[index]);
        real_t im = cnum_imaginary(mine[index]);

        if(!CONFORMATION_IS_NEAR(re, GSL_RE(theirs, index))
           || !CONFORMATION_IS_NEAR(im, GSL_IM(theirs, index)))
        {
            flag = false;
            break;
        }
    }

    free(mine);
    free(theirs);
    fft_free(&fft);

    return flag;
}

// The transform of a signal that holds no imaginary part. This library has a
// road of its own for that, which gives only the bins up to the middle,
// because the rest are the mirror of them. The other library is given the same
// signal as a complex one, and the bins that both give are compared.
static bool real_forward_check(uint32_t size)
{
    fft_t fft = fft_alloc(size);
    real_t* signal = (real_t*)malloc(sizeof(real_t) * size);
    cnum_t* mine = (cnum_t*)malloc(sizeof(cnum_t) * size);
    double* theirs = (double*)malloc(sizeof(double) * 2u * size);
    bool flag = true;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t value = random_between(REAL_C(-4.0), REAL_C(4.0));

        signal[index] = value;
        GSL_RE(theirs, index) = (double)value;
        GSL_IM(theirs, index) = 0.0;
    }

    fft_forward_real(&fft, signal, mine);
    gsl_fft_complex_radix2_forward(theirs, 1, size);

    for(uint32_t index = 0; index <= (size / 2u); index++)
    {
        real_t re = cnum_real(mine[index]);
        real_t im = cnum_imaginary(mine[index]);

        if(!CONFORMATION_IS_NEAR(re, GSL_RE(theirs, index))
           || !CONFORMATION_IS_NEAR(im, GSL_IM(theirs, index)))
        {
            flag = false;
            break;
        }
    }

    free(signal);
    free(mine);
    free(theirs);
    fft_free(&fft);

    return flag;
}

// Forward and then back must give the signal again. Both libraries are asked
// for it, so that a fault in the way back shows on whichever side it lies.
static bool inverse_check(uint32_t size)
{
    fft_t fft = fft_alloc(size);
    cnum_t* mine = (cnum_t*)malloc(sizeof(cnum_t) * size);
    cnum_t* first = (cnum_t*)malloc(sizeof(cnum_t) * size);
    double* theirs = (double*)malloc(sizeof(double) * 2u * size);
    bool flag = true;

    fill_both(mine, theirs, size);

    for(uint32_t index = 0; index < size; index++)
    {
        first[index] = mine[index];
    }

    fft_forward(&fft, mine);
    fft_inverse(&fft, mine);

    gsl_fft_complex_radix2_forward(theirs, 1, size);
    gsl_fft_complex_radix2_inverse(theirs, 1, size);

    for(uint32_t index = 0; index < size; index++)
    {
        if(!CONFORMATION_IS_NEAR(cnum_real(mine[index]),
                                 cnum_real(first[index]))
           || !CONFORMATION_IS_NEAR(cnum_imaginary(mine[index]),
                                    cnum_imaginary(first[index]))
           || !CONFORMATION_IS_NEAR(cnum_real(mine[index]),
                                    GSL_RE(theirs, index)))
        {
            flag = false;
            break;
        }
    }

    free(mine);
    free(first);
    free(theirs);
    fft_free(&fft);

    return flag;
}

void run_fft_conformation_tests(void)
{
    static const uint32_t sizes[] = {8u, 16u, 64u, 256u, 1024u};

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(complex_forward_check(sizes[k]),
                             "FFT Complex Forward Test");
    }

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(real_forward_check(sizes[k]),
                             "FFT Real Forward Test");
    }

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(inverse_check(sizes[k]),
                             "FFT Inverse Test");
    }
}
