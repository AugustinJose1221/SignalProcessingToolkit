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


## stats

Measures of a list of samples. Two kinds stand here, and the difference decides
which one a piece of work needs.

**The plain measures** are the mean, the variance, the deviation, the root mean
square, the smallest and the largest. Each reads the list once and changes
nothing. Each also follows every sample, and that is their weakness: one bad
sample moves them all, and it moves the deviation most, because the deviation
squares the distance.

**The robust measures** are the median, the percentile and the median absolute
deviation. These follow the middle of the list and not its edges. Half of the
samples may be wrong before the median moves at all. They cost more, and
`stats_median` and `stats_percentile` reorder the list that the caller gives,
because they must put it in order and they take no memory of their own.
`stats_mad` takes a work list instead and leaves the data as it was.

**When the robust ones matter.** Take them to set a threshold from live data.
A detector that puts its threshold a few deviations above the mean is undone by
the first spike: the spike raises the threshold that was meant to catch it, and
the detector then sees nothing. The median absolute deviation answers that. For
samples that follow a normal spread it estimates the same number as the
deviation, but a spike does not move it. Multiply by `STATS_MAD_TO_DEVIATION`
to get a number that stands beside a deviation.

**Why the median is fast.** `stats_median` does not sort the list. It uses the
select of Hoare, which is the quick sort with one half left out: after each
split only the half that holds the middle is worked on again. That costs about
one pass over the list instead of one pass for each level. The pivot is the
middle sample and not the first one, because a list that is already in order is
a common input and would be the worst case otherwise.

**What the width of `real_t` costs here.** Every sum runs at the width of the
build, and a sum is where the digits run out first. Five samples that sit at
eight million and move by one have a variance of exactly 2:

| build | answer | |
| --- | --- | --- |
| 32 bits | 2.25 | out by an eighth |
| 64 bits | 2.00 | right |

The fault is in the sum and not in the squaring: five samples near eight
million give a total near forty million, where one step of a float is 4. A
caller whose readings sit far from zero should build in 64 bits, or take the
level away first with `dcblock`. The tests hold both numbers.
