# Filters

A filter takes a band of frequencies away and keeps the rest. The three
modules here reach that goal in three different ways, and each way pays a
different price.

| Module | How it works | Choose it when |
| --- | --- | --- |
| `fir` | A weighted sum of the last few samples | The shape of the signal must stay |
| `iir` | Feeds its own output back into itself | The work for each sample must be small |
| `savgol` | Lays a polynomial through a window | A peak must keep its height |

## fir

A filter with a finite impulse response multiplies the last `n` samples by `n`
coefficients and adds the products. It holds no feedback.

Two things follow from that. It can never run away, because nothing feeds back.
And it moves every frequency by the same time, which is half the length of the
filter. Thus the shape of the signal stays as it is, and a peak arrives later
but not deformed.

The price is the length. A sharp edge between the band that passes and the band
that stops needs many coefficients. A rule of thumb: a filter of about `4/w`
coefficients gives an edge of the width `w`, where `w` is a part of the sample
rate.

**How the design works.** The perfect low pass filter is a sinc curve, which
runs for ever. The design cuts it to the length of the filter, which brings
large waves into the band that stops. A window takes the two ends of the
coefficients down to almost zero and makes those waves small. This module uses
the window of Hamming.

A high pass filter is the whole band less a low pass filter, thus the design
changes the sign of every coefficient and adds one in the middle. A band pass
filter is a wide low pass filter less a narrow one.

`fir_get_gain` says how much of a frequency the filter lets through, without
running a signal through it.

## iir

A filter with an infinite impulse response feeds its output back into itself.
Thus a small number of coefficients gives a sharp edge: one section needs five
multiplications, where a finite filter of the same sharpness needs dozens.

The price is that it moves different frequencies by different times, thus the
shape of the signal changes. And a filter with bad coefficients can run away,
because the feedback can grow.

**How the design works.** The shape of a filter is easy to describe for a
signal that runs on, and this module holds a filter of Butterworth, whose band
that passes is as flat as it can be. The bilinear transform brings that shape
to a signal of samples. It bends the frequency, thus the design first bends the
cutoff the other way. The test of the module examines the result: the gain at
the cutoff must be `1/sqrt(2)` for every order, which is what a filter of
Butterworth gives.

The filter is a chain of biquad sections. Each section holds two poles, thus
the order of the whole filter is two times the number of sections. Each section
keeps its state in the form of Direct Form II transposed, which holds the error
of a float better than the plain form.

## savgol

This filter smooths a signal and keeps its shape. It takes a window of samples,
lays a polynomial through them by the method of the least squares, and gives
the value of that polynomial at the middle of the window.

A plain mean of a window makes a peak lower and wider, because it pulls the
middle down towards its neighbours. A polynomial can follow a peak, thus this
filter keeps the height. That matters when the height and the width of a peak
carry the information, as in a spectrometer or a chromatograph.

**It also gives a derivative.** The derivative of the polynomial at the middle
of the window is a far better answer than the difference of two samples, which
noise disturbs strongly. Give the number of the derivative to `savgol_design`:
0 for the smoothed signal, 1 for the first derivative, 2 for the second.

**Where the coefficients come from.** The design builds the matrix of the
powers of the positions in the window and solves the normal equations of the
least squares with the module `sptk/linalg`. That work happens one time, inside
`savgol_design`. The filter itself then multiplies and adds only, thus it gets
no memory while it runs.

The window must hold an odd number of samples, so that it has a middle. The
order of the polynomial must be below the size of the window. A higher order
follows the signal more closely and takes away less noise.



## movavg

The mean over a window that slides is the most common smoothing there is, and
the one most often written badly.

A `fir` whose coefficients are all `1/length` gives the right answer, and many
callers reach for that. It costs one multiplication and one addition for each
coefficient, for each sample. A window of 500 samples costs 500 operations a
sample, and at 32 kHz that is 16 million a second for a mean.

It need not cost that. The mean of the new window differs from the mean of the
old one by exactly two samples: the one that arrived and the one that fell off
the end. Measured, in nanoseconds for one sample:

| window | 4 | 8 | 16 | 64 | 500 | 4096 |
| --- | --- | --- | --- | --- | --- | --- |
| `movavg` | 13.4 | 13.4 | 12.5 | 12.4 | 14.1 | 17.6 |
| equal `fir` | 6.0 | 8.2 | 14.1 | 58.9 | 438.7 | 3588.4 |

**Below a window of 16 the plain filter is faster.** The bookkeeping costs more
than four multiplications do. Take `fir` for a very short window and `movavg`
from about 16 upwards.

**It is not a good low pass.** Its answer to a single frequency falls to
nothing at the rate that fits the window and then rises again, thus a tone at
the wrong frequency comes through nearly untouched. Take `fir` or `iir` where
the frequencies matter. Take `movavg` where the window itself is the point: an
energy over the last 200 ms, a level over the last second, the moving mean in
the middle of a detector.

**Three measures, two costs.** `movavg_get_mean` and `movavg_get_rms` are held
as running totals, thus they cost nothing to read. `movavg_get_deviation`
reads the whole window, and it must: a deviation from running totals would be
the mean of the squares less the square of the mean, and those two numbers are
nearly equal whenever the signal sits far from zero. A reading at 8 000 000
that moves by 1 gives two numbers near 64 000 000 000 000 whose difference is
1, and that difference is lost.

**The totals are built again from time to time.** A running total that is added
to and taken away from for ever gathers a small error at every step, and the
error walks rather than cancels. Every `MOVAVG_REFRESH` samples the totals are
worked out again from the window, which costs about one eighth of an operation
for each sample and holds the accuracy for ever.


## medfilt

The median of the last samples. This answers a fault that no mean and no filter
of frequency can answer.

A sample can be **wrong** rather than noisy. A knock on an electrode, a sample
lost on a wire, a spike from a switching supply: each puts one value in the
signal that has nothing to do with the signal, and such a value does not
average away.

**A mean spreads it. A median removes it.** One sample a thousand times too
large, in a window of 50, moves every one of the 50 answers that the window
touches. The fault goes in as one bad sample and comes out as fifty. The middle
of a window does not move at all while fewer than half of its samples are
wrong, thus the same spike has no effect whatever and the signal on both sides
of it is untouched.

**The trap.** A median also removes any peak narrower than half the window,
whether that peak is a fault or not. A window long enough to take out a wide
spike also takes out a real QRS, a real pulse, a real edge. Choose the window
from the width of the FAULT: long enough to cover the spike, and shorter than
anything worth keeping.

**It is not a filter of frequency.** It passes no band and stops no band, and
it has no gain at each frequency, because it is not linear: two signals
filtered and added do not give the same answer as the two added and filtered.

**The usual chain is a median first, then a filter of frequency.** The other
order does not work, because a filter of frequency spreads each spike over its
whole window before the median can reach it.

**What it costs.** One pass over the window for each sample, and no more. The
filter holds its window in order, thus a new sample is put into its place and
the old one taken out of its place, and neither needs a sort. An odd window is
better than an even one: an odd window has a true middle sample, while an even
one gives the mean of two, and that mean is no longer one of the samples.

## What a design cannot do, and how it says so

Every design function gives a `bool`. It is `false` when the filter that was
asked for cannot be built, and then the filter is left exactly as it was.

There are two ways to ask for a filter that cannot be built, and neither one
announces itself in the answer of the filter. Both give back something that
looks like a filter and behaves like a wrong one. This is why the checks exist.

**A cutoff that is too low for an infinite impulse response.** A section holds
its poles near the circle when the cutoff is low. A float holds about seven
digits, and below a cutoff of about 0.001 of the sample rate those digits run
out: the coefficients round to values that describe a different filter. The
gain that should be 1 at zero frequency, measured:

| cutoff | 0.0100 | 0.0020 | 0.0010 | 0.0005 | 0.0001 |
| --- | --- | --- | --- | --- | --- |
| gain | 1.0000 | 1.0014 | 0.9959 | 0.9909 | 0.6849 |

`IIR_MIN_CUTOFF` holds the limit, and `iir_is_valid_cutoff` answers for a
given cutoff.

**A cutoff that is too low for the length of a finite impulse response.** Such
a filter turns from passing to stopping over a band about `2/length` wide. A
cutoff nearer to 0 than that has no room for the turn, thus the pass band never
reaches 1. For 101 coefficients, where the turn is 0.0198 wide:

| cutoff | 0.0500 | 0.0200 | 0.0100 | 0.0050 | 0.0020 |
| --- | --- | --- | --- | --- | --- |
| gain | 1.0024 | 1.0039 | 0.8443 | 0.5065 | 0.2140 |

`fir_is_valid_cutoff` and `fir_is_valid_band` answer for a given length.

**What to do when a design says false.** A cutoff below the limit nearly always
means the sample rate is too high for the work. A cutoff of 0.5 Hz against
32 kHz is 0.000016 and no design can hold it; the same cutoff against 500 Hz is
0.001 and any of them can. Bring the rate down first, then filter. Making the
filter longer answers the second case as well, at the cost of delay.
