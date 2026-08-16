# Utilities

Three small modules that the areas above use, and that stand on their own.

## binarysearch

Gives the index of the first value of a list that is not less than the value
you look for. The values of the list must rise. The cost grows with the
logarithm of the size, thus a list of a thousand values needs about ten steps.

**The result is always an index that you can use.** When every value of the
list is less than the value you look for, the search would stop at the size of
the list, which is not an index. A caller that read the list at that place
would read memory after the end of it. The module holds the result below the
size instead.

The cubic spline uses it to find the interval that holds a place.

## peakdetect and valleydetect

A peak is a sample that is larger than the sample before it and larger than the
sample after it. A valley is the same with smaller. Thus the first and the last
sample are never peaks or valleys, and a signal with fewer than three samples
holds neither.

Each function writes the index of every peak into one buffer and the value into
another, and it gives the number that it found. Both buffers must hold room for
as many values as the signal.

This is the simplest rule that finds a peak. It has no threshold and no idea of
how far apart two peaks must be, thus noise on a signal gives many small peaks.
Filter the signal first when that matters, with `sptk/filter`.

The empirical mode decomposition uses both to find the points that its
envelopes pass through.
