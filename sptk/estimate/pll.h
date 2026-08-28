#ifndef PLL_H
#define PLL_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Follow a tone whose frequency will not stay still.
//
// A tachometer gives a tone whose frequency IS the speed. A mains supply sits
// near 50 Hz and wanders. A tag returns a carrier that the motion between the
// two ends has shifted. In each of them the frequency is the measurement, and it
// changes while it is being measured.
//
// A TRANSFORM CANNOT DO THIS, and the reason is not that it is slow. A transform
// reads a block and gives the frequencies in that block. Make the block long
// enough to tell 50.0 Hz from 50.1 and the frequency has moved before the block
// is over; make it short enough to follow the movement and it can no longer tell
// the two apart. That trade is not an implementation and it does not go away.
//
// A loop does not have it. It holds a guess of the frequency and a guess of the
// phase, compares its guess against what arrives, and moves. It gives a NEW
// ANSWER AT EVERY SAMPLE, and how quickly it follows a change is a number the
// caller sets rather than a length it has to choose.
//
// WHAT IS PAID FOR THAT. A loop can be wrong in ways a transform cannot:
//
//   IT MUST BE STARTED NEAR THE ANSWER. Told to look near 50 Hz it will find a
//   tone at 50.4; told to look near 50 Hz it will not find one at 200. How far
//   it will reach is about its bandwidth, and pll_pull_range gives it.
//
//   IT CAN LOCK ONTO SOMETHING ELSE. Given noise and no tone it will settle
//   somewhere and report a frequency with the same confidence as a real one.
//   pll_lock_quality is the only thing that says which happened, and it must be
//   read.
//
//   IT TAKES TIME TO ARRIVE. Of the order of a few divided by the bandwidth in
//   samples, and pll_settle_samples gives a rough figure. The table below gives
//   what was measured, which runs from about two divided by the bandwidth for a
//   narrow loop to about half of that for a wide one.
//
// THE BANDWIDTH IS THE WHOLE OF THE TRADE. Wide follows a change quickly and
// lets noise into the answer; narrow is steady and slow. Measured on a tone at a
// tenth of the sample rate, with noise as loud as the tone for the wander and
// none for the lock:
//
//   bandwidth     wander of the answer     samples to find the tone
//   ---------     --------------------     ------------------------
//   0.0005                    0.001107                         4603
//   0.0010                    0.001540                         1154
//   0.0020                    0.002542                          258
//   0.0050                    0.006196                           57
//   0.0100                    0.012724                           42
//
// READ IT ACROSS. Twenty times the bandwidth finds the tone a hundred times as
// fast and wanders eleven times as far. Neither end is right; which one is
// depends on how fast the thing being watched really moves and how much noise
// is on it.
//
// AND THE BANDWIDTH MUST STAY WELL BELOW THE FREQUENCY BEING FOLLOWED. The
// detector gives the error it wants and a ripple at twice the tone on top of it,
// and the loop leans on being too slow to follow that ripple. At a bandwidth
// near the tone it follows the ripple instead and the answer shakes.
//
// THE LOOP MEASURES HOW LOUD THE SIGNAL IS AND DIVIDES BY IT. Without that the
// gain of the loop would be the gain the caller set MULTIPLIED BY the loudness
// of whatever arrived, thus a quiet tone would never lock and a loud one would
// be unstable, and the bandwidth would mean nothing.

typedef struct{
    real_t phase;               // Where the loop thinks the tone is, 0 to 1
    real_t step;                // How far that moves each sample
    real_t free_step;           // Where it was told to start looking
    real_t fast;                // What the error is multiplied by
    real_t slow;                // And what the running total of it is
    real_t gathered;            // That running total
    real_t loudness;            // The running loudness of what arrives
    real_t quality;             // The running measure of how well it is locked
    real_t quality_keep;        // How much of that measure is kept each sample
    real_t bandwidth;           // As it was designed
    real_t damping;             // As it was designed
    bool designed;              // True once pll_design has been called
}pll_t;

// How much of the running loudness is kept at each sample. It follows the
// signal rather than the loop and must be far slower than the tone, so that the
// ripple at twice the tone does not get into it.
#ifndef PLL_KEEP
#define PLL_KEEP                REAL_C(0.999)
#endif

// THE LOCK MEASURE FOLLOWS THE LOOP AND NOT A FIXED RATE. Held at a fixed rate
// it would be slower than a wide loop and would still be reporting the last
// answer long after the loop had found a new one. It is held instead at a few
// times the bandwidth, thus a loop that arrives quickly says so quickly.
#define PLL_QUALITY_KEEP(bandwidth) (REAL_C(1.0) - ((bandwidth) / REAL_C(2.0)))

// The smallest loudness the loop will divide by, so that a reading of silence
// does not turn into a division by nothing.
#ifndef PLL_SMALLEST_LOUDNESS
#define PLL_SMALLEST_LOUDNESS   REAL_C(1.0e-9)
#endif

// True if this is a bandwidth the loop can be designed at, as a part of the
// sample rate. It must be above nothing and well below a half.
bool pll_is_valid_bandwidth(real_t bandwidth);

// True if this is a damping the loop can be designed at.
//
// Below about 0.5 the loop rings: it swings past the answer and comes back
// several times before settling. Above about 2 it crawls. 0.707 is the usual
// choice and is what gives the fastest arrival with no overshoot.
bool pll_is_valid_damping(real_t damping);

// Give a loop that is not yet following anything.
pll_t pll_make(void);

// Tell the loop where to start looking and how quickly to follow.
//
// The frequency and the sample rate say where to start; the bandwidth says how
// quickly to follow, as a part of the sample rate; the damping says how it
// behaves on the way. Give 0.707 for the damping where nothing says otherwise.
//
// Give false and leave the loop as it was if the frequency cannot be followed at
// this rate, or if the bandwidth or the damping is refused.
bool pll_design(pll_t* pll, real_t frequency, real_t sample_rate,
                real_t bandwidth, real_t damping);

// Give the loop one sample and take back its own tone at the phase it has
// arrived at. That tone is the carrier recovered: it holds the frequency and the
// phase of what arrived and none of its noise.
real_t pll_process_sample(pll_t* pll, real_t sample);

// Give the frequency the loop is following, at the sample rate it was designed
// at. THIS IS THE MEASUREMENT for a tachometer or a mains watcher.
real_t pll_get_frequency(const pll_t* pll, real_t sample_rate);

// Give where in its turn the loop stands, from 0 to 1.
real_t pll_get_phase(const pll_t* pll);

// Give how well the loop is following what arrived, from 0 to 1.
//
// THIS MUST BE READ. A loop given noise and no tone settles somewhere and
// reports a frequency exactly as confidently as it reports a real one. This is
// the only number that tells the two apart: near 1 the loop is following
// something, and near 0 it is following nothing.
//
// It is the running mean of how well the loop's own tone lines up with what
// arrived, thus it needs a few hundred samples after a change before it means
// anything.
real_t pll_lock_quality(const pll_t* pll);

// Give roughly how far either side of where it was told to look the loop can
// still find a tone, as a part of the sample rate.
real_t pll_pull_range(const pll_t* pll);

// Give roughly how many samples the loop takes to arrive after a change.
uint32_t pll_settle_samples(const pll_t* pll);

// Put the loop back where it was told to start looking, keeping what it was
// designed with.
void pll_reset(pll_t* pll);

#endif//PLL_H
