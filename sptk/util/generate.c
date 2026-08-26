#ifndef TEST
#include <sptk/util/generate.h>
#include <sptk/core/defs.h>
#else
#include "generate.h"
#include "defs.h"
#endif

#include <math.h>

#define GENERATE_PI     REAL_C(3.14159265358979323846)

bool generate_is_valid_kind(generate_kind_t kind)
{
    return (kind >= GENERATE_SINE) && (kind <= GENERATE_PINK_NOISE);
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
    return (kind == GENERATE_WHITE_NOISE) || (kind == GENERATE_PINK_NOISE);
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
    generate.counted = 0u;
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

    // How far past the corner this sample stands, held inside one turn so that
    // a corner at the start is reached from both ends of it.
    real_t past = phase - corner;

    if(past < REAL_C(0.0))
    {
        past += REAL_C(1.0);
    }

    if(past < step)
    {
        return generate_corner(past / step);
    }

    if(past > (REAL_C(1.0) - step))
    {
        return generate_corner((past - REAL_C(1.0)) / step);
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
        default:
        {
            // Each part is renewed half as often as the one before it: the
            // first at every sample, the second at every second sample, and so
            // on. Added together they hold twice the power in each halving of
            // frequency, which is what most natural noise does.
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
            value /= (real_t)GENERATE_PINK_PARTS;
            break;
        }
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
