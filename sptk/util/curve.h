#ifndef CURVE_H
#define CURVE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// The shapes a peak can have, read at a place.
//
// These are not waves. A wave has a frequency and a phase and goes on for ever,
// and generate makes those. THESE HAPPEN ONCE: a bump on a baseline, read at
// whatever place is asked for. Nothing here carries state and nothing here has
// a sample rate.
//
// WHAT THEY ARE FOR.
//
// A peak in a real measurement has a SHAPE, and which shape it has decides what
// may be read off it. A chromatograph gives peaks close to gaussian; a
// resonance gives peaks close to lorentzian; a peak that arrives slowly and
// leaves quickly is neither. Every module in this library that finds a peak or
// refines one -- peakdetect, delay_refine_peak, the fitting in lstsq -- gives
// an answer that depends on the shape it was given, and the only honest way to
// measure that dependence is against a shape that is KNOWN.
//
// THE ONE THING THAT MATTERS MOST, AND IT IS NOT WHICH SHAPE IS PRETTIEST.
//
// delay_refine_peak fits a curve of the second order through a peak and its two
// neighbours. That curve is exact for a peak that IS of the second order and
// wrong for every other, and how wrong depends on the shape:
//
//   shape                 how far the refined top stands from the true one
//   -------------------   ------------------------------------------------
//   gaussian              small, and it leans the same way each time
//   lorentzian            larger, because the top is sharper than a curve
//   skewed gaussian       larger again, and it leans towards the long side
//
// A refinement measured only against a gaussian looks better than it is. That
// is what the skewed and the lorentzian shapes are here for.
//
// WHERE THE WIDTH IS MEASURED. Every shape here takes its width as the distance
// from the middle at which it has fallen to the same share of its top as a
// normal spread has at one standard deviation, which is about 0.6065. Written
// that way the widths of two different shapes may be set beside each other and
// mean the same thing, which they do not if one is given as a standard
// deviation and another as a half width at half the top.

// True if this is a width a curve can be read at, which means above nothing.
// A width of nothing is a peak of no width at all, and every shape here
// divides by it.
bool curve_is_valid_width(real_t width);

// A gaussian bump, standing at one at its middle.
//
// The shape of a great many things measured once and added up, and the shape to
// reach for where nothing says otherwise. A peak fitter tested only on this one
// is being tested on the kindest shape there is.
//
// Give 0 where the width is not one curve_is_valid_width accepts.
real_t curve_gaussian(real_t at, real_t middle, real_t width);

// A lorentzian bump, standing at one at its middle.
//
// The shape of a resonance: anything that rings at one frequency and dies away
// gives this. ITS TAILS ARE ENORMOUS BESIDE A GAUSSIAN'S. Measured, as a share
// of the top at each distance from the middle in widths:
//
//   widths      gaussian    lorentzian
//   ------      --------    ----------
//        0      1.000000      1.000000
//        1      0.606531      0.606531
//        2      0.135335      0.278173
//        3      0.011109      0.146231
//        5      0.000004      0.058079
//       10      0.000000      0.015181
//       20      0.000000      0.003839
//
// The two agree at one width, which is what the width is defined to mean, and
// part company everywhere else. At three widths the gaussian is down to a
// hundredth and the lorentzian holds a seventh; at twenty widths the gaussian
// has been nothing for a long time and the lorentzian still holds a two
// hundred and sixtieth of its top.
//
// That difference is why a peak fitter must be tried against both. A fitter
// that measures a baseline near a peak reads the tail of a lorentzian AS
// baseline and takes the peak to be smaller than it is, and nothing in the
// numbers says so.
//
// Give 0 where the width is not one curve_is_valid_width accepts.
real_t curve_lorentzian(real_t at, real_t middle, real_t width);

// A gaussian bump with one side stretched and the other squeezed.
//
// REAL PEAKS ARE RARELY EVEN. Anything that arrives quickly and leaves slowly
// gives a peak with a tail on one side: a sensor that warms fast and cools
// slowly, a bolus passing a detector, a chromatograph peak that tails. A fitter
// that assumes an even peak reports a middle that has been pulled towards the
// long side, and the amount it is pulled by does not go away with more samples.
//
// The skew says which way and how far. Nothing gives the plain gaussian, above
// nothing gives a tail on the high side, and below nothing gives one on the low
// side. Its size is best kept under about 8; past that the shape is nearly all
// tail and the width no longer means much.
//
// HOW FAR THE TOP MOVES DOES NOT GROW WITH THE SKEW FOR EVER. It moves out,
// turns round and comes back, because a very large skew cuts the shape off at
// the middle rather than leaning it. Measured, the top in widths from the
// middle:
//
//   skew      0.0    0.5    1.0    2.0    4.0    8.0
//   top       0.00   0.35   0.51   0.53   0.42   0.28
//
// A fitter that assumes an even peak is out by about this much, thus a skew of
// about 2 is the hardest case to give one.
//
// THE TOP DOES NOT STAND AT THE MIDDLE, and that is the point rather than a
// fault. The middle is where the even part of the shape is centred; the top of
// a skewed peak stands to one side of it, and how far is exactly what a fitter
// gets wrong. curve_skewed_gaussian_top gives where it really stands.
//
// The answer stands at one at its top, thus the shapes may be compared.
//
// Give 0 where the width is not one curve_is_valid_width accepts.
real_t curve_skewed_gaussian(real_t at, real_t middle, real_t width,
                             real_t skew);

// Where the top of a skewed gaussian really stands.
//
// This is the number a peak fitter is trying to find, thus it is the number to
// measure a peak fitter against. It is found by walking rather than by a closed
// form, because there is no closed form.
//
// Give the middle itself where the width is not one curve_is_valid_width
// accepts, or where the skew is nothing and the top therefore is the middle.
real_t curve_skewed_gaussian_top(real_t middle, real_t width, real_t skew);

// Write a curve of the chosen shape across a list of places.
//
// The places run from `from` to `to` across the count, which is what a
// measurement swept across a range looks like. Give false if the count is
// nothing or the width is not one curve_is_valid_width accepts.
typedef enum{
    CURVE_GAUSSIAN = 0,
    CURVE_LORENTZIAN,
    CURVE_SKEWED_GAUSSIAN
}curve_shape_t;

// True if the shape is one this module knows.
bool curve_is_valid_shape(curve_shape_t shape);

// Read any of the shapes by name. The skew is ignored by the two that are even.
real_t curve_value(curve_shape_t shape, real_t at, real_t middle,
                   real_t width, real_t skew);

// Write one of the shapes across a list of evenly spaced places, which is what
// a measurement swept across a range looks like.
bool curve_block(curve_shape_t shape, real_t from, real_t to, real_t middle,
                 real_t width, real_t skew, real_t* output, uint32_t count);

#endif//CURVE_H
