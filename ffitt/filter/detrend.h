#ifndef DETREND_H
#define DETREND_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// Take the level, or the level and the drift, out of a block of samples.
//
// A block of readings almost never sits about zero. A converter gives a large
// constant part, and a sensor that warms up gives a slow drift on top of it.
// Neither carries anything, and both ruin whatever is done next.
//
// WHY IT MATTERS MORE THAN IT LOOKS
//
// A transform of a block that ends higher than it began sees a step, because
// the transform reads the block as one period of something that repeats. The
// step is not in the signal; it is the join between the end of the block and
// the start of the next copy of it. Its energy spreads across EVERY frequency
// and buries whatever was there. Measured, on a wave of one unit at bin 8 with
// a drift of 4 units across the block:
//
//     what is at bin 1, which holds no signal at all
//     with the drift left in         1.27
//     with the drift taken out       0.08
//
// The false answer at a bin that holds nothing is larger than the true signal,
// which stands at 0.99 where it belongs. A window helps, but it cannot undo a
// drift; it only softens the join.
//
// WHAT IT IS NOT
//
// This is a BLOCK operation. It reads the whole block before it can give the
// first answer, thus it cannot run on a stream. For a stream the dcblock
// module follows the level as it goes and takes it away one sample at a time.
//
// Nor is this a filter. It fits one straight line across the whole block and
// takes that away. A drift that turns round in the middle of the block is not
// a straight line and will not go; that wants a high pass, or a shorter block.
//
// AND IT TAKES A LITTLE OF THE SIGNAL WITH THE DRIFT. THIS IS NOT SMALL.
//
// A straight line through a block always holds something in common with a wave
// in that block, thus fitting one takes a piece of the wave away. How much
// depends on where the wave sits against the middle of the block, and the two
// cases are far apart. Measured, as the part of the amplitude that goes:
//
//   A wave EVEN about the middle of the block loses 3/(n+1) of itself, and
//   nothing else about the wave matters. That is 0.046 for a block of 64 and
//   0.0007 for a block of 4096: it falls away as the block grows, and for any
//   block worth transforming it is nothing.
//
//   A wave ODD about the middle loses about 3 divided by (pi times the number
//   of periods it makes across the block), AND THE LENGTH OF THE BLOCK DOES
//   NOT COME INTO IT:
//
//     periods in the block      1       4      16      32
//     part of the wave taken   0.95    0.24    0.06    0.03
//
//   A wave that makes ONE period across the block loses 95 of every 100 parts
//   of itself, at a block of 64 samples and at a block of 4096 alike.
//
// The fit is not at fault. A wave that makes one period across the block IS a
// drift, as far as anything looking at that block can tell, and no method can
// separate the two. THE ANSWER IS TO KEEP THE BLOCK LONG COMPARED WITH THE
// LOWEST FREQUENCY WANTED: at sixteen periods the loss is 6 parts in 100 and
// at thirty-two it is 3, whatever the sample rate.
//
// THE ARITHMETIC, AND WHY THE SAMPLES ARE NUMBERED FROM THE MIDDLE
//
// A straight line of least squared error has a closed form, thus this module
// does not go near the general fit of the lstsq module. It numbers the samples
// from the MIDDLE of the block rather than from the start, which makes their
// numbers sum to zero and keeps every sum below the size of the samples
// themselves.
//
// That is not free precision; it is measured precision. On a block of 4096
// samples rising by 0.001 each, the worst the recovered trend is out across
// the block, at 32 bits:
//
//     level of the block        0    1 000   100 000   8 300 000
//     numbered from the start   0.000013  0.00073   8.20     614.5
//     numbered from the middle  0.0000025 0.000047  2.05     153.7
//
// About four times better at every level, for the same work. At 64 bits both
// give the answer exactly and the choice does not show.
//
// READ THE LAST COLUMN, NOT THE FIRST. A block sitting on a level of eight
// million is out by 150 units however it is numbered, because at 32 bits the
// samples themselves are that coarse: the level has spent the digits before
// this module ever sees them. WHERE A BLOCK SITS ON A LARGE LEVEL, TAKE THE
// LEVEL OUT FIRST, with the dcblock module as the samples arrive or with
// DETREND_CONSTANT here, and only then look for a drift.
//
// The neighbouring trap is worth knowing. A trend that bends is not this
// module's work, and reaching for lstsq_polyfit with the sample number as x is
// how it is usually attempted. THAT IS REFUSED AT 32 BITS from the second
// order upwards, for a block of any length, because the sample number sits far
// from zero and its powers cannot stay apart. Use lstsq_polyfit_scaled, which
// exists for exactly this.
//
// BECAUSE OF THE NUMBERING, the slope and the offset are about the middle of
// the block. The offset is the value of the trend at the middle, which is the
// mean of the block, and NOT the value at the first sample. Use
// detrend_trend_at to get the trend at a sample rather than working it out by
// hand.

// Which trend to take away.
typedef enum{
    // The mean of the block. Use this where the readings sit at a level that
    // does not move.
    DETREND_CONSTANT = 0,

    // The straight line of least squared error through the block. Use this
    // where the level drifts across the block, which a sensor that warms up
    // and almost any long recording will do.
    DETREND_LINEAR
}detrend_kind_t;

// True if the kind is one this module knows.
bool detrend_is_valid_kind(detrend_kind_t kind);

// Give the trend of a block without changing it.
//
// The offset is the value of the trend at the MIDDLE of the block, which for
// both kinds is the mean. The slope is how much the trend rises for each
// sample, and it is zero for the constant kind.
//
// Use this where the trend itself is the thing wanted: the drift of a sensor
// across a recording is the slope, multiplied by the number of samples.
//
// Give false if the kind is not known, or if the block is too short: the
// constant kind needs one sample and the straight line needs two.
bool detrend_trend(const real_t* input, uint32_t size, detrend_kind_t kind,
                   real_t* offset, real_t* slope);

// Give the value of a trend at one sample of the block.
//
// The offset and the slope must be the ones detrend_trend gave, and the size
// must be the size of the same block, because the offset is the value at the
// middle and the middle is where the size puts it.
real_t detrend_trend_at(real_t offset, real_t slope, uint32_t size,
                        uint32_t index);

// Take the trend out of a block.
//
// The output may be the input, and then the block is changed in place.
//
// Give false if the kind is not known, or if the block is too short.
bool detrend_block(const real_t* input, real_t* output, uint32_t size,
                   detrend_kind_t kind);

// Take a trend that is already known out of a block.
//
// This is for the second block onwards, where the trend was worked out once
// from a block that is known to be quiet and must now be taken out of every
// block that follows. Working the trend out afresh from each block would take
// the signal out along with the trend, where the signal itself rises across
// the block.
//
// The output may be the input. The size must be the size the trend was found
// with, for the reason detrend_trend_at gives.
//
// Give false if the block is empty.
bool detrend_remove(const real_t* input, real_t* output, uint32_t size,
                    real_t offset, real_t slope);

#endif//DETREND_H
