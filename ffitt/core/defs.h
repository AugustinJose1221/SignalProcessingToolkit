#ifndef DEF_H
#define DEF_H

#include <stdlib.h>
#include <stdio.h>

#ifndef TEST
#include <assert.h>
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif//TEST

#endif//DEF_H
