#ifndef TEST
#include <sptk/filter/iir.h>
#include <sptk/core/defs.h>
#else
#include "iir.h"
#include "defs.h"
#endif

#include <math.h>

#define IIR_PI      REAL_C(3.14159265358979323846)

static void iir_set_pass_through(iir_t* iir);
static real_t iir_section_angle(uint32_t section, uint32_t sections);

iir_t iir_alloc(uint32_t sections)
{
    ASSERT(sections > 0);

    iir_t iir;

    iir.sections = sections;
    iir.coefficient = (real_t*)malloc(sizeof(real_t)*IIR_COEFFICIENT_SIZE(sections));
    iir.state = (real_t*)malloc(sizeof(real_t)*IIR_STATE_SIZE(sections));
    iir.dynamic_alloc = true;

    iir_set_pass_through(&iir);
    iir_reset(&iir);

    return iir;
}

iir_t iir_static_alloc(uint32_t sections, real_t* coefficient, real_t* state)
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

bool iir_is_valid_cutoff(real_t cutoff)
{
    return (cutoff >= IIR_MIN_CUTOFF) && (cutoff < REAL_C(0.5));
}

bool iir_design_low_pass(iir_t* iir, real_t cutoff)
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
    real_t warped = REAL_TAN(IIR_PI * cutoff);
    real_t squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        // Each section of a filter of Butterworth holds a pair of poles at its
        // own angle on the circle.
        real_t damping = REAL_C(2.0) * REAL_COS(iir_section_angle(section, iir->sections));
        real_t divisor = REAL_C(1.0) + (damping * warped) + squared;

        real_t b0 = squared / divisor;
        real_t b1 = REAL_C(2.0) * b0;
        real_t b2 = b0;
        real_t a1 = (REAL_C(2.0) * (squared - REAL_C(1.0))) / divisor;
        real_t a2 = (REAL_C(1.0) - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, REAL_C(1.0), a1, a2);
    }

    return true;
}

bool iir_design_high_pass(iir_t* iir, real_t cutoff)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

    real_t warped = REAL_TAN(IIR_PI * cutoff);
    real_t squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t damping = REAL_C(2.0) * REAL_COS(iir_section_angle(section, iir->sections));
        real_t divisor = REAL_C(1.0) + (damping * warped) + squared;

        real_t b0 = REAL_C(1.0) / divisor;
        real_t b1 = -REAL_C(2.0) * b0;
        real_t b2 = b0;
        real_t a1 = (REAL_C(2.0) * (squared - REAL_C(1.0))) / divisor;
        real_t a2 = (REAL_C(1.0) - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, REAL_C(1.0), a1, a2);
    }

    return true;
}

// The two numbers that every second order design at one frequency needs.
//
// The first is the cosine of the turn that the frequency makes in one sample.
// The second sets how wide the design reaches around that frequency: it falls
// as the quality rises, thus a high quality gives a narrow design.
static void iir_resonance(real_t centre, real_t quality, real_t* cosine, real_t* alpha)
{
    real_t turn = REAL_C(2.0) * IIR_PI * centre;

    *cosine = REAL_COS(turn);
    *alpha = REAL_SIN(turn) / (REAL_C(2.0) * quality);
}

// The middle of a band, and how narrow that band is.
//
// The middle is the GEOMETRIC mean of the two edges and not the plain mean.
// A filter of this kind is symmetric about its middle in the ratio of the
// frequencies and not in their difference: a band from 100 to 400 has its
// middle at 200, because 200 is twice 100 and 400 is twice 200. The plain mean
// would put it at 250 and the two edges would then not fall away equally.
static void iir_band_to_resonance(real_t low_cutoff, real_t high_cutoff,
                                  real_t* centre, real_t* quality)
{
    *centre = REAL_SQRT(low_cutoff * high_cutoff);
    *quality = *centre / (high_cutoff - low_cutoff);
}

bool iir_design_band_pass(iir_t* iir, real_t low_cutoff, real_t high_cutoff)
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
            const real_t* from = &high_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, section, from[0], from[1], from[2],
                            REAL_C(1.0), from[3], from[4]);
        }
        for(uint32_t section = 0; section < half; section++)
        {
            const real_t* from = &low_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, half + section, from[0], from[1], from[2],
                            REAL_C(1.0), from[3], from[4]);
        }
    }

    iir_free(&low_part);
    iir_free(&high_part);

    return built;
}

bool iir_design_notch(iir_t* iir, real_t centre, real_t quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= REAL_C(0.0)))
    {
        return false;
    }

    real_t cosine;
    real_t alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The zeros stand exactly on the circle at the frequency, thus the gain
    // there is nothing. The poles stand just inside the circle at the same
    // frequency, thus everywhere else the two nearly cancel and the gain stays
    // at one.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        REAL_C(1.0), -REAL_C(2.0) * cosine, REAL_C(1.0),
                        REAL_C(1.0) + alpha, -REAL_C(2.0) * cosine, REAL_C(1.0) - alpha);
    }

    return true;
}

bool iir_design_peak(iir_t* iir, real_t centre, real_t quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= REAL_C(0.0)))
    {
        return false;
    }

    real_t cosine;
    real_t alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The same poles as the notch, and zeros at nothing and at half the sample
    // rate instead of on the frequency. Thus the gain is 1 at the frequency
    // and falls away on both sides of it.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        alpha, REAL_C(0.0), -alpha,
                        REAL_C(1.0) + alpha, -REAL_C(2.0) * cosine, REAL_C(1.0) - alpha);
    }

    return true;
}

bool iir_design_band_stop(iir_t* iir, real_t low_cutoff, real_t high_cutoff)
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

    real_t centre;
    real_t quality;
    iir_band_to_resonance(low_cutoff, high_cutoff, &centre, &quality);

    return iir_design_notch(iir, centre, quality);
}

void iir_set_section(iir_t* iir, uint32_t section, real_t b0, real_t b1, real_t b2,
                     real_t a0, real_t a1, real_t a2)
{
    ASSERT(iir != NULL);
    ASSERT(section < iir->sections);
    ASSERT(a0 != REAL_C(0.0));

    real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

    coefficient[0] = b0 / a0;
    coefficient[1] = b1 / a0;
    coefficient[2] = b2 / a0;
    coefficient[3] = a1 / a0;
    coefficient[4] = a2 / a0;
}

real_t iir_process_sample(iir_t* iir, real_t sample)
{
    ASSERT(iir != NULL);

    real_t value = sample;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];
        real_t* state = &iir->state[section * IIR_STATE_COUNT];

        // Direct Form II transposed.
        real_t result = (coefficient[0] * value) + state[0];
        state[0] = (coefficient[1] * value) - (coefficient[3] * result) + state[1];
        state[1] = (coefficient[2] * value) - (coefficient[4] * result);

        value = result;
    }

    return value;
}

void iir_process_block(iir_t* iir, const real_t* input, real_t* output, uint32_t size)
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
        iir->state[index] = REAL_C(0.0);
    }
}

real_t iir_get_gain(iir_t* iir, real_t frequency)
{
    ASSERT(iir != NULL);

    real_t angle = REAL_C(2.0) * IIR_PI * frequency;
    real_t total = REAL_C(1.0);

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

        // The answer of one section is the value of its polynomial of the
        // input divided by the value of its polynomial of the feedback, both
        // at the point on the circle at this angle.
        real_t top_real = coefficient[0] + (coefficient[1] * REAL_COS(-angle))
                         + (coefficient[2] * REAL_COS(-REAL_C(2.0)*angle));
        real_t top_imaginary = (coefficient[1] * REAL_SIN(-angle))
                              + (coefficient[2] * REAL_SIN(-REAL_C(2.0)*angle));

        real_t bottom_real = REAL_C(1.0) + (coefficient[3] * REAL_COS(-angle))
                            + (coefficient[4] * REAL_COS(-REAL_C(2.0)*angle));
        real_t bottom_imaginary = (coefficient[3] * REAL_SIN(-angle))
                                 + (coefficient[4] * REAL_SIN(-REAL_C(2.0)*angle));

        real_t top = REAL_SQRT((top_real*top_real) + (top_imaginary*top_imaginary));
        real_t bottom = REAL_SQRT((bottom_real*bottom_real)
                             + (bottom_imaginary*bottom_imaginary));

        if(bottom == REAL_C(0.0))
        {
            return REAL_C(0.0);
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
static real_t iir_section_angle(uint32_t section, uint32_t sections)
{
    real_t order = REAL_C(2.0) * (real_t)sections;

    return (IIR_PI * (((REAL_C(2.0) * (real_t)section) + REAL_C(1.0)) / (REAL_C(2.0) * order)));
}

// Give every section the coefficients that let the signal pass unchanged.
static void iir_set_pass_through(iir_t* iir)
{
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section, REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0));
    }
}
