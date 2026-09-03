// This file is left out of the build when FFITT_NO_UTIL is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_UTIL

#ifndef TEST
#include <ffitt/util/generate.h>
#include <ffitt/core/defs.h>
#else
#include "generate.h"
#include "defs.h"
#endif

#include <math.h>

#define GENERATE_PI     REAL_C(3.14159265358979323846)

bool generate_is_valid_kind(generate_kind_t kind)
{
    return (kind >= GENERATE_SINE) && (kind <= GENERATE_IMPULSE);
}

bool generate_is_valid_frequency(real_t frequency, real_t sample_rate)
{
    if(sample_rate <= REAL_C(0.0))
    {
        return false;
    }

    return (frequency > REAL_C(0.0))
           && (frequency < (sample_rate / REAL_C(2.0)));
}

// True where the kind makes noise rather than a wave, and thus reads no
// frequency at all.
static bool generate_is_noise(generate_kind_t kind)
{
    return (kind == GENERATE_WHITE_NOISE) || (kind == GENERATE_PINK_NOISE)
           || (kind == GENERATE_BROWN_NOISE) || (kind == GENERATE_BLUE_NOISE)
           || (kind == GENERATE_GAUSSIAN_NOISE);
}

bool generate_is_valid_part(real_t part)
{
    return (part > REAL_C(0.0)) && (part < REAL_C(1.0));
}

bool generate_set_part(generate_t* generate, real_t part)
{
    ASSERT(generate != NULL);

    if(!generate_is_valid_part(part))
    {
        return false;
    }

    generate->part = part;

    return true;
}

real_t generate_get_part(const generate_t* generate)
{
    ASSERT(generate != NULL);

    return generate->part;
}

generate_t generate_make(generate_kind_t kind)
{
    generate_t generate;

    generate.kind = kind;
    generate.phase = REAL_C(0.0);
    generate.step = REAL_C(0.0);
    generate.sweep = REAL_C(0.0);
    generate.last_step = REAL_C(0.0);
    generate.seed = 1u;
    generate.part = GENERATE_DEFAULT_PART;
    generate.running = REAL_C(0.0);
    generate.last_pink = REAL_C(0.0);
    generate.spare = REAL_C(0.0);
    generate.counted = 0u;
    generate.has_spare = false;
    generate.designed = false;

    for(uint32_t part = 0; part < GENERATE_PINK_PARTS; part++)
    {
        generate.pink[part] = REAL_C(0.0);
    }

    return generate;
}

void generate_reset(generate_t* generate)
{
    ASSERT(generate != NULL);

    generate->phase = REAL_C(0.0);
    generate->counted = 0u;
    generate->running = REAL_C(0.0);
    generate->last_pink = REAL_C(0.0);
    generate->spare = REAL_C(0.0);
    generate->has_spare = false;

    for(uint32_t part = 0; part < GENERATE_PINK_PARTS; part++)
    {
        generate->pink[part] = REAL_C(0.0);
    }
}

bool generate_design(generate_t* generate, real_t frequency,
                     real_t sample_rate)
{
    ASSERT(generate != NULL);

    if(!generate_is_valid_kind(generate->kind))
    {
        return false;
    }

    if(generate_is_noise(generate->kind))
    {
        generate->step = REAL_C(0.0);
        generate->sweep = REAL_C(0.0);
        generate->designed = true;

        return true;
    }

    if(!generate_is_valid_frequency(frequency, sample_rate))
    {
        return false;
    }

    // How far round the turn one sample carries the phase.
    generate->step = frequency / sample_rate;
    generate->sweep = REAL_C(0.0);
    generate->designed = true;

    return true;
}

bool generate_design_sweep(generate_t* generate, real_t from, real_t to,
                           real_t sample_rate, uint32_t samples)
{
    ASSERT(generate != NULL);

    if(!generate_is_valid_kind(generate->kind)
       || generate_is_noise(generate->kind))
    {
        return false;
    }

    if(!generate_is_valid_frequency(from, sample_rate)
       || !generate_is_valid_frequency(to, sample_rate)
       || (samples == 0u))
    {
        return false;
    }

    generate->step = from / sample_rate;
    generate->last_step = to / sample_rate;

    // How much the step moves at each sample so that it arrives exactly.
    generate->sweep = ((to - from) / sample_rate) / (real_t)samples;
    generate->designed = true;

    return true;
}

void generate_set_seed(generate_t* generate, uint32_t seed)
{
    ASSERT(generate != NULL);

    // A seed of nothing would leave the values below at nothing for ever,
    // since they are made by multiplying.
    generate->seed = (seed == 0u) ? 1u : seed;
}

real_t generate_get_phase(const generate_t* generate)
{
    ASSERT(generate != NULL);

    return generate->phase;
}

void generate_set_phase(generate_t* generate, real_t phase)
{
    ASSERT(generate != NULL);

    // Held inside one turn, so that a caller cannot leave it somewhere the
    // shapes below do not expect.
    real_t held = phase - REAL_FLOOR(phase);

    generate->phase = (held < REAL_C(0.0)) ? (held + REAL_C(1.0)) : held;
}

// The next random value, spread evenly from -1 to 1.
//
// This is the shift of xorshift, which is short, has no memory beyond one
// number, and gives the same values on every machine and at either width. That
// last point is what lets a test be repeated.
static real_t generate_next_random(generate_t* generate)
{
    uint32_t held = generate->seed;

    held ^= held << 13u;
    held ^= held >> 17u;
    held ^= held << 5u;

    generate->seed = held;

    // The top 24 bits, which is every bit that a float can hold exactly.
    real_t part = (real_t)(held >> 8u) / (real_t)(1u << 24u);

    return (REAL_C(2.0) * part) - REAL_C(1.0);
}

// THE SMOOTHING OF A CORNER, WHICH IS WHAT HOLDS THE FOLDING DOWN.
//
// A step between two samples is a thing a sampled signal cannot hold. What can
// be held is a step smoothed across the samples either side of where it really
// fell, and this gives how much to move each of them by.
//
// The place is how far the sample stands from the corner, as a part of one
// step of the phase: from 0 to 1 for the sample after it, and from -1 to 0 for
// the one before. A sample a whole step away is not moved at all.
static real_t generate_corner(real_t place)
{
    if(place >= REAL_C(0.0))
    {
        // Just after the corner.
        return (REAL_C(2.0) * place) - (place * place) - REAL_C(1.0);
    }

    // Just before it.
    return (place * place) + (REAL_C(2.0) * place) + REAL_C(1.0);
}

// How much to move a sample by, for a corner standing at the given place in
// the turn. Gives nothing where the sample is more than one step away from it.
static real_t generate_corner_at(real_t phase, real_t corner, real_t step)
{
    if(step <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    // How far past the corner this sample stands, held in the HALF TURN EITHER
    // SIDE of it so that a corner at the start of the turn is reached from both
    // ends of it.
    //
    // HELD EITHER SIDE AND NOT FROM NOTHING TO ONE, and the difference is a
    // fault that was here before. Held from nothing to one, a sample standing a
    // hair BEFORE the corner has a distance of a hair below nothing, and one is
    // added to it to bring it round. At 64 bits a hair below nothing plus one
    // rounds to EXACTLY one, and exactly one is then read as a hair AFTER the
    // corner rather than before it. The two sides of the corner are moved in
    // opposite directions, thus the sample was moved by one the wrong way and
    // the wave jumped by two. Measured on a pulse at 100 Hz in 8000 with a part
    // of an eighth: sample 10 came out at 2.0 where the whole shape stands
    // between -1 and 1. The square wave and the sawtooth ran the same risk and
    // were saved only by which numbers their corners happened to land on.
    //
    // Held either side, a sample before the corner keeps a distance below
    // nothing and there is no adding and nothing to round.
    real_t past = phase - corner;

    while(past < -REAL_C(0.5))
    {
        past += REAL_C(1.0);
    }

    while(past >= REAL_C(0.5))
    {
        past -= REAL_C(1.0);
    }

    if(REAL_ABS(past) < step)
    {
        return generate_corner(past / step);
    }

    return REAL_C(0.0);
}

// A sawtooth, rising from -1 to 1 across the turn, with its one corner
// smoothed.
static real_t generate_sawtooth_at(real_t phase, real_t step)
{
    real_t value = (REAL_C(2.0) * phase) - REAL_C(1.0);

    // The corner stands at the start of the turn, and the wave falls there.
    return value - generate_corner_at(phase, REAL_C(0.0), step);
}

// A square wave, with both of its corners smoothed. It rises at the start of
// the turn and falls in the middle of it.
static real_t generate_square_at(real_t phase, real_t step)
{
    real_t value = (phase < REAL_C(0.5)) ? REAL_C(1.0) : -REAL_C(1.0);

    value += generate_corner_at(phase, REAL_C(0.0), step);
    value -= generate_corner_at(phase, REAL_C(0.5), step);

    return value;
}

// What the difference of two pink values is multiplied by, so that the blue
// noise comes out about as loud as the white noise does.
//
// MEASURED AND NOT WORKED OUT. Taking a difference changes the loudness by an
// amount that depends on how the power is spread across the band, and the pink
// noise here is made of seven running parts rather than by an exact filter.
// The number below is what brought the standard deviation of the blue noise to
// the standard deviation of the white noise over a million samples.
#define GENERATE_BLUE_GAIN      REAL_C(1.33)

// The running part of the pink noise, worked out and kept.
//
// Each part is renewed half as often as the one before it: the first at every
// sample, the second at every second sample, and so on. Added together they
// hold twice the power in each halving of frequency, which is what most
// natural noise does.
static real_t generate_next_pink(generate_t* generate)
{
    real_t value = REAL_C(0.0);
    uint32_t counted = generate->counted;

    for(uint32_t part = 0; part < GENERATE_PINK_PARTS; part++)
    {
        uint32_t every = (uint32_t)1u << part;

        if((counted % every) == 0u)
        {
            generate->pink[part] = generate_next_random(generate);
        }

        value += generate->pink[part];
    }

    // Brought back to about the same size as the white noise.
    return value / (real_t)GENERATE_PINK_PARTS;
}

// A draw from a normal spread with a standard deviation of one.
//
// The method is the one Box and Muller published: two evenly spread values
// turn into two normally spread ones, thus the second is kept and given out at
// the next call rather than thrown away.
//
// AN EVEN SPREAD PUT THROUGH A LOGARITHM IS WHAT MAKES THE TAILS RIGHT. Adding
// a dozen even draws together also gives something bell shaped, and it is the
// usual shortcut, but the sum of a dozen bounded things is itself bounded and
// it has no tails at all past about four standard deviations. The tails are
// the whole reason this kind exists: a rate of false alarms of one in a
// million is a question about what happens past five.
static real_t generate_next_normal(generate_t* generate)
{
    if(generate->has_spare)
    {
        generate->has_spare = false;

        return generate->spare;
    }

    // generate_next_random gives values from -1 to 1 and the logarithm needs
    // one above nothing, thus the first is folded into the range from just
    // above 0 to 1.
    real_t first = (generate_next_random(generate) + REAL_C(1.0))
                   / REAL_C(2.0);
    real_t second = generate_next_random(generate);

    if(first <= REAL_SMALLEST)
    {
        first = REAL_SMALLEST;
    }

    real_t how_far = REAL_SQRT(REAL_C(-2.0) * REAL_LOG(first));
    real_t which_way = GENERATE_PI * second;

    generate->spare = how_far * REAL_SIN(which_way);
    generate->has_spare = true;

    return how_far * REAL_COS(which_way);
}

// A rectangular pulse that is high for the given part of the turn, with both
// of its corners smoothed. The square wave is this with a part of a half.
static real_t generate_pulse_at(real_t phase, real_t part, real_t step)
{
    real_t value = (phase < part) ? REAL_C(1.0) : -REAL_C(1.0);

    value += generate_corner_at(phase, REAL_C(0.0), step);
    value -= generate_corner_at(phase, part, step);

    return value;
}

real_t generate_sample(generate_t* generate)
{
    ASSERT(generate != NULL);

    if(!generate->designed)
    {
        return REAL_C(0.0);
    }

    real_t value = REAL_C(0.0);

    switch(generate->kind)
    {
        case GENERATE_SINE:
            value = REAL_SIN(REAL_C(2.0) * GENERATE_PI * generate->phase);
            break;

        case GENERATE_SQUARE:
            value = generate_square_at(generate->phase, generate->step);
            break;

        case GENERATE_SAWTOOTH:
            value = generate_sawtooth_at(generate->phase, generate->step);
            break;

        case GENERATE_TRIANGLE:
        {
            // A triangle has no step of its own, only a change of slope, thus
            // it needs no smoothing: it rises across half the turn and falls
            // across the other half.
            real_t rising = (generate->phase < REAL_C(0.5))
                            ? (REAL_C(4.0) * generate->phase) - REAL_C(1.0)
                            : REAL_C(3.0) - (REAL_C(4.0) * generate->phase);

            value = rising;
            break;
        }

        case GENERATE_WHITE_NOISE:
            value = generate_next_random(generate);
            break;

        case GENERATE_PINK_NOISE:
            value = generate_next_pink(generate);
            break;

        case GENERATE_BROWN_NOISE:
        {
            // A running sum of random values, with slightly less than all of
            // it kept at each sample so that the wander stays bounded.
            //
            // WHAT IS ADDED IS SCALED BY THE ROOT OF WHAT IS NOT KEPT, which
            // is what leaves the spread of the answer the same as the spread
            // of the white noise it is made from. Without it the sum of a
            // thousand samples is about twenty times as loud as the noise that
            // built it, and a caller who scaled every kind alike would find
            // this one drowning the others.
            real_t adding = REAL_SQRT(REAL_C(1.0)
                                      - (GENERATE_BROWN_KEEP
                                         * GENERATE_BROWN_KEEP));

            generate->running = (GENERATE_BROWN_KEEP * generate->running)
                                + (adding * generate_next_random(generate));

            value = generate->running;
            break;
        }

        case GENERATE_BLUE_NOISE:
        {
            // THE MIRROR OF PINK, MADE FROM PINK. Taking the difference of one
            // sample and the one before it lifts the answer by six decibels
            // for each doubling of frequency. Pink falls by three, thus the
            // difference of pink rises by three, which is blue. Taking the
            // difference of WHITE would rise by six, which is violet and not
            // what was asked for.
            real_t now = generate_next_pink(generate);

            value = (now - generate->last_pink) * GENERATE_BLUE_GAIN;
            generate->last_pink = now;
            break;
        }

        case GENERATE_GAUSSIAN_NOISE:
            value = generate_next_normal(generate);
            break;

        case GENERATE_PULSE:
            value = generate_pulse_at(generate->phase, generate->part,
                                      generate->step);
            break;

        case GENERATE_GAUSSIAN_PULSE:
        {
            // A bump standing in the middle of the turn rather than at its
            // start, so that it does not fall across the wrap and have to be
            // worked out from both ends of the turn at once.
            real_t from_middle = (generate->phase - REAL_C(0.5))
                                 / generate->part;

            value = REAL_EXP(-REAL_C(0.5) * from_middle * from_middle);
            break;
        }

        case GENERATE_IMPULSE:
        default:
            // One sample at the start of each turn. The phase moves by one
            // step each sample, thus exactly one sample of each turn stands
            // below one step.
            value = (generate->phase < generate->step) ? REAL_C(1.0)
                                                       : REAL_C(0.0);
            break;
    }

    // The phase is carried and folded, thus it never grows and never loses its
    // digits however long the run.
    if(!generate_is_noise(generate->kind))
    {
        generate->phase += generate->step;

        while(generate->phase >= REAL_C(1.0))
        {
            generate->phase -= REAL_C(1.0);
        }

        while(generate->phase < REAL_C(0.0))
        {
            generate->phase += REAL_C(1.0);
        }

        // A sweep moves the step itself, and stops when it arrives.
        if(generate->sweep != REAL_C(0.0))
        {
            generate->step += generate->sweep;

            bool rising = (generate->sweep > REAL_C(0.0));

            if((rising && (generate->step >= generate->last_step))
               || (!rising && (generate->step <= generate->last_step)))
            {
                generate->step = generate->last_step;
                generate->sweep = REAL_C(0.0);
            }
        }
    }

    generate->counted++;

    return value;
}

bool generate_block(generate_t* generate, real_t* output, uint32_t count)
{
    ASSERT(generate != NULL);
    ASSERT(output != NULL);

    if(!generate->designed)
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        output[index] = generate_sample(generate);
    }

    return true;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int generate_is_not_in_this_build_t;

#endif//FFITT_NO_UTIL
