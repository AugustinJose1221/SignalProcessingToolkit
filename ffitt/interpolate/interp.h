#ifndef INTERP_H
#define INTERP_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// Reading a value between the points of a table.
//
// A device is given a table: at these inputs, that output. Every real reading
// falls between two of them. What to do there is a choice, and the three ways
// here answer three different needs.
//
//   LINEAR      a straight line between the two neighbours.
//   PCHIP       a smooth curve that NEVER GOES OUTSIDE the two neighbours.
//   cspline     a smooth curve that may.
//
// THE ONE THAT MATTERS IS THE THIRD LINE, AND IT IS A TRAP
//
// A cubic spline lays a single smooth curve through every point, and it is the
// right answer when the thing behind the table really is smooth. It buys that
// smoothness by letting the curve OVERSHOOT: between two points the curve may
// rise above both of them or fall below both.
//
// For a calibration table that is wrong, and wrong in a way nobody notices. A
// thermistor table that rises from 20 to 30 degrees between two entries can be
// read by a spline as 31, which is a temperature the two entries do not
// bracket and the device never measured. Worse, a table that only ever rises
// can be read by a spline as falling.
//
// Measured, on a table that is flat, steps up from 0 to 10 once, and is flat
// again, which is what a calibration of something with a threshold looks like:
//
//                  lowest    highest    outside the table by
//     linear        0.000     10.000            nothing
//     pchip         0.000     10.000            nothing
//     cspline      -1.094     11.078            22 percent
//
// The spline reports MINUS ONE for a table that holds nothing below zero. Read
// as a temperature, that is a device saying it is below freezing because the
// table happened to step.
//
// And the shape is wrong as well as the range. Walking the same table from end
// to end at 600 places:
//
//     cspline goes DOWN at 262 of them
//     pchip   goes down at none
//
// The table only ever rises. A device watching for a fall would see 262 of
// them, and every one would be the reading and not the thing being read.
//
// PCHIP is the answer. It is smooth, its slope has no corners, and it cannot
// overshoot, because at each point it chooses a slope that the neighbours
// allow. Where the table rises the curve rises; where the table is flat the
// curve is flat.
//
// WHICH TO TAKE
//
//   the table is a MEASUREMENT and must not be exceeded    pchip
//   the thing behind the table is truly smooth             cspline
//   the cost must be as small as it can be                 linear
//   the table has only two points                          any; all agree
//
// THE INPUTS MUST RISE THROUGH THE TABLE. That is what lets a search find the
// place in a few steps rather than by walking it. A table written the other
// way round must be turned round first.

typedef enum{
    INTERP_LINEAR = 0,          // A straight line between the neighbours
    INTERP_PCHIP                // Smooth, and never outside the neighbours
}interp_kind_t;

// True if the module knows this kind.
bool interp_is_valid_kind(interp_kind_t kind);

// True if a table of the given inputs can be read: at least two points, and
// the inputs rise through it with no two the same.
//
// Ask this once when the table is set up. Two entries with the same input
// would ask the curve to hold two values at one place, and the answer would be
// a division by nothing.
bool interp_is_valid_table(const real_t* input, uint32_t size);

// How many working values interp_pchip needs for a table of the given size,
// which is one slope for each point.
#define INTERP_SLOPE_COUNT(size)    (size)

// Read the table at one place, with a straight line between the neighbours.
//
// A place below the first input or above the last is outside the table. The
// answer is then the first or the last output, held flat rather than carried
// on: a straight line carried on past the end of a calibration says what the
// device would read at a temperature it was never calibrated at, and saying
// nothing is better than saying that.
real_t interp_linear(const real_t* input, const real_t* output, uint32_t size,
                     real_t place);

// Work out the slope at each point of the table, for pchip.
//
// The slopes must hold INTERP_SLOPE_COUNT values. This runs once when the
// table is set up; interp_pchip then reads the table and the slopes together.
//
// Give false if the table cannot be read.
bool interp_pchip_slopes(const real_t* input, const real_t* output,
                         uint32_t size, real_t* slopes);

// Read the table at one place, smoothly and without overshooting.
//
// The slopes must be the ones that interp_pchip_slopes gave for this table.
// Outside the table the answer is held flat, as with a straight line.
real_t interp_pchip(const real_t* input, const real_t* output,
                    const real_t* slopes, uint32_t size, real_t place);

// Read the table at many places at once, into a list of answers.
//
// For INTERP_PCHIP the slopes must be the ones that interp_pchip_slopes gave.
// For INTERP_LINEAR they are not read and may be NULL.
//
// Give false if the table or the kind cannot be used.
bool interp_block(const real_t* input, const real_t* output,
                  const real_t* slopes, uint32_t size, interp_kind_t kind,
                  const real_t* places, real_t* answers, uint32_t count);

#endif//INTERP_H
