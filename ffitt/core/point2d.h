#ifndef POINT2D_H
#define POINT2D_H

#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif
// A point on a plane.
typedef struct{
    real_t x;
    real_t y;
}point2d_t;

#endif//POINT2D_H
