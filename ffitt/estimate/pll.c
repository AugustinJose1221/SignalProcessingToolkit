#ifndef TEST
#include <ffitt/estimate/pll.h>
#include <ffitt/core/defs.h>
#else
#include "pll.h"
#include "defs.h"
#endif

#include <math.h>

#define PLL_PI          REAL_C(3.14159265358979323846)
#define PLL_TWO_PI      REAL_C(6.28318530717958647692)

// The widest bandwidth that leaves the loop slower than the ripple it has to
// ignore. Past this the loop follows its own detector rather than the signal.
#define PLL_LARGEST_BANDWIDTH   REAL_C(0.05)

bool pll_is_valid_bandwidth(real_t bandwidth)
{
    return (bandwidth > REAL_C(0.0))
           && (bandwidth <= PLL_LARGEST_BANDWIDTH);
}

bool pll_is_valid_damping(real_t damping)
{
    return (damping >= REAL_C(0.1)) && (damping <= REAL_C(4.0));
}

pll_t pll_make(void)
{
    pll_t pll;

    pll.free_step = REAL_C(0.0);
    pll.fast = REAL_C(0.0);
    pll.slow = REAL_C(0.0);
    pll.bandwidth = REAL_C(0.0);
    pll.damping = REAL_C(0.0);
    pll.quality_keep = PLL_KEEP;
    pll.designed = false;

    pll_reset(&pll);

    return pll;
}

bool pll_design(pll_t* pll, real_t frequency, real_t sample_rate,
                real_t bandwidth, real_t damping)
{
    ASSERT(pll != NULL);

    if((sample_rate <= REAL_C(0.0)) || (frequency <= REAL_C(0.0))
       || (frequency >= (sample_rate / REAL_C(2.0))))
    {
        return false;
    }

    if(!pll_is_valid_bandwidth(bandwidth) || !pll_is_valid_damping(damping))
    {
        return false;
    }

    pll->free_step = frequency / sample_rate;
    pll->bandwidth = bandwidth;
    pll->damping = damping;

    // The two gains of a loop of the second order, written from the bandwidth
    // and the damping in the usual way. The bandwidth is given as a part of the
    // sample rate, thus it becomes an angle for each sample by a whole turn.
    real_t turn = PLL_TWO_PI * bandwidth;

    // What the error moves the frequency by at once, and what the running total
    // of the error moves it by. The first sets how quickly the loop answers and
    // the second is what lets it hold a frequency that is not the one it was
    // told to look for: without it the loop would need a standing error to
    // stand anywhere but where it started.
    pll->fast = REAL_C(2.0) * damping * turn;
    pll->slow = turn * turn;
    pll->quality_keep = PLL_QUALITY_KEEP(bandwidth);

    pll->designed = true;

    pll_reset(pll);

    return true;
}

void pll_reset(pll_t* pll)
{
    ASSERT(pll != NULL);

    pll->phase = REAL_C(0.0);
    pll->step = pll->free_step;
    pll->gathered = REAL_C(0.0);
    pll->loudness = REAL_C(0.0);
    pll->quality = REAL_C(0.0);
}

real_t pll_process_sample(pll_t* pll, real_t sample)
{
    ASSERT(pll != NULL);

    if(!pll->designed)
    {
        return REAL_C(0.0);
    }

    real_t angle = PLL_TWO_PI * pll->phase;
    real_t mine = REAL_SIN(angle);
    real_t across = REAL_COS(angle);

    // HOW LOUD WHAT ARRIVES IS, followed slowly.
    //
    // Without this the gain of the loop would be the gain the caller asked for
    // MULTIPLIED BY the loudness of whatever arrived. A tone at a tenth of the
    // loudness the loop was designed for would answer at a tenth of the
    // bandwidth and might never arrive at all; one ten times as loud would be
    // unstable. The bandwidth would then be a number about the signal rather
    // than about the loop.
    pll->loudness = (PLL_KEEP * pll->loudness)
                    + ((REAL_C(1.0) - PLL_KEEP) * sample * sample);

    real_t scale = REAL_SQRT((REAL_C(2.0) * pll->loudness)
                             + PLL_SMALLEST_LOUDNESS);

    real_t held = sample / scale;

    // HOW FAR OUT THE LOOP IS, IN TURNS. A tone multiplied by the loop's own
    // tone a quarter turn along gives the difference between the two phases,
    // and a ripple at twice the tone on top of it. The loop is far slower than
    // that ripple and does not follow it, which is why the bandwidth must stay
    // well below the frequency.
    //
    // WHAT THAT MULTIPLICATION GIVES IS NOT THE ERROR ITSELF. For two tones of
    // unit height it comes to half the sine of a whole turn of the error, which
    // for a small error is PI TIMES the error measured in turns. The phase and
    // the step here are both in turns and the two gains below are written for
    // an error in turns, thus the pi is taken out here. Left in, every gain
    // would be pi times what it was asked to be and the bandwidth would mean
    // nothing.
    real_t error = (held * across) / PLL_PI;

    // How well the two line up, followed as slowly as the loudness is. A loop
    // that is following something has its own tone lined up with what arrived,
    // thus this stands near one; a loop following noise has it wandering, thus
    // this stands near nothing.
    pll->quality = (pll->quality_keep * pll->quality)
                   + ((REAL_C(1.0) - pll->quality_keep) * held * mine);

    // The running total is what lets the loop hold a frequency other than the
    // one it started at.
    pll->gathered += pll->slow * error;

    pll->step = pll->free_step + pll->gathered + (pll->fast * error);

    pll->phase += pll->step;

    while(pll->phase >= REAL_C(1.0))
    {
        pll->phase -= REAL_C(1.0);
    }

    while(pll->phase < REAL_C(0.0))
    {
        pll->phase += REAL_C(1.0);
    }

    return mine;
}

real_t pll_get_frequency(const pll_t* pll, real_t sample_rate)
{
    ASSERT(pll != NULL);

    return pll->step * sample_rate;
}

real_t pll_get_phase(const pll_t* pll)
{
    ASSERT(pll != NULL);

    return pll->phase;
}

real_t pll_lock_quality(const pll_t* pll)
{
    ASSERT(pll != NULL);

    // The running total above stands near a half when the loop is following a
    // tone, because a tone multiplied by itself averages to a half. It is
    // doubled here so that the answer runs from nothing to one, and held inside
    // that range because noise can carry it either side.
    real_t found = REAL_C(2.0) * pll->quality;

    if(found < REAL_C(0.0))
    {
        found = -found;
    }

    return (found > REAL_C(1.0)) ? REAL_C(1.0) : found;
}

real_t pll_pull_range(const pll_t* pll)
{
    ASSERT(pll != NULL);

    // About the bandwidth either side, which is what a loop of the second order
    // reaches when it is given time to walk in.
    return pll->bandwidth;
}

uint32_t pll_settle_samples(const pll_t* pll)
{
    ASSERT(pll != NULL);

    if(pll->bandwidth <= REAL_C(0.0))
    {
        return 0u;
    }

    // Two divided by the bandwidth. THE FIGURE IS ROUGH AND THE HEADER SAYS SO:
    // measured, a narrow loop took about two divided by its bandwidth to find a
    // tone and a wide one about half of that. The bandwidth sets the order of
    // the thing and the signal decides the rest.
    return (uint32_t)(REAL_C(2.0) / pll->bandwidth);
}

bool pll_process_block(pll_t* pll, const real_t* input, real_t* output,
                       uint32_t count)
{
    ASSERT(pll != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    if(!pll->designed)
    {
        return false;
    }

    for(uint32_t index = 0; index < count; index++)
    {
        output[index] = pll_process_sample(pll, input[index]);
    }

    return true;
}
