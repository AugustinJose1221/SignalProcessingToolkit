#ifndef MATCHED_H
#define MATCHED_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Look for a known shape in a noisy reading.
//
// A radar sends a chirp and waits for it to come back. A depth sounder sends a
// ping. A tag reader sends a code. In each of them the shape that will come
// back is KNOWN, and the only questions are whether it came back and when. The
// answer is not a filter of frequency: the shape covers the same band as the
// noise, thus no band can be kept or thrown away.
//
// What parts them is SHAPE. Slide the known shape along the reading and add up
// the products at each offset. Where the reading holds the shape, every product
// is positive at once and the sum is large. Where it holds only noise, the
// products cancel. Of everything that can be done to a reading with a known
// shape in it, this gives the largest answer for the noise it lets through, and
// nothing else does better.
//
// THE SCORE IS IN UNITS OF THE NOISE. The sum is divided by the square root of
// the energy of the shape, thus a reading of pure noise of standard deviation
// s gives a score whose standard deviation is also s. Divide the score by the
// noise of the reading and the answer says HOW MANY STANDARD DEVIATIONS this
// offset stands out by, whatever the shape was and however loud it was. That is
// the number a threshold can be set on, and matched_threshold_for gives it.
//
// THE ONE WAY THIS FAILS QUIETLY: the shape must be the shape that will arrive,
// not the shape that was sent. A path that stretches, delays or colours it
// leaves a matched filter matched to something else, and the score falls away
// with no sign of why.

typedef struct{
    const real_t* pattern;      // The shape being looked for
    uint32_t length;            // How many samples long it is
    real_t root_energy;         // The square root of its energy
    bool designed;              // True once matched_design has been called
}matched_t;

#ifndef MATCHED_LARGEST_LENGTH
#define MATCHED_LARGEST_LENGTH      65536u
#endif

// How many scores a reading of this many samples gives for a shape of this
// length. The shape must lie whole inside the reading, thus the last offset
// that can be scored is the one where its end reaches the end of the reading.
#define MATCHED_SCORE_COUNT(count, length)      (((count) - (length)) + 1u)

// Give whether a shape of this length can be looked for. A shape of no samples
// says nothing, and the bound above is what the sums are held to.
bool matched_is_valid_length(uint32_t length);

// Give a filter that is not yet looking for anything. Give it a shape with
// matched_design before asking it for a score.
matched_t matched_make(void);

// Tell the filter which shape to look for.
//
// THE FILTER KEEPS THE POINTER AND DOES NOT COPY THE SHAPE. The shape must
// stand still for as long as the filter is used, which is what lets a shape of
// any length be used without taking memory from the heap.
//
// Give false and leave the filter as it was if the length is not valid or if
// the shape holds no energy at all, because a shape of nothing would be found
// everywhere.
bool matched_design(matched_t* matched, const real_t* pattern,
                    uint32_t length);

// Score every offset of a reading.
//
// The output holds MATCHED_SCORE_COUNT(count, length) values, and the value at
// offset k says how much the reading looks like the shape when the shape begins
// at sample k. Divide by the noise of the reading to read the score in standard
// deviations.
//
// Give false if the filter holds no shape or if the reading is shorter than the
// shape.
bool matched_score_block(const matched_t* matched, const real_t* signal,
                         uint32_t count, real_t* score);

// Score one offset. The reading must hold at least as many samples from here on
// as the shape is long.
real_t matched_score_at(const matched_t* matched, const real_t* signal);

// Give the largest score of a reading and where it stands.
//
// This is the answer where a reading is known to hold the shape once and the
// only question is when. Where it may hold the shape more than once, or not at
// all, score the whole reading and use a threshold.
//
// Give false, and leave both answers as they were, on the same grounds as
// matched_score_block.
bool matched_best(const matched_t* matched, const real_t* signal,
                  uint32_t count, uint32_t* where, real_t* score);

// Give how many standard deviations a score must reach before it is called a
// find, for a wanted rate of false alarms.
//
// THE RATE IS FOR THE WHOLE SEARCH AND NOT FOR ONE OFFSET, and the difference
// is the mistake this function exists to stop. A threshold that is wrong once
// in a thousand offsets is wrong about once in every reading of a thousand
// offsets, thus a search that looks at 10 000 offsets with that threshold cries
// wolf ten times over. Give the number of offsets that will be looked at and
// the rate wanted across all of them together.
//
// The number rests on the noise being even about nothing and on one offset
// saying nothing about the next. Where the noise is coloured, the offsets lean
// on each other, fewer of them are really free, and the threshold this gives is
// higher than it needs to be.
//
// Give a rate above 0 and below 1, and at least one offset. Give 0 otherwise.
real_t matched_threshold_for(real_t false_alarm_rate, uint32_t offsets);

#endif//MATCHED_H
