// Find out whether two signals really have anything to do with each other.
//
// A machine is running and a floor is shaking. Both hold a peak at 50 hertz,
// and the obvious conclusion is that the machine is shaking the floor. THE
// CONCLUSION DOES NOT FOLLOW. Half the building holds a peak at 50 hertz,
// because half the building is plugged into the mains. Two peaks at the same
// frequency say nothing about whether one causes the other.
//
// Coherence answers the question that was really being asked: at this
// frequency, does one signal EXPLAIN the other? It reads from 0 to 1.
//
// THE TRAP THAT MAKES THE ANSWER MEANINGLESS, AND IT CATCHES EVERYONE
//
// A single block gives a coherence of exactly 1 at every frequency, for any
// two signals whatever. Two recordings of unrelated noise read 1.00 across the
// board. It is not a rounding matter and no wider number fixes it: with one
// block the arithmetic reduces to a number divided by itself.
//
// What coherence really measures is whether the relation HOLDS STILL from
// block to block, and one block has nothing to hold still against. This
// example runs the same unrelated signals at rising numbers of blocks so that
// the false answer can be watched falling away.
//
// TO PORT THIS: replace the three signals with your own two recordings, taken
// AT THE SAME MOMENTS, and keep the rule at the end of the output.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_COHERENCE_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/csd.h>
#include <ffitt/transform/window.h>
#include <math.h>
#include <stdio.h>

#define RATE        REAL_C(1000.0)
#define BLOCK       64u
#define BINS        ((BLOCK / 2u) + 1u)
#define SAMPLES     4096u

static real_t machine[SAMPLES];
static real_t floor_of_the_room[SAMPLES];
static real_t unrelated[SAMPLES];
static real_t coherence[BINS];

static uint32_t seed = 987654321u;

static real_t noise(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

// Three signals. The machine and the floor share a real 50 hertz shaking; the
// third is a SECOND SOURCE at 53 hertz, which is a different machine
// altogether.
//
// 53 hertz and 50 hertz fall in the SAME BIN at this block, thus a spectrum of
// either signal shows one peak in the same place and cannot separate them at
// all. What differs is the phase: two sources three hertz apart slip against
// each other, and after a few blocks they stand in a wholly different relation.
// Coherence is exactly the measure of whether that relation holds still, and
// that is why it can tell these apart when a spectrum cannot.
//
// A FIXED PHASE OFFSET WOULD NOT DO. A 50 hertz tone shifted by a constant is
// perfectly coherent with the machine, and rightly: a constant shift is what a
// wall or a cable does, and the two really are related through it.
static void make_the_signals(void)
{
    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t angle = (REAL_C(2.0) * REAL_PI * REAL_C(50.0) * (real_t)index)
                       / RATE;
        real_t other = (REAL_C(2.0) * REAL_PI * REAL_C(53.0) * (real_t)index)
                       / RATE;
        real_t shaking = REAL_SIN(angle);

        machine[index] = shaking + noise();

        // The floor feels the same shaking, a little weaker, plus its own
        // noise from everything else in the building.
        floor_of_the_room[index] = (REAL_C(0.4) * shaking) + noise();

        // The other machine, in the same bin and unrelated.
        unrelated[index] = REAL_SIN(other) + noise();
    }
}

static real_t reading_at_fifty_hertz(csd_t* csd, const real_t* first,
                                     const real_t* second, uint32_t size)
{
    if(!csd_coherence(csd, first, second, size, coherence))
    {
        return -REAL_C(1.0);
    }

    // Bin 50/1000 of 64 samples, which stands at 50 hertz exactly.
    uint32_t bin = (uint32_t)((REAL_C(50.0) * (real_t)BLOCK) / RATE);

    return coherence[bin];
}

int main(void)
{
    csd_t csd = csd_alloc(BLOCK);

    make_the_signals();
    csd_design(&csd, BLOCK / 2u, WINDOW_HANN, REAL_C(0.0));

    printf("Three recordings. The machine shakes at 50 Hz, the floor feels\n"
           "it, and a second machine nearby runs at 53 Hz.\n");
    printf("At a block of %u samples the bins stand %.1f Hz apart, thus 50 Hz\n"
           "and 53 Hz fall in the SAME BIN and no spectrum can tell them\n"
           "apart.\n\n", BLOCK, (double)(RATE / (real_t)BLOCK));

    printf("Coherence at 50 Hz, against the number of blocks averaged:\n\n");
    printf("  %8s  %22s  %22s\n", "blocks", "machine and floor",
           "machine and the other");

    for(uint32_t blocks = 1u; blocks <= 64u; blocks *= 2u)
    {
        // A hop of half the block, thus this many samples give this many
        // blocks.
        uint32_t size = ((blocks - 1u) * (BLOCK / 2u)) + BLOCK;

        real_t real_pair = reading_at_fifty_hertz(&csd, machine,
                                                  floor_of_the_room, size);
        real_t false_pair = reading_at_fifty_hertz(&csd, machine, unrelated,
                                                   size);

        if(real_pair < REAL_C(0.0))
        {
            printf("  %8u  %22s  %22s\n", blocks, "REFUSED", "REFUSED");
        }
        else
        {
            printf("  %8u  %22.2f  %22.2f\n", blocks, (double)real_pair,
                   (double)false_pair);
        }
    }

    printf("\nBelow %u blocks the module refuses, and that is the whole\n"
           "point: at one block BOTH pairs read exactly 1.00, because the\n"
           "arithmetic there is a number divided by itself. An answer of 1\n"
           "from one block is not a strong result. It is no result.\n",
           CSD_SMALLEST_BLOCK_COUNT);
    printf("\nAs the blocks are averaged, the real pair holds its reading and\n"
           "the second machine falls away, though both peaks sit in the same\n"
           "bin and no spectrum can separate them. THAT IS THE MEASUREMENT.\n");
    printf("\nThe rule to carry away: two unrelated signals read about 1\n"
           "divided by the number of blocks. Believe a reading only when it\n"
           "stands well above that.\n");

    csd_free(&csd);

    return 0;
}

#endif
