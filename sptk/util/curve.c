#ifndef TEST
#include <sptk/util/curve.h>
#include <sptk/core/defs.h>
#else
#include "curve.h"
#include "defs.h"
#endif

#include <math.h>

// The share of its top that every shape here has fallen to at one width from
// the middle, which is what a normal spread has fallen to at one standard
// deviation. Writing every width against the same share is what lets the width
// of one shape be set beside the width of another.
#define CURVE_AT_ONE_WIDTH      REAL_C(0.60653065971263342)

// The root of two, for turning a width into the half width a lorentzian is
// usually written with.
#define CURVE_ROOT_TWO          REAL_C(1.41421356237309505)

// How far either side of the middle the top of a skewed shape is looked for,
// in widths. The top moves away from the middle as the skew grows and comes
// back towards it again once the skew is large, and it never leaves this.
#define CURVE_TOP_REACH         REAL_C(3.0)

// How many times the search for that top narrows what it is looking in. Each
// turn keeps two thirds of the range, thus sixty turns brings a range of six
// widths down to far below anything a caller could measure.
#define CURVE_TOP_TURNS         60u

bool curve_is_valid_width(real_t width)
{
    return width > REAL_C(0.0);
}

bool curve_is_valid_shape(curve_shape_t shape)
{
    return (shape >= CURVE_GAUSSIAN) && (shape <= CURVE_SKEWED_GAUSSIAN);
}

real_t curve_gaussian(real_t at, real_t middle, real_t width)
{
    if(!curve_is_valid_width(width))
    {
        return REAL_C(0.0);
    }

    real_t from_middle = (at - middle) / width;

    return REAL_EXP(-REAL_C(0.5) * from_middle * from_middle);
}

real_t curve_lorentzian(real_t at, real_t middle, real_t width)
{
    if(!curve_is_valid_width(width))
    {
        return REAL_C(0.0);
    }

    // A lorentzian is usually written as one over one plus the square of the
    // distance divided by its half width. That half width is NOT the width
    // this module takes, thus it is worked out from it: the two are related by
    // whatever makes the shape fall to the same share at one width as a
    // gaussian does.
    //
    //     1 / (1 + (width/half)^2) = the share
    //
    // which rearranges to the line below.
    real_t half = width / REAL_SQRT((REAL_C(1.0) / CURVE_AT_ONE_WIDTH)
                                    - REAL_C(1.0));

    real_t from_middle = (at - middle) / half;

    return REAL_C(1.0) / (REAL_C(1.0) + (from_middle * from_middle));
}

// The skewed shape before it is brought to one at its top, read in widths from
// the middle.
//
// A gaussian multiplied by how much of a normal spread stands below the place,
// stretched by the skew. Where the skew is nothing the second half is exactly
// one and this is the plain gaussian doubled.
static real_t curve_skew_shape(real_t from_middle, real_t skew)
{
    real_t bump = REAL_EXP(-REAL_C(0.5) * from_middle * from_middle);
    real_t leaning = REAL_C(1.0)
                     + REAL_ERF((skew * from_middle) / CURVE_ROOT_TWO);

    return bump * leaning;
}

// Where the top of the skewed shape stands, in widths from the middle.
//
// FOUND BY LOOKING AND NOT BY A FORMULA, because there is no formula: the place
// where the shape stops rising is where a gaussian and an error function
// balance, and that has no closed form. The shape rises to one top and falls
// away either side of it, thus narrowing the range around the larger of two
// places inside it finds that top and cannot find anything else.
static real_t curve_skew_top(real_t skew)
{
    if(skew == REAL_C(0.0))
    {
        return REAL_C(0.0);
    }

    // The shape with the skew turned round is the mirror of the shape itself,
    // thus only one side need be looked at.
    real_t leaning = REAL_ABS(skew);

    real_t low = REAL_C(0.0);
    real_t high = CURVE_TOP_REACH;

    for(uint32_t turn = 0; turn < CURVE_TOP_TURNS; turn++)
    {
        real_t third = (high - low) / REAL_C(3.0);
        real_t first = low + third;
        real_t second = high - third;

        if(curve_skew_shape(first, leaning)
           < curve_skew_shape(second, leaning))
        {
            low = first;
        }
        else
        {
            high = second;
        }
    }

    real_t found = (low + high) / REAL_C(2.0);

    return (skew < REAL_C(0.0)) ? -found : found;
}

real_t curve_skewed_gaussian_top(real_t middle, real_t width, real_t skew)
{
    if(!curve_is_valid_width(width))
    {
        return middle;
    }

    return middle + (width * curve_skew_top(skew));
}

real_t curve_skewed_gaussian(real_t at, real_t middle, real_t width,
                             real_t skew)
{
    if(!curve_is_valid_width(width))
    {
        return REAL_C(0.0);
    }

    real_t top = curve_skew_top(skew);
    real_t tallest = curve_skew_shape(top, skew);

    if(tallest <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    return curve_skew_shape((at - middle) / width, skew) / tallest;
}

real_t curve_value(curve_shape_t shape, real_t at, real_t middle,
                   real_t width, real_t skew)
{
    if(!curve_is_valid_shape(shape))
    {
        return REAL_C(0.0);
    }

    if(shape == CURVE_GAUSSIAN)
    {
        return curve_gaussian(at, middle, width);
    }

    if(shape == CURVE_LORENTZIAN)
    {
        return curve_lorentzian(at, middle, width);
    }

    return curve_skewed_gaussian(at, middle, width, skew);
}

bool curve_block(curve_shape_t shape, real_t from, real_t to, real_t middle,
                 real_t width, real_t skew, real_t* output, uint32_t count)
{
    ASSERT(output != NULL);

    if(!curve_is_valid_shape(shape) || !curve_is_valid_width(width)
       || (count == 0u))
    {
        return false;
    }

    // THE TOP OF A SKEWED SHAPE IS LOOKED FOR ONCE AND NOT AT EVERY PLACE.
    // Looking for it costs about sixty times what reading the shape costs, thus
    // a block of ten thousand places that looked for it each time would spend
    // nearly all of its work finding the same number over and over.
    real_t tallest = REAL_C(1.0);

    if(shape == CURVE_SKEWED_GAUSSIAN)
    {
        tallest = curve_skew_shape(curve_skew_top(skew), skew);

        if(tallest <= REAL_SMALLEST)
        {
            return false;
        }
    }

    // One place at each end where there is more than one, so that a caller
    // asking across a range gets the whole of that range.
    real_t between = (count > 1u)
                     ? ((to - from) / (real_t)(count - 1u))
                     : REAL_C(0.0);

    for(uint32_t index = 0; index < count; index++)
    {
        real_t at = from + (between * (real_t)index);

        if(shape == CURVE_SKEWED_GAUSSIAN)
        {
            output[index] = curve_skew_shape((at - middle) / width, skew)
                            / tallest;
        }
        else
        {
            output[index] = curve_value(shape, at, middle, width, skew);
        }
    }

    return true;
}
