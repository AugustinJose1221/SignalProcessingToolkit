#ifndef TEST
#include <sptk/filter/lattice.h>
#include <sptk/core/defs.h>
#else
#include "lattice.h"
#include "defs.h"
#endif

#include <math.h>
#include <stdlib.h>

bool lattice_is_valid_rate(real_t rate)
{
    return (rate > REAL_C(0.0)) && (rate <= LATTICE_LARGEST_RATE);
}

bool lattice_is_valid_forgetting(real_t forgetting)
{
    return (forgetting > REAL_C(0.0)) && (forgetting <= REAL_C(1.0));
}

// Each list holds one value for each stage and one more for what comes out of
// the last of them.
static uint32_t lattice_room(uint32_t stages)
{
    return stages + 1u;
}

lattice_t lattice_alloc(uint32_t stages)
{
    ASSERT(stages > 0u);

    lattice_t lattice;
    uint32_t room = lattice_room(stages);

    lattice.stages = stages;
    lattice.reflection = (real_t*)calloc(room, sizeof(real_t));
    lattice.forward = (real_t*)calloc(room, sizeof(real_t));
    lattice.backward = (real_t*)calloc(room, sizeof(real_t));
    lattice.held = (real_t*)calloc(room, sizeof(real_t));
    lattice.energy = (real_t*)calloc(room, sizeof(real_t));
    lattice.weight = (real_t*)calloc(room, sizeof(real_t));
    lattice.rate = REAL_C(0.5);
    lattice.forgetting = REAL_C(0.99);
    lattice.designed = false;
    lattice.dynamic_alloc = true;

    if((lattice.reflection == NULL) || (lattice.forward == NULL)
       || (lattice.backward == NULL) || (lattice.held == NULL)
       || (lattice.energy == NULL) || (lattice.weight == NULL))
    {
        lattice_free(&lattice);
        return lattice;
    }

    lattice_design(&lattice, REAL_C(0.5), REAL_C(0.99));

    return lattice;
}

lattice_t lattice_static_alloc(uint32_t stages, real_t* reflection,
                               real_t* forward, real_t* backward,
                               real_t* held, real_t* energy, real_t* weight)
{
    ASSERT(stages > 0u);
    ASSERT(reflection != NULL);
    ASSERT(forward != NULL);
    ASSERT(backward != NULL);
    ASSERT(held != NULL);
    ASSERT(energy != NULL);
    ASSERT(weight != NULL);

    lattice_t lattice;

    lattice.stages = stages;
    lattice.reflection = reflection;
    lattice.forward = forward;
    lattice.backward = backward;
    lattice.held = held;
    lattice.energy = energy;
    lattice.weight = weight;
    lattice.rate = REAL_C(0.5);
    lattice.forgetting = REAL_C(0.99);
    lattice.designed = false;
    lattice.dynamic_alloc = false;

    lattice_design(&lattice, REAL_C(0.5), REAL_C(0.99));

    return lattice;
}

void lattice_reset(lattice_t* lattice)
{
    ASSERT(lattice != NULL);

    uint32_t room = lattice_room(lattice->stages);

    for(uint32_t index = 0; index < room; index++)
    {
        lattice->reflection[index] = REAL_C(0.0);
        lattice->forward[index] = REAL_C(0.0);
        lattice->backward[index] = REAL_C(0.0);
        lattice->held[index] = REAL_C(0.0);
        lattice->weight[index] = REAL_C(0.0);
        lattice->before = REAL_C(0.0);
        lattice->after = REAL_C(0.0);

        // The energies begin at the floor rather than at nothing, so that the
        // first sample cannot divide by zero.
        lattice->energy[index] = LATTICE_FLOOR;
    }
}

bool lattice_design(lattice_t* lattice, real_t rate, real_t forgetting)
{
    ASSERT(lattice != NULL);

    if(!lattice_is_valid_rate(rate)
       || !lattice_is_valid_forgetting(forgetting))
    {
        return false;
    }

    lattice->rate = rate;
    lattice->forgetting = forgetting;
    lattice->designed = true;

    lattice_reset(lattice);

    return true;
}

real_t lattice_get_reflection(const lattice_t* lattice, uint32_t stage)
{
    ASSERT(lattice != NULL);
    ASSERT(stage < lattice->stages);

    return lattice->reflection[stage];
}

real_t lattice_error_before(const lattice_t* lattice)
{
    ASSERT(lattice != NULL);

    return lattice->before;
}

real_t lattice_error_after(const lattice_t* lattice)
{
    ASSERT(lattice != NULL);

    return lattice->after;
}

real_t lattice_process_sample(lattice_t* lattice, real_t reference,
                              real_t wanted)
{
    ASSERT(lattice != NULL);

    if(!lattice->designed)
    {
        return REAL_C(0.0);
    }

    uint32_t stages = lattice->stages;

    // The sample enters the first stage both ways round.
    lattice->forward[0] = reference;
    lattice->backward[0] = reference;

    for(uint32_t stage = 0; stage < stages; stage++)
    {
        // What comes forward at this stage, and what the stage before it held
        // back from the sample before.
        real_t came_forward = lattice->forward[stage];
        real_t was_held = lattice->held[stage];

        // How loud this stage has been, faded so that it follows the signal
        // now rather than the signal of an hour ago.
        //
        // THIS IS A RUNNING SUM AND NOT A RUNNING MEAN, and the difference is
        // not a detail. A sum over a fading past is about 1/(1-factor) times
        // the mean, thus at a factor of 0.99 it is a hundred times larger. The
        // steps below are divided by it, so a sum makes every step a hundredth
        // of what a mean would make it. Written with a mean, the steps are so
        // large that the stages chase each other and never settle at all.
        real_t loudness = (lattice->forgetting * lattice->energy[stage])
                          + (came_forward * came_forward)
                          + (was_held * was_held);

        if(loudness < LATTICE_FLOOR)
        {
            loudness = LATTICE_FLOOR;
        }

        lattice->energy[stage] = loudness;

        real_t reflection = lattice->reflection[stage];

        // What neither this stage nor the one before it could explain, carried
        // on to the next. The two ways round are what makes a ladder a ladder:
        // one runs forward through the signal and one runs back.
        lattice->forward[stage + 1u] = came_forward - (reflection * was_held);
        lattice->backward[stage + 1u] = was_held - (reflection * came_forward);

        // The stage moves its own number towards whatever the two still hold
        // in common. DIVIDING BY THE LOUDNESS IS WHAT LETS EVERY STAGE LEARN
        // AT ITS OWN PACE, and that is the whole reason a ladder is quicker
        // than a straight filter on an input that leans on itself.
        real_t together = (lattice->forward[stage + 1u] * was_held)
                          + (lattice->backward[stage + 1u] * came_forward);

        real_t moved = reflection + ((lattice->rate * together) / loudness);

        // A reflection number outside this range describes a stage that gives
        // out more than it was given, which no stage of a ladder can do. The
        // arithmetic holds it rather than trusting it, and that is why a
        // ladder cannot run away as an rls filter can.
        if(moved > LATTICE_LARGEST_REFLECTION)
        {
            moved = LATTICE_LARGEST_REFLECTION;
        }

        if(moved < -LATTICE_LARGEST_REFLECTION)
        {
            moved = -LATTICE_LARGEST_REFLECTION;
        }

        lattice->reflection[stage] = moved;
    }

    // WHAT THE LADDER SAYS, BEFORE ANY WEIGHT HAS LEARNED FROM THIS SAMPLE.
    // Each stage gives out what it could not explain, and the weights say how
    // much of each to take.
    real_t guess = REAL_C(0.0);

    for(uint32_t stage = 0; stage <= stages; stage++)
    {
        guess += lattice->weight[stage] * lattice->backward[stage];
    }

    lattice->before = wanted - guess;

    // Every weight moves by how loudly its stage was heard and by how far the
    // answer was out. Dividing by the loudness again is what keeps the quiet
    // stages learning as fast as the loud ones.
    // The energies above are running SUMS, which is what the reflection steps
    // want. A weight is a plain amount of its stage and wants the running MEAN
    // instead, which is the sum multiplied by one less the forgetting factor.
    // Dividing a weight by the sum makes its step a hundredth of what it
    // should be, and the ladder then settles more slowly than a straight
    // filter, which is the opposite of the point.
    // AND THE RATE IS SHARED AMONG THE STAGES. Each stage is normalised on its
    // own here, thus every one of them takes a step of the size the rate asks
    // for and the steps add. A ladder of twelve stages at a rate of 0.5 would
    // move six times as far as a plain normalised filter at that rate, which
    // no filter is stable at. Sharing the rate keeps the whole step the size
    // the caller asked for, whatever the number of stages.
    real_t to_mean = REAL_C(1.0) - lattice->forgetting;
    real_t shared = lattice->rate / (real_t)(stages + 1u);

    for(uint32_t stage = 0; stage <= stages; stage++)
    {
        real_t heard = lattice->backward[stage];
        real_t loudness = (stage < stages) ? lattice->energy[stage]
                                           : lattice->energy[stages - 1u];

        real_t against = loudness * to_mean;

        // AND THE STEP IS HELD SO THAT IT CANNOT OVERSHOOT.
        //
        // A fading sum has not accumulated at the start of a run, thus the
        // mean taken from it is far too small there and the first steps are
        // enormous: measured, they carried the answer past the mark and left
        // MORE error after learning than before it, which is a thing that
        // should never happen.
        //
        // Holding what is divided by to at least the loudness of this sample
        // keeps each step at or below the rate it was given, whatever the
        // energies have had time to gather.
        if(against < (heard * heard))
        {
            against = heard * heard;
        }

        lattice->weight[stage] += (shared * lattice->before * heard)
                                  / (against + LATTICE_FLOOR);
    }

    // AND WHAT IS LEFT AFTER THE SAMPLE HAS BEEN LEARNED FROM, worked out with
    // the weights as they now stand. It is always the smaller of the two, thus
    // it must never be reported as how well the filter is doing.
    real_t settled = REAL_C(0.0);

    for(uint32_t stage = 0; stage <= stages; stage++)
    {
        settled += lattice->weight[stage] * lattice->backward[stage];
    }

    lattice->after = wanted - settled;

    // What each stage held back this time is what it gives the next sample.
    for(uint32_t stage = 0; stage <= stages; stage++)
    {
        lattice->held[stage] = lattice->backward[stage];
    }

    return lattice->before;
}

void lattice_free(lattice_t* lattice)
{
    ASSERT(lattice != NULL);

    if(lattice->dynamic_alloc)
    {
        free(lattice->reflection);
        free(lattice->forward);
        free(lattice->backward);
        free(lattice->held);
        free(lattice->energy);
        free(lattice->weight);
    }

    lattice->reflection = NULL;
    lattice->forward = NULL;
    lattice->backward = NULL;
    lattice->held = NULL;
    lattice->energy = NULL;
    lattice->weight = NULL;
    lattice->stages = 0u;
    lattice->designed = false;
}
