#ifndef __DEF_H__
#define __DEF_H__

#include <stdlib.h>
#include <stdio.h>

#ifndef TEST
#include <assert.h>
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif//TEST

#endif//__DEF_H__