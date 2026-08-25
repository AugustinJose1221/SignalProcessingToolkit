#ifndef TEST
#include <sptk/transform/window.h>
#include <sptk/core/defs.h>
#else
#include "window.h"
#include "defs.h"
#endif

#include <math.h>

#define WINDOW_PI       REAL_C(3.14159265358979323846)

// The modified Bessel function of the first kind, of the order zero.
//
// The Kaiser window needs it. There is no such function in the standard
// library, thus it stands here. The series below converges quickly for the
// values that a window asks for, which lie under about 20.
static real_t window_bessel(real_t x)
{
    // Each term of the series is the one before it times (x/2)^2/k^2, thus no
    // factorial and no power has to be worked out on its own. That keeps the
    // numbers small and holds the accuracy.
    real_t half = x / REAL_C(2.0);
    real_t term = REAL_C(1.0);
    real_t total = REAL_C(1.0);

    for(uint32_t k = 1; k < 40u; k++)
    {
        term *= (half / (real_t)k) * (half / (real_t)k);
        total += term;

        if(term < (REAL_C(1.0e-9) * total))
        {
            break;
        }
    }

    return total;
}

// The value of a window that is a sum of cosines. Every fixed window of this
// module is such a sum, thus one function serves them all.
static real_t window_cosine_sum(uint32_t index, uint32_t size,
                               real_t a0, real_t a1, real_t a2, real_t a3)
{
    if(size <= 1u)
    {
        return REAL_C(1.0);
    }

    // The window is symmetric, thus the last value stands at 2*pi and equals
    // the first one. The divisor is size-1 and not size for that reason.
    real_t turn = (REAL_C(2.0) * WINDOW_PI * (real_t)index) / (real_t)(size - 1u);

    return a0
           - (a1 * REAL_COS(turn))
           + (a2 * REAL_COS(REAL_C(2.0) * turn))
           - (a3 * REAL_COS(REAL_C(3.0) * turn));
}

static real_t window_tukey(uint32_t index, uint32_t size, real_t part)
{
    if(size <= 1u)
    {
        return REAL_C(1.0);
    }
    if(part <= REAL_C(0.0))
    {
        return REAL_C(1.0);
    }
    if(part > REAL_C(1.0))
    {
        part = REAL_C(1.0);
    }

    real_t last = (real_t)(size - 1u);
    real_t position = (real_t)index / last;
    real_t edge = part / REAL_C(2.0);

    if(position < edge)
    {
        // The falling part at the start, which is half of a Hann window.
        return REAL_C(0.5) * (REAL_C(1.0) + REAL_COS(WINDOW_PI * ((position / edge) - REAL_C(1.0))));
    }
    if(position > (REAL_C(1.0) - edge))
    {
        return REAL_C(0.5) * (REAL_C(1.0) + REAL_COS(WINDOW_PI * (((position - REAL_C(1.0)) / edge) + REAL_C(1.0))));
    }

    // The flat part in the middle.
    return REAL_C(1.0);
}

static real_t window_kaiser(uint32_t index, uint32_t size, real_t beta)
{
    if(size <= 1u)
    {
        return REAL_C(1.0);
    }
    if(beta < REAL_C(0.0))
    {
        beta = REAL_C(0.0);
    }

    real_t last = (real_t)(size - 1u);
    // A number from -1 at the first sample to 1 at the last one.
    real_t position = ((REAL_C(2.0) * (real_t)index) / last) - REAL_C(1.0);
    real_t inside = REAL_C(1.0) - (position * position);

    if(inside < REAL_C(0.0))
    {
        inside = REAL_C(0.0);
    }

    return window_bessel(beta * REAL_SQRT(inside)) / window_bessel(beta);
}

bool window_is_valid_kind(window_kind_t kind)
{
    return (kind >= WINDOW_RECTANGULAR) && (kind <= WINDOW_KAISER);
}

bool window_is_valid_size(uint32_t size, window_kind_t kind)
{
    if(!window_is_valid_kind(kind) || (size == 0u))
    {
        return false;
    }

    // A rectangular window takes nothing away, thus it has no ends to fall at
    // and any size serves.
    if(kind == WINDOW_RECTANGULAR)
    {
        return true;
    }

    // Every other window here tapers. A size of 1 is the single value 1, which
    // is well defined and useful. A size of 2 is the two ends and nothing
    // else, which is not.
    return (size != 2u);
}

bool window_takes_a_parameter(window_kind_t kind)
{
    return (kind == WINDOW_TUKEY) || (kind == WINDOW_KAISER);
}

real_t window_value(uint32_t index, uint32_t size, window_kind_t kind,
                   real_t parameter)
{
    ASSERT(window_is_valid_kind(kind));

    if(index >= size)
    {
        return REAL_C(0.0);
    }

    switch(kind)
    {
        case WINDOW_HANN:
            return window_cosine_sum(index, size, REAL_C(0.5), REAL_C(0.5), REAL_C(0.0), REAL_C(0.0));

        case WINDOW_HAMMING:
            return window_cosine_sum(index, size, REAL_C(0.54), REAL_C(0.46), REAL_C(0.0), REAL_C(0.0));

        case WINDOW_BLACKMAN:
            return window_cosine_sum(index, size, REAL_C(0.42), REAL_C(0.5), REAL_C(0.08), REAL_C(0.0));

        case WINDOW_BLACKMAN_HARRIS:
            return window_cosine_sum(index, size,
                                     REAL_C(0.35875), REAL_C(0.48829), REAL_C(0.14128), REAL_C(0.01168));

        case WINDOW_TUKEY:
            return window_tukey(index, size, parameter);

        case WINDOW_KAISER:
            return window_kaiser(index, size, parameter);

        case WINDOW_RECTANGULAR:
        default:
            return REAL_C(1.0);
    }
}

void window_build_with(real_t* window, uint32_t size, window_kind_t kind,
                       real_t parameter)
{
    ASSERT(window != NULL);
    ASSERT(window_is_valid_kind(kind));

    for(uint32_t index = 0; index < size; index++)
    {
        window[index] = window_value(index, size, kind, parameter);
    }
}

void window_build(real_t* window, uint32_t size, window_kind_t kind)
{
    window_build_with(window, size, kind, REAL_C(0.0));
}

real_t window_kaiser_beta(real_t stop_band_decibel)
{
    // The rule of Kaiser, in three pieces over the range of the level. The
    // level is the stop band of a filter that the window builds, not the side
    // lobe of the window. The header says why that matters.
    real_t level = (stop_band_decibel < REAL_C(0.0))
                  ? -stop_band_decibel : stop_band_decibel;

    if(level > REAL_C(50.0))
    {
        return REAL_C(0.1102) * (level - REAL_C(8.7));
    }
    if(level >= REAL_C(21.0))
    {
        return (REAL_C(0.5842) * REAL_POW(level - REAL_C(21.0), REAL_C(0.4)))
               + (REAL_C(0.07886) * (level - REAL_C(21.0)));
    }

    // Under 21 dB no window is needed: a rectangular one already does it.
    return REAL_C(0.0);
}

real_t window_coherent_gain(const real_t* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t total = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        total += window[index];
    }

    return total / (real_t)size;
}

real_t window_noise_gain(const real_t* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t total = REAL_C(0.0);
    for(uint32_t index = 0; index < size; index++)
    {
        total += window[index] * window[index];
    }

    return REAL_SQRT(total / (real_t)size);
}

real_t window_noise_bandwidth(const real_t* window, uint32_t size)
{
    ASSERT(window != NULL);

    if(size == 0u)
    {
        return REAL_C(0.0);
    }

    real_t sum = REAL_C(0.0);
    real_t squares = REAL_C(0.0);

    for(uint32_t index = 0; index < size; index++)
    {
        sum += window[index];
        squares += window[index] * window[index];
    }

    if((sum * sum) <= REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    return ((real_t)size * squares) / (sum * sum);
}

void window_apply(const real_t* window, const real_t* input, real_t* output,
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
