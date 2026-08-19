#ifndef TEST
#include <sptk/transform/window.h>
#include <sptk/core/defs.h>
#else
#include "window.h"
#include "defs.h"
#endif

#include <math.h>

#define WINDOW_PI       3.14159265358979323846f

// The modified Bessel function of the first kind, of the order zero.
//
// The Kaiser window needs it. There is no such function in the standard
// library, thus it stands here. The series below converges quickly for the
// values that a window asks for, which lie under about 20.
static float window_bessel(float x)
{
    // Each term of the series is the one before it times (x/2)^2/k^2, thus no
    // factorial and no power has to be worked out on its own. That keeps the
    // numbers small and holds the accuracy.
    float half = x / 2.0f;
    float term = 1.0f;
    float total = 1.0f;

    for(uint32_t k = 1; k < 40u; k++)
    {
        term *= (half / (float)k) * (half / (float)k);
        total += term;

        if(term < (1.0e-9f * total))
        {
            break;
        }
    }

    return total;
}

// The value of a window that is a sum of cosines. Every fixed window of this
// module is such a sum, thus one function serves them all.
static float window_cosine_sum(uint32_t index, uint32_t size,
                               float a0, float a1, float a2, float a3)
{
    if(size <= 1u)
    {
        return 1.0f;
    }

    // The window is symmetric, thus the last value stands at 2*pi and equals
    // the first one. The divisor is size-1 and not size for that reason.
    float turn = (2.0f * WINDOW_PI * (float)index) / (float)(size - 1u);

    return a0
           - (a1 * cosf(turn))
           + (a2 * cosf(2.0f * turn))
           - (a3 * cosf(3.0f * turn));
}

static float window_tukey(uint32_t index, uint32_t size, float part)
{
    if(size <= 1u)
    {
        return 1.0f;
    }
    if(part <= 0.0f)
    {
        return 1.0f;
    }
    if(part > 1.0f)
    {
        part = 1.0f;
    }

    float last = (float)(size - 1u);
    float position = (float)index / last;
    float edge = part / 2.0f;

    if(position < edge)
    {
        // The falling part at the start, which is half of a Hann window.
        return 0.5f * (1.0f + cosf(WINDOW_PI * ((position / edge) - 1.0f)));
    }
    if(position > (1.0f - edge))
    {
        return 0.5f * (1.0f + cosf(WINDOW_PI * (((position - 1.0f) / edge) + 1.0f)));
    }

    // The flat part in the middle.
    return 1.0f;
}

static float window_kaiser(uint32_t index, uint32_t size, float beta)
{
    if(size <= 1u)
    {
        return 1.0f;
    }
    if(beta < 0.0f)
    {
        beta = 0.0f;
    }

    float last = (float)(size - 1u);
    // A number from -1 at the first sample to 1 at the last one.
    float position = ((2.0f * (float)index) / last) - 1.0f;
    float inside = 1.0f - (position * position);

    if(inside < 0.0f)
    {
        inside = 0.0f;
    }

    return window_bessel(beta * sqrtf(inside)) / window_bessel(beta);
}

bool window_is_valid_kind(window_kind_t kind)
{
    return (kind >= WINDOW_RECTANGULAR) && (kind <= WINDOW_KAISER);
}

bool window_takes_a_parameter(window_kind_t kind)
{
    return (kind == WINDOW_TUKEY) || (kind == WINDOW_KAISER);
}

float window_value(uint32_t index, uint32_t size, window_kind_t kind,
                   float parameter)
{
    ASSERT(window_is_valid_kind(kind));

    if(index >= size)
    {
        return 0.0f;
    }

    switch(kind)
    {
        case WINDOW_HANN:
            return window_cosine_sum(index, size, 0.5f, 0.5f, 0.0f, 0.0f);

        case WINDOW_HAMMING:
            return window_cosine_sum(index, size, 0.54f, 0.46f, 0.0f, 0.0f);

        case WINDOW_BLACKMAN:
            return window_cosine_sum(index, size, 0.42f, 0.5f, 0.08f, 0.0f);

        case WINDOW_BLACKMAN_HARRIS:
            return window_cosine_sum(index, size,
                                     0.35875f, 0.48829f, 0.14128f, 0.01168f);

        case WINDOW_TUKEY:
            return window_tukey(index, size, parameter);

        case WINDOW_KAISER:
            return window_kaiser(index, size, parameter);

        case WINDOW_RECTANGULAR:
        default:
            return 1.0f;
    }
}

void window_build_with(float* window, uint32_t size, window_kind_t kind,
                       float parameter)
{
    ASSERT(window != NULL);
    ASSERT(window_is_valid_kind(kind));

    for(uint32_t index = 0; index < size; index++)
    {
        window[index] = window_value(index, size, kind, parameter);
    }
}

void window_build(float* window, uint32_t size, window_kind_t kind)
{
    window_build_with(window, size, kind, 0.0f);
}

float window_kaiser_beta(float stop_band_decibel)
{
    // The rule of Kaiser, in three pieces over the range of the level. The
    // level is the stop band of a filter that the window builds, not the side
    // lobe of the window. The header says why that matters.
    float level = (stop_band_decibel < 0.0f)
                  ? -stop_band_decibel : stop_band_decibel;

    if(level > 50.0f)
    {
        return 0.1102f * (level - 8.7f);
    }
    if(level >= 21.0f)
    {
        return (0.5842f * powf(level - 21.0f, 0.4f))
               + (0.07886f * (level - 21.0f));
    }

    // Under 21 dB no window is needed: a rectangular one already does it.
    return 0.0f;
}

float window_coherent_gain(const float* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    float total = 0.0f;
    for(uint32_t index = 0; index < size; index++)
    {
        total += window[index];
    }

    return total / (float)size;
}

float window_noise_gain(const float* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    float total = 0.0f;
    for(uint32_t index = 0; index < size; index++)
    {
        total += window[index] * window[index];
    }

    return sqrtf(total / (float)size);
}

float window_noise_bandwidth(const float* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return 0.0f;
    }

    float sum = 0.0f;
    float squares = 0.0f;

    for(uint32_t index = 0; index < size; index++)
    {
        sum += window[index];
        squares += window[index] * window[index];
    }

    if((sum * sum) <= 0.0f)
    {
        return 0.0f;
    }

    return ((float)size * squares) / (sum * sum);
}

void window_apply(const float* window, const float* input, float* output,
                  uint32_t size)
{
    ASSERT(window != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = input[index] * window[index];
    }
}
