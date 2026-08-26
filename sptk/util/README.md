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


## Finding the peaks that are real

`peakdetect_get_peaks` gives every local maximum, which is what `emd` wants and
what a caller wants when the signal is already clean.

**On real data that is not enough.** Noise puts a local maximum every few
samples: a recording of a heart at 500 samples in a second holds about a
hundred of them for every beat. A test in this repository builds exactly that
signal and finds **over 100** local maxima where there are **10** beats.

`peakdetect_find` takes four rules, and they are not equal.

**Prominence is the one to reach for.** It asks: how far must you descend from
this peak before you can climb to a higher one? A wobble on the side of a large
peak has almost no prominence however high it stands. A test holds this
directly: a wobble standing at 7.9 has a prominence of 0.2, while a lone peak
standing at only 6.0 has a prominence of 5.0. **A height rule keeps the wrong
one of those two.** Prominence also does not care where the signal sits, thus a
drifting level does not defeat it.

**Height** is the obvious rule and the weakest, for the reason above.
**Width** throws away what is too narrow to be real: a spike one sample wide is
noise, a heartbeat is thirty samples wide. **Distance** says no two peaks may
stand closer than so many samples, keeping the taller — a heart cannot beat
twice in 200 ms.

With prominence and distance together, the same test finds exactly the 10
beats, each within three samples of where it was put.

**A flat top counts as one peak.** This matters on real data: a reading from a
converter is a whole number of counts, thus the top of a peak is often two or
three samples of exactly the same value. A test of "larger than both
neighbours" finds no peak there at all, and a test in this repository shows
`peakdetect_get_peaks` returning zero on such a peak while `peakdetect_find`
returns one, at the middle of the flat part.

**A valley is a peak of the signal turned upside down.** There is no separate
set of these rules for valleys; negate the signal and use these.

## generate

**Every test and example in this library used to write its own sine wave.** That
is fine for a sine and a trap for anything else.

**A square wave is not a row of ones and minus ones.** Written the obvious way,
by taking the sign of a sine, it holds every odd harmonic out to infinity — and a
sampled signal cannot hold anything above half the sample rate, so every harmonic
above that **folds back** and lands somewhere below it, at a frequency that has
nothing to do with the note being played.

Measured at 8000 samples in a second: the loudest thing in the answer that is
**not** a harmonic of the tone, against the tone itself.

| tone Hz | 100 | 300 | 700 | 1300 | 1900 | 3100 |
|---|---|---|---|---|---|---|
| samples a turn | 80 | 27 | 11 | 6.2 | 4.2 | 2.6 |
| naive | -39.3 | -23.9 | -17.3 | -13.9 | -9.2 | -9.2 |
| `generate` | -49.3 | -33.7 | -29.6 | -39.6 | -25.7 | -39.6 |

Read the naive row across. The fewer samples to a turn, the worse it gets, until
at 1900 Hz the loudest false tone is only 9 dB below the one that was asked for.
**A filter tested with that wave is being tested against a signal nobody meant to
make.**

**It does not remove the folding altogether**, and the table is honest about
that: the best it reaches is about 50 dB down and the worst about 26. Nothing
that runs in constant time does better. A test that needs better than that wants
a sine, which folds nothing because it holds one frequency and no other.

**The phase is carried, not worked out from the sample number.** Working out
`sin(2πfn/rate)` from `n` goes wrong in two ways: the angle grows without bound
so a long run loses its digits, and a frequency that changes cannot be written
that way at all without a jump. Carrying and folding the phase costs nothing and
allows `generate_design_sweep` — a chirp visits every frequency in one run, and
one chirp through a filter shows the whole of what the filter does.

**The same seed gives the same noise**, on every machine and at either width. A
test that cannot be repeated is not a test.

**A note on Unity's assertions.** The macros use what they are given more than
once for the *expected* argument. A call that moves a generator on must be stored
in a local first, or the two sides fall out of step with each other rather than
with the test.

## quantise

**Every signal in a device has been through this.** A converter of 12 bits holds
4096 steps and nothing between them, and what falls between two steps has to go
to one of them.

**The error is the same size whatever is done.** Nothing here makes it smaller.
What this module chooses is **what shape it takes**, and that decides whether it
can be got rid of afterwards.

Measured, a sine of 300 Hz at a hundredth of full scale into 8 bits at 8000
samples a second, everything against the sine:

| | worst false tone | noise below 1 kHz | noise above it |
|---|---|---|---|
| rounded plainly | -15.6 dB | -15.2 dB | -8.0 dB |
| with dither | -30.9 dB | -7.6 dB | -2.9 dB |
| with dither and shape | -25.4 dB | -14.2 dB | +1.2 dB |

**Read it a column at a time.** Plain rounding leaves a false tone only 15.6 dB
below the signal — a harmonic of it, which **no amount of averaging removes**,
because it is not noise but a signal. Dither takes 15 dB off that and leaves
noise in its place, and noise averages away.

The second column is what the dither costs: the noise below 1 kHz rises from
−15.2 to −7.6. The third row is why shaping exists — it takes that back down to
−14.2, nearly where plain rounding had it, and pays for it above 1 kHz where the
noise rises to +1.2.

**The noise has not gone anywhere.** It has been moved out of the band the signal
is in. A signal that fills the whole band gains nothing from shaping and loses a
little; a signal that sits low down, which most do, gains the whole 6.6 dB.

**A signal beyond the reach is held, never wrapped.** A signal that wraps does
not sound loud, it sounds broken, and one sample of it can undo a whole
measurement.
