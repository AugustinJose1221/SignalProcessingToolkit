#ifndef TEST
#include <sptk/filter/fir.h>
#include <sptk/core/defs.h>
#else
#include "fir.h"
#include "defs.h"
#endif

#include <math.h>

#define FIR_PI      3.14159265358979323846f

static float fir_sinc(float x);
static float fir_hamming(uint32_t index, uint32_t length);
static void fir_build_low_pass(float* coefficient, uint32_t length, float cutoff);

fir_t fir_alloc(uint32_t length)
{
    ASSERT(length > 0);

    fir_t fir;

    fir.length = length;
    fir.coefficient = (float*)malloc(sizeof(float)*length);
    fir.history = (float*)malloc(sizeof(float)*length);
    fir.position = 0;
    fir.dynamic_alloc = true;

    for(uint32_t index = 0; index < length; index++)
    {
        fir.coefficient[index] = 0.0f;
    }
    fir_reset(&fir);

    return fir;
}

fir_t fir_static_alloc(uint32_t length, float* coefficient, float* history)
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
        fir.coefficient[index] = 0.0f;
    }
    fir_reset(&fir);

    return fir;
}

bool fir_is_valid_cutoff(uint32_t length, float cutoff)
{
    if(length == 0u)
    {
        return false;
    }

    float turn = FIR_TRANSITION / (float)length;

    return (cutoff >= turn) && (cutoff <= (0.5f - turn));
}

bool fir_is_valid_band(uint32_t length, float low_cutoff, float high_cutoff)
{
    if(!fir_is_valid_cutoff(length, low_cutoff)
       || !fir_is_valid_cutoff(length, high_cutoff))
    {
        return false;
    }

    float turn = FIR_TRANSITION / (float)length;

    return (high_cutoff - low_cutoff) >= turn;
}

bool fir_design_low_pass(fir_t* fir, float cutoff)
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

bool fir_design_high_pass(fir_t* fir, float cutoff)
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
    fir->coefficient[middle] += 1.0f;

    return true;
}

bool fir_design_band_pass(fir_t* fir, float low_cutoff, float high_cutoff)
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
        float middle = ((float)fir->length - 1.0f) / 2.0f;
        float position = (float)index - middle;
        float lower = 2.0f * low_cutoff * fir_sinc(2.0f * low_cutoff * position);
        fir->coefficient[index] -= lower * fir_hamming(index, fir->length);
    }

    return true;
}

void fir_set_coefficient(fir_t* fir, uint32_t index, float value)
{
    ASSERT(fir != NULL);
    ASSERT(index < fir->length);

    fir->coefficient[index] = value;
}

float fir_get_coefficient(fir_t* fir, uint32_t index)
{
    ASSERT(fir != NULL);
    ASSERT(index < fir->length);

    return fir->coefficient[index];
}

float fir_process_sample(fir_t* fir, float sample)
{
    ASSERT(fir != NULL);

    fir->history[fir->position] = sample;

    float result = 0.0f;
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

void fir_process_block(fir_t* fir, const float* input, float* output, uint32_t size)
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
        fir->history[index] = 0.0f;
    }
    fir->position = 0;
}

float fir_get_gain(fir_t* fir, float frequency)
{
    ASSERT(fir != NULL);

    // The answer of the filter is the sum of each coefficient times the point
    // on the circle at the angle of that step.
    float real = 0.0f;
    float imaginary = 0.0f;

    for(uint32_t index = 0; index < fir->length; index++)
    {
        float angle = -2.0f * FIR_PI * frequency * (float)index;
        real += fir->coefficient[index] * cosf(angle);
        imaginary += fir->coefficient[index] * sinf(angle);
    }

    return sqrtf((real*real) + (imaginary*imaginary));
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
static float fir_sinc(float x)
{
    if((x < 0.000001f) && (x > -0.000001f))
    {
        return 1.0f;
    }

    return sinf(FIR_PI * x) / (FIR_PI * x);
}

// The window of Hamming. A window takes the ends of the coefficients down to
// almost zero. Without it the filter holds large waves in the band that it
// stops.
static float fir_hamming(uint32_t index, uint32_t length)
{
    if(length == 1)
    {
        return 1.0f;
    }

    return 0.54f - (0.46f * cosf((2.0f * FIR_PI * (float)index)
                                 / ((float)length - 1.0f)));
}

static void fir_build_low_pass(float* coefficient, uint32_t length, float cutoff)
{
    float middle = ((float)length - 1.0f) / 2.0f;

    for(uint32_t index = 0; index < length; index++)
    {
        float position = (float)index - middle;
        coefficient[index] = 2.0f * cutoff * fir_sinc(2.0f * cutoff * position)
                             * fir_hamming(index, length);
    }
}
