#ifndef REAL_ASSERT_H
#define REAL_ASSERT_H

#include "unity.h"
#include "real.h"

// Comparing two values of the type real_t in a test.
//
// Unity has one set of assertions for a float and another for a double, and
// they are not the same: the ones for a float bring both values down to seven
// digits before they compare them. A test in 64 bit mode that used the float
// ones would throw away the very accuracy it was written to examine, and would
// pass whether the library held sixteen digits or seven.
//
// These macros stand for the pair that fits the build, thus one test serves
// both widths and each one measures what that width really gives.

#if defined(FFITT_REAL_64)

#define TEST_ASSERT_REAL_WITHIN(delta, expected, actual) \
    TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)

#define TEST_ASSERT_EQUAL_REAL(expected, actual) \
    TEST_ASSERT_EQUAL_DOUBLE(expected, actual)

#else

#define TEST_ASSERT_REAL_WITHIN(delta, expected, actual) \
    TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)

#define TEST_ASSERT_EQUAL_REAL(expected, actual) \
    TEST_ASSERT_EQUAL_FLOAT(expected, actual)

#endif//FFITT_REAL_64

#endif//REAL_ASSERT_H
