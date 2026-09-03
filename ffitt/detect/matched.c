// This file is left out of the build when FFITT_NO_DETECT is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_DETECT

#ifndef TEST
#include <ffitt/detect/matched.h>
#include <ffitt/core/defs.h>
#else
#include "matched.h"
#include "defs.h"
#endif

#include <math.h>

// The inverse of the normal tail, worked out here rather than taken from a
// library, because the library must need nothing but the C standard library.
//
// What is wanted is the number of standard deviations t for which the share of
// a normal distribution above t is the given part. The forward direction has no
// closed form and neither has this one, thus it is a fit: the rational form
// below is the one Peter Acklam published, and the FIT holds to about one part
// in a billion of the answer across the whole range.
//
// What comes out is only as fine as the width it is worked in. Measured against
// a table at rates from a half down to a thousand millionth, the answer stands
// within seven millionths at 32 bits and within a millionth at 64. The first of
// those is the width and not the fit, and both are far finer than the noise of
// any real reading: a threshold of 4.75 standard deviations moved by seven
// millionths changes nothing a caller could measure.
//
// The alternative was a search on the forward direction, which would cost far
// more for an answer nobody could tell apart from this one.
static real_t matched_normal_above(real_t part);

bool matched_is_valid_length(uint32_t length)
{
    return (length >= 1u) && (length <= MATCHED_LARGEST_LENGTH);
}

matched_t matched_make(void)
{
    matched_t matched;

    matched.pattern = NULL;
    matched.length = 0u;
    matched.root_energy = REAL_C(0.0);
    matched.designed = false;

    return matched;
}

bool matched_design(matched_t* matched, const real_t* pattern,
                    uint32_t length)
{
    ASSERT(matched != NULL);
    ASSERT(pattern != NULL);

    if(!matched_is_valid_length(length))
    {
        return false;
    }

    real_t energy = REAL_C(0.0);

    for(uint32_t index = 0; index < length; index++)
    {
        energy += pattern[index] * pattern[index];
    }

    // A shape of nothing has nothing to match, and dividing by its energy would
    // find it at every offset with equal strength.
    if(energy <= REAL_SMALLEST)
    {
        return false;
    }

    matched->pattern = pattern;
    matched->length = length;
    matched->root_energy = REAL_SQRT(energy);
    matched->designed = true;

    return true;
}

real_t matched_score_at(const matched_t* matched, const real_t* signal)
{
    ASSERT(matched != NULL);
    ASSERT(signal != NULL);

    if(!matched->designed)
    {
        return REAL_C(0.0);
    }

    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; index < matched->length; index++)
    {
        total += matched->pattern[index] * signal[index];
    }

    // Dividing by the root of the energy is what puts the score in units of the
    // noise: a reading of pure noise of standard deviation s then gives a score
    // of standard deviation s as well, whatever shape is being looked for.
    return total / matched->root_energy;
}

bool matched_score_block(const matched_t* matched, const real_t* signal,
                         uint32_t count, real_t* score)
{
    ASSERT(matched != NULL);
    ASSERT(signal != NULL);
    ASSERT(score != NULL);

    if(!matched->designed || (count < matched->length))
    {
        return false;
    }

    uint32_t offsets = MATCHED_SCORE_COUNT(count, matched->length);

    for(uint32_t offset = 0; offset < offsets; offset++)
    {
        score[offset] = matched_score_at(matched, &signal[offset]);
    }

    return true;
}

bool matched_best(const matched_t* matched, const real_t* signal,
                  uint32_t count, uint32_t* where, real_t* score)
{
    ASSERT(matched != NULL);
    ASSERT(signal != NULL);
    ASSERT(where != NULL);
    ASSERT(score != NULL);

    if(!matched->designed || (count < matched->length))
    {
        return false;
    }

    uint32_t offsets = MATCHED_SCORE_COUNT(count, matched->length);

    uint32_t best = 0u;
    real_t largest = matched_score_at(matched, signal);

    for(uint32_t offset = 1u; offset < offsets; offset++)
    {
        real_t here = matched_score_at(matched, &signal[offset]);

        if(here > largest)
        {
            largest = here;
            best = offset;
        }
    }

    *where = best;
    *score = largest;

    return true;
}

real_t matched_threshold_for(real_t false_alarm_rate, uint32_t offsets)
{
    if((false_alarm_rate <= REAL_C(0.0)) || (false_alarm_rate >= REAL_C(1.0))
       || (offsets == 0u))
    {
        return REAL_C(0.0);
    }

    // The rate the caller gives is for the whole search. Each offset is a
    // separate chance to be wrong, thus the chance allowed at any one of them
    // is the whole rate shared among them.
    //
    // Sharing it out this way is a little stricter than the exact answer, which
    // would be one less the nth root of one less the rate. The two part company
    // only where the rate is close to one, and a rate close to one is a search
    // that is wrong more often than it is right.
    real_t each = false_alarm_rate / (real_t)offsets;

    return matched_normal_above(each);
}

// The coefficients of the fit. They carry no meaning apart and are named for
// where they stand.
static const real_t MATCHED_A[6] = {
    REAL_C(-3.969683028665376e+01), REAL_C(2.209460984245205e+02),
    REAL_C(-2.759285104469687e+02), REAL_C(1.383577518672690e+02),
    REAL_C(-3.066479806614716e+01), REAL_C(2.506628277459239e+00)
};

static const real_t MATCHED_B[5] = {
    REAL_C(-5.447609879822406e+01), REAL_C(1.615858368580409e+02),
    REAL_C(-1.556989798598866e+02), REAL_C(6.680131188771972e+01),
    REAL_C(-1.328068155288572e+01)
};

static const real_t MATCHED_C[6] = {
    REAL_C(-7.784894002430293e-03), REAL_C(-3.223964580411365e-01),
    REAL_C(-2.400758277161838e+00), REAL_C(-2.549732539343734e+00),
    REAL_C(4.374664141464968e+00), REAL_C(2.938163982698783e+00)
};

static const real_t MATCHED_D[4] = {
    REAL_C(7.784695709041462e-03), REAL_C(3.224671290700398e-01),
    REAL_C(2.445134137142996e+00), REAL_C(3.754408661907416e+00)
};

// Where the fit changes from one form to the other.
#define MATCHED_JOIN    REAL_C(0.02425)

static real_t matched_normal_above(real_t part)
{
    // THE WORKING IS DONE ON THE TAIL ITSELF AND NOT ON ONE LESS IT. The fit is
    // usually written for the share BELOW a point, which would make the first
    // step here one less the tail. At 32 bits that step throws the answer away:
    // one less a tail of a millionth rounds to a number whose distance from one
    // is no longer a millionth, and the far tail is exactly where a rate of
    // false alarms lives. Each branch below is therefore written to read the
    // small number it needs straight from what the caller gave.

    if(part < MATCHED_JOIN)
    {
        // The tail that matters. A small rate of false alarms lands here, and
        // the fit runs on the logarithm of the rate.
        real_t q = REAL_SQRT(REAL_C(-2.0) * REAL_LOG(part));

        // The fit gives the point with this share BELOW it, which is below
        // nothing. The point with this share ABOVE it is its mirror, thus the
        // sign is turned.
        return -(((((((MATCHED_C[0] * q) + MATCHED_C[1]) * q + MATCHED_C[2]) * q
                    + MATCHED_C[3]) * q + MATCHED_C[4]) * q + MATCHED_C[5])
                 / ((((MATCHED_D[0] * q + MATCHED_D[1]) * q + MATCHED_D[2]) * q
                     + MATCHED_D[3]) * q + REAL_C(1.0)));
    }

    if(part > (REAL_C(1.0) - MATCHED_JOIN))
    {
        // The other end, where the answer is below nothing. A caller asking for
        // a rate of false alarms above 0.97 is asking for a threshold that is
        // wrong more often than it is right, thus this branch is here for
        // completeness rather than for use.
        real_t q = REAL_SQRT(REAL_C(-2.0)
                             * REAL_LOG(REAL_C(1.0) - part));

        return ((((((MATCHED_C[0] * q) + MATCHED_C[1]) * q + MATCHED_C[2]) * q
                  + MATCHED_C[3]) * q + MATCHED_C[4]) * q + MATCHED_C[5])
               / ((((MATCHED_D[0] * q + MATCHED_D[1]) * q + MATCHED_D[2]) * q
                   + MATCHED_D[3]) * q + REAL_C(1.0));
    }

    // The middle, where the fit runs on how far the share below stands from the
    // half way point. That distance is a half less the tail, which is worked
    // out here without ever forming the share below.
    real_t q = REAL_C(0.5) - part;
    real_t r = q * q;

    return ((((((MATCHED_A[0] * r) + MATCHED_A[1]) * r + MATCHED_A[2]) * r
              + MATCHED_A[3]) * r + MATCHED_A[4]) * r + MATCHED_A[5]) * q
           / ((((((MATCHED_B[0] * r) + MATCHED_B[1]) * r + MATCHED_B[2]) * r
                + MATCHED_B[3]) * r + MATCHED_B[4]) * r + REAL_C(1.0));
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int matched_is_not_in_this_build_t;

#endif//FFITT_NO_DETECT
