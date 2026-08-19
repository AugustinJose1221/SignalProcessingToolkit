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

bool iir_is_valid_cutoff(float cutoff)
{
    return (cutoff >= IIR_MIN_CUTOFF) && (cutoff < 0.5f);
}

bool iir_design_low_pass(iir_t* iir, float cutoff)
{
    ASSERT(iir != NULL);

    // The check stands here and not in an assertion. An assertion goes away in
    // a release build, and a cutoff that is too low does not announce itself:
    // the filter answers, and its answer is wrong. Thus the module holds the
    // value itself.
    if(!iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

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

    return true;
}

bool iir_design_high_pass(iir_t* iir, float cutoff)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

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

    return true;
}

// The two numbers that every second order design at one frequency needs.
//
// The first is the cosine of the turn that the frequency makes in one sample.
// The second sets how wide the design reaches around that frequency: it falls
// as the quality rises, thus a high quality gives a narrow design.
static void iir_resonance(float centre, float quality, float* cosine, float* alpha)
{
    float turn = 2.0f * IIR_PI * centre;

    *cosine = cosf(turn);
    *alpha = sinf(turn) / (2.0f * quality);
}

// The middle of a band, and how narrow that band is.
//
// The middle is the GEOMETRIC mean of the two edges and not the plain mean.
// A filter of this kind is symmetric about its middle in the ratio of the
// frequencies and not in their difference: a band from 100 to 400 has its
// middle at 200, because 200 is twice 100 and 400 is twice 200. The plain mean
// would put it at 250 and the two edges would then not fall away equally.
static void iir_band_to_resonance(float low_cutoff, float high_cutoff,
                                  float* centre, float* quality)
{
    *centre = sqrtf(low_cutoff * high_cutoff);
    *quality = *centre / (high_cutoff - low_cutoff);
}

bool iir_design_band_pass(iir_t* iir, float low_cutoff, float high_cutoff)
{
    ASSERT(iir != NULL);

    if((iir->sections % 2u) != 0u)
    {
        return false;
    }
    if(!iir_is_valid_cutoff(low_cutoff) || !iir_is_valid_cutoff(high_cutoff))
    {
        return false;
    }
    if(high_cutoff <= low_cutoff)
    {
        return false;
    }

    // Half of the sections make the high pass at the low edge, and half make
    // the low pass at the high edge. Each half is designed on its own and then
    // copied into its place, thus the design of each edge stays the one that
    // is already tested.
    uint32_t half = iir->sections / 2u;

    iir_t low_part = iir_alloc(half);
    iir_t high_part = iir_alloc(half);

    bool built = iir_design_high_pass(&high_part, low_cutoff)
                 && iir_design_low_pass(&low_part, high_cutoff);

    if(built)
    {
        for(uint32_t section = 0; section < half; section++)
        {
            const float* from = &high_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, section, from[0], from[1], from[2],
                            1.0f, from[3], from[4]);
        }
        for(uint32_t section = 0; section < half; section++)
        {
            const float* from = &low_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, half + section, from[0], from[1], from[2],
                            1.0f, from[3], from[4]);
        }
    }

    iir_free(&low_part);
    iir_free(&high_part);

    return built;
}

bool iir_design_notch(iir_t* iir, float centre, float quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= 0.0f))
    {
        return false;
    }

    float cosine;
    float alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The zeros stand exactly on the circle at the frequency, thus the gain
    // there is nothing. The poles stand just inside the circle at the same
    // frequency, thus everywhere else the two nearly cancel and the gain stays
    // at one.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        1.0f, -2.0f * cosine, 1.0f,
                        1.0f + alpha, -2.0f * cosine, 1.0f - alpha);
    }

    return true;
}

bool iir_design_peak(iir_t* iir, float centre, float quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= 0.0f))
    {
        return false;
    }

    float cosine;
    float alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The same poles as the notch, and zeros at nothing and at half the sample
    // rate instead of on the frequency. Thus the gain is 1 at the frequency
    // and falls away on both sides of it.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        alpha, 0.0f, -alpha,
                        1.0f + alpha, -2.0f * cosine, 1.0f - alpha);
    }

    return true;
}

bool iir_design_band_stop(iir_t* iir, float low_cutoff, float high_cutoff)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(low_cutoff) || !iir_is_valid_cutoff(high_cutoff))
    {
        return false;
    }
    if(high_cutoff <= low_cutoff)
    {
        return false;
    }

    float centre;
    float quality;
    iir_band_to_resonance(low_cutoff, high_cutoff, &centre, &quality);

    return iir_design_notch(iir, centre, quality);
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
