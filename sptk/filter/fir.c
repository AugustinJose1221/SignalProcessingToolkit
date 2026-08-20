#ifndef TEST
#include <sptk/filter/fir.h>
#include <sptk/core/defs.h>
#else
#include "fir.h"
#include "defs.h"
#endif

#include <math.h>

#define FIR_PI      REAL_C(3.14159265358979323846)

static real_t fir_sinc(real_t x);
static real_t fir_hamming(uint32_t index, uint32_t length);
static void fir_build_low_pass(real_t* coefficient, uint32_t length, real_t cutoff);

fir_t fir_alloc(uint32_t length)
{
    ASSERT(length > 0);

    fir_t fir;

    fir.length = length;
    fir.coefficient = (real_t*)malloc(sizeof(real_t)*length);
    fir.history = (real_t*)malloc(sizeof(real_t)*length);
    fir.position = 0;
    fir.dynamic_alloc = true;

    for(uint32_t index = 0; index < length; index++)
    {
        fir.coefficient[index] = REAL_C(0.0);
    }
    fir_reset(&fir);

    return fir;
}

fir_t fir_static_alloc(uint32_t length, real_t* coefficient, real_t* history)
{
    ASSERT(length > 0);
    ASSERT(coefficient != NULL);
    ASSERT(history != NULL);

    fir_t fir;

    fir.length = length;
    fir.coefficient = coefficient;
    fir.history = history;
    fir.position = 0;
    fir.dynamic_alloc = false;

    for(uint32_t index = 0; index < length; index++)
    {
        fir.coefficient[index] = REAL_C(0.0);
    }
    fir_reset(&fir);

    return fir;
}

bool fir_is_valid_cutoff(uint32_t length, real_t cutoff)
{
    if(length == 0u)
    {
        return false;
    }

    real_t turn = FIR_TRANSITION / (real_t)length;

    return (cutoff >= turn) && (cutoff <= (REAL_C(0.5) - turn));
}

bool fir_is_valid_band(uint32_t length, real_t low_cutoff, real_t high_cutoff)
{
    if(!fir_is_valid_cutoff(length, low_cutoff)
       || !fir_is_valid_cutoff(length, high_cutoff))
    {
        return false;
    }

    real_t turn = FIR_TRANSITION / (real_t)length;

    return (high_cutoff - low_cutoff) >= turn;
}

bool fir_design_low_pass(fir_t* fir, real_t cutoff)
{
    ASSERT(fir != NULL);

    // The check stands here and not in an assertion, because an assertion goes
    // away in a release build and a filter that is too short for its cutoff
    // does not announce itself: it answers, and its answer is wrong.
    if(!fir_is_valid_cutoff(fir->length, cutoff))
    {
        return false;
    }

    fir_build_low_pass(fir->coefficient, fir->length, cutoff);

    return true;
}

bool fir_design_high_pass(fir_t* fir, real_t cutoff)
{
    ASSERT(fir != NULL);
    // The change of the sign works with a middle coefficient only, thus the
    // length must be odd.
    ASSERT((fir->length % 2) == 1);

    if(!fir_is_valid_cutoff(fir->length, cutoff))
    {
        return false;
    }

    fir_build_low_pass(fir->coefficient, fir->length, cutoff);

    // A high pass filter is the whole band less the low pass filter. Change
    // the sign of every coefficient and add one in the middle.
    uint32_t middle = fir->length / 2;
    for(uint32_t index = 0; index < fir->length; index++)
    {
        fir->coefficient[index] = -fir->coefficient[index];
    }
    fir->coefficient[middle] += REAL_C(1.0);

    return true;
}

bool fir_design_band_pass(fir_t* fir, real_t low_cutoff, real_t high_cutoff)
{
    ASSERT(fir != NULL);

    if(!fir_is_valid_band(fir->length, low_cutoff, high_cutoff))
    {
        return false;
    }

    // A band pass filter is the wider low pass filter less the narrower one.
    fir_build_low_pass(fir->coefficient, fir->length, high_cutoff);

    for(uint32_t index = 0; index < fir->length; index++)
    {
        real_t middle = ((real_t)fir->length - REAL_C(1.0)) / REAL_C(2.0);
        real_t position = (real_t)index - middle;
        real_t lower = REAL_C(2.0) * low_cutoff * fir_sinc(REAL_C(2.0) * low_cutoff * position);
        fir->coefficient[index] -= lower * fir_hamming(index, fir->length);
    }

    return true;
}

void fir_set_coefficient(fir_t* fir, uint32_t index, real_t value)
{
    ASSERT(fir != NULL);
    ASSERT(index < fir->length);

    fir->coefficient[index] = value;
}

real_t fir_get_coefficient(fir_t* fir, uint32_t index)
{
    ASSERT(fir != NULL);
    ASSERT(index < fir->length);

    return fir->coefficient[index];
}

real_t fir_process_sample(fir_t* fir, real_t sample)
{
    ASSERT(fir != NULL);

    fir->history[fir->position] = sample;

    real_t result = REAL_C(0.0);
    uint32_t index = fir->position;

    // Walk back through the history, from the newest sample to the oldest one.
    for(uint32_t step = 0; step < fir->length; step++)
    {
        result += fir->coefficient[step] * fir->history[index];

        if(index == 0)
        {
            index = fir->length - 1;
        }
        else
        {
            index--;
        }
    }

    fir->position++;
    if(fir->position >= fir->length)
    {
        fir->position = 0;
    }

    return result;
}

void fir_process_block(fir_t* fir, const real_t* input, real_t* output, uint32_t size)
{
    ASSERT(fir != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = fir_process_sample(fir, input[index]);
    }
}

void fir_reset(fir_t* fir)
{
    ASSERT(fir != NULL);

    for(uint32_t index = 0; index < fir->length; index++)
    {
        fir->history[index] = REAL_C(0.0);
    }
    fir->position = 0;
}

real_t fir_get_gain(fir_t* fir, real_t frequency)
{
    ASSERT(fir != NULL);

    // The answer of the filter is the sum of each coefficient times the point
    // on the circle at the angle of that step.
    real_t real = REAL_C(0.0);
    real_t imaginary = REAL_C(0.0);

    for(uint32_t index = 0; index < fir->length; index++)
    {
        real_t angle = -REAL_C(2.0) * FIR_PI * frequency * (real_t)index;
        real += fir->coefficient[index] * REAL_COS(angle);
        imaginary += fir->coefficient[index] * REAL_SIN(angle);
    }

    return REAL_SQRT((real*real) + (imaginary*imaginary));
}

void fir_free(fir_t* fir)
{
    ASSERT(fir != NULL);

    if(fir->dynamic_alloc)
    {
        free(fir->coefficient);
        free(fir->history);
        fir->coefficient = NULL;
        fir->history = NULL;
        fir->dynamic_alloc = false;
    }
}

// Give sin(pi*x)/(pi*x), which is 1 at the point zero.
static real_t fir_sinc(real_t x)
{
    if((x < REAL_C(0.000001)) && (x > -REAL_C(0.000001)))
    {
        return REAL_C(1.0);
    }

    return REAL_SIN(FIR_PI * x) / (FIR_PI * x);
}

// The window of Hamming. A window takes the ends of the coefficients down to
// almost zero. Without it the filter holds large waves in the band that it
// stops.
static real_t fir_hamming(uint32_t index, uint32_t length)
{
    if(length == 1)
    {
        return REAL_C(1.0);
    }

    return REAL_C(0.54) - (REAL_C(0.46) * REAL_COS((REAL_C(2.0) * FIR_PI * (real_t)index)
                                 / ((real_t)length - REAL_C(1.0))));
}

static void fir_build_low_pass(real_t* coefficient, uint32_t length, real_t cutoff)
{
    real_t middle = ((real_t)length - REAL_C(1.0)) / REAL_C(2.0);

    for(uint32_t index = 0; index < length; index++)
    {
        real_t position = (real_t)index - middle;
        coefficient[index] = REAL_C(2.0) * cutoff * fir_sinc(REAL_C(2.0) * cutoff * position)
                             * fir_hamming(index, length);
    }
}
