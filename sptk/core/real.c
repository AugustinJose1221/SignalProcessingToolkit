#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

#include <math.h>

// Each one is the macro of the header with a name and an address. The macro
// does the work, thus these agree with it by construction and cannot drift
// away from it.

real_t real_sin(real_t x)
{
    return REAL_SIN(x);
}

real_t real_cos(real_t x)
{
    return REAL_COS(x);
}

real_t real_tan(real_t x)
{
    return REAL_TAN(x);
}

real_t real_sqrt(real_t x)
{
    return REAL_SQRT(x);
}

real_t real_exp(real_t x)
{
    return REAL_EXP(x);
}

real_t real_log(real_t x)
{
    return REAL_LOG(x);
}

real_t real_abs(real_t x)
{
    return REAL_ABS(x);
}
