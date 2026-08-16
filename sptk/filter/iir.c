#ifndef TEST
#include <sptk/filter/iir.h>
#include <sptk/core/defs.h>
#else
#include "iir.h"
#include "defs.h"
#endif

#include <math.h>

#define IIR_PI      3.14159265358979323846f

static void iir_set_pass_through(iir_t* iir);
static float iir_section_angle(uint32_t section, uint32_t sections);

iir_t iir_alloc(uint32_t sections)
{
    ASSERT(sections > 0);

    iir_t iir;

    iir.sections = sections;
    iir.coefficient = (float*)malloc(sizeof(float)*IIR_COEFFICIENT_SIZE(sections));
    iir.state = (float*)malloc(sizeof(float)*IIR_STATE_SIZE(sections));
    iir.dynamic_alloc = true;

    iir_set_pass_through(&iir);
    iir_reset(&iir);

    return iir;
}

iir_t iir_static_alloc(uint32_t sections, float* coefficient, float* state)
{
    ASSERT(sections > 0);
    ASSERT(coefficient != NULL);
    ASSERT(state != NULL);

    iir_t iir;

    iir.sections = sections;
    iir.coefficient = coefficient;
    iir.state = state;
    iir.dynamic_alloc = false;

    iir_set_pass_through(&iir);
    iir_reset(&iir);

    return iir;
}

void iir_design_low_pass(iir_t* iir, float cutoff)
{
    ASSERT(iir != NULL);
    ASSERT(cutoff > 0.0f && cutoff < 0.5f);

    // The bilinear transform bends the frequency, thus the design first bends
    // the cutoff the other way. Then the filter holds its cutoff at the place
    // that the caller asked for.
    float warped = tanf(IIR_PI * cutoff);
    float squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        // Each section of a filter of Butterworth holds a pair of poles at its
        // own angle on the circle.
        float damping = 2.0f * cosf(iir_section_angle(section, iir->sections));
        float divisor = 1.0f + (damping * warped) + squared;

        float b0 = squared / divisor;
        float b1 = 2.0f * b0;
        float b2 = b0;
        float a1 = (2.0f * (squared - 1.0f)) / divisor;
        float a2 = (1.0f - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, 1.0f, a1, a2);
    }
}

void iir_design_high_pass(iir_t* iir, float cutoff)
{
    ASSERT(iir != NULL);
    ASSERT(cutoff > 0.0f && cutoff < 0.5f);

    float warped = tanf(IIR_PI * cutoff);
    float squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        float damping = 2.0f * cosf(iir_section_angle(section, iir->sections));
        float divisor = 1.0f + (damping * warped) + squared;

        float b0 = 1.0f / divisor;
        float b1 = -2.0f * b0;
        float b2 = b0;
        float a1 = (2.0f * (squared - 1.0f)) / divisor;
        float a2 = (1.0f - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, 1.0f, a1, a2);
    }
}

void iir_set_section(iir_t* iir, uint32_t section, float b0, float b1, float b2,
                     float a0, float a1, float a2)
{
    ASSERT(iir != NULL);
    ASSERT(section < iir->sections);
    ASSERT(a0 != 0.0f);

    float* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

    coefficient[0] = b0 / a0;
    coefficient[1] = b1 / a0;
    coefficient[2] = b2 / a0;
    coefficient[3] = a1 / a0;
    coefficient[4] = a2 / a0;
}

float iir_process_sample(iir_t* iir, float sample)
{
    ASSERT(iir != NULL);

    float value = sample;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        float* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];
        float* state = &iir->state[section * IIR_STATE_COUNT];

        // Direct Form II transposed.
        float result = (coefficient[0] * value) + state[0];
        state[0] = (coefficient[1] * value) - (coefficient[3] * result) + state[1];
        state[1] = (coefficient[2] * value) - (coefficient[4] * result);

        value = result;
    }

    return value;
}

void iir_process_block(iir_t* iir, const float* input, float* output, uint32_t size)
{
    ASSERT(iir != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = iir_process_sample(iir, input[index]);
    }
}

void iir_reset(iir_t* iir)
{
    ASSERT(iir != NULL);

    for(uint32_t index = 0; index < IIR_STATE_SIZE(iir->sections); index++)
    {
        iir->state[index] = 0.0f;
    }
}

float iir_get_gain(iir_t* iir, float frequency)
{
    ASSERT(iir != NULL);

    float angle = 2.0f * IIR_PI * frequency;
    float total = 1.0f;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        float* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

        // The answer of one section is the value of its polynomial of the
        // input divided by the value of its polynomial of the feedback, both
        // at the point on the circle at this angle.
        float top_real = coefficient[0] + (coefficient[1] * cosf(-angle))
                         + (coefficient[2] * cosf(-2.0f*angle));
        float top_imaginary = (coefficient[1] * sinf(-angle))
                              + (coefficient[2] * sinf(-2.0f*angle));

        float bottom_real = 1.0f + (coefficient[3] * cosf(-angle))
                            + (coefficient[4] * cosf(-2.0f*angle));
        float bottom_imaginary = (coefficient[3] * sinf(-angle))
                                 + (coefficient[4] * sinf(-2.0f*angle));

        float top = sqrtf((top_real*top_real) + (top_imaginary*top_imaginary));
        float bottom = sqrtf((bottom_real*bottom_real)
                             + (bottom_imaginary*bottom_imaginary));

        if(bottom == 0.0f)
        {
            return 0.0f;
        }

        total *= top / bottom;
    }

    return total;
}

void iir_free(iir_t* iir)
{
    ASSERT(iir != NULL);

    if(iir->dynamic_alloc)
    {
        free(iir->coefficient);
        free(iir->state);
        iir->coefficient = NULL;
        iir->state = NULL;
        iir->dynamic_alloc = false;
    }
}

// Give the angle of the pair of poles of one section of a filter of
// Butterworth. The poles of such a filter lie at even distances on a half
// circle.
static float iir_section_angle(uint32_t section, uint32_t sections)
{
    float order = 2.0f * (float)sections;

    return (IIR_PI * (((2.0f * (float)section) + 1.0f) / (2.0f * order)));
}

// Give every section the coefficients that let the signal pass unchanged.
static void iir_set_pass_through(iir_t* iir)
{
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    }
}
