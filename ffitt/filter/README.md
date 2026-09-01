# Filters

A filter takes a band of frequencies away and keeps the rest. The three
modules here reach that goal in three different ways, and each way pays a
different price.

| Module | How it works | Choose it when |
| --- | --- | --- |
| `fir` | A weighted sum of the last few samples | The shape of the signal must stay |
| `iir` | Feeds its own output back into itself | The work for each sample must be small |
| `savgol` | Lays a polynomial through a window | A peak must keep its height |

## farrow

`delay_by_phase` measures how far one reading stands behind another to below a
sample. **This is the other half of that**: having measured 2.35 samples,
something has to apply it.

A delay of a whole number of samples costs nothing — it is reading further back
in a buffer, and `ringbuf` already does it. A delay of a **part** of a sample is
a filter, because the value between two samples is not in the reading.

**It goes wrong in two ways and the one that matters is not the obvious one.**
The obvious one is that the delay comes out a little different from the delay
asked for. The one that matters is that it **quietens the signal**: working out a
value between two samples averages them, and averaging takes the fast part away.

How much of the signal is left, at the delay halfway between two samples, which
is the worst place there is:

| part of the rate | order 1 | order 3 | order 5 | order 7 |
|---|---|---|---|---|
| 0.05 | 0.9877 | 0.9998 | 1.0000 | 1.0000 |
| 0.20 | 0.8090 | 0.9488 | 0.9850 | 0.9955 |
| 0.30 | 0.5878 | 0.7801 | 0.8746 | 0.9261 |
| 0.40 | 0.3090 | 0.4488 | 0.5436 | 0.6150 |

At four tenths of the rate an order of 1 puts the delay out by a seventh of a
sample, which sounds tolerable, and throws away **seven tenths of the signal**,
which is not. A caller watching only the delay would call that filter good.

**Choose the order by that table and not by the delay error.** Below a fifth of
the rate an order of 3 keeps 95 in every hundred. Above three tenths no order
here is worth much, and the answer is to sample faster rather than to interpolate
harder.

**The filter adds a whole delay of its own** and cannot not: it works the value
out from the samples either side, so it must wait for them. The delay it applies
runs from half its order to half its order plus one. For more, take the whole
samples with a `ringbuf` and leave the part to this.


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
least squares with the module `ffitt/linalg`. That work happens one time, inside
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


## Choosing a shape of filter

`fir` and `iir` build five shapes between them. The table says which one
answers which question.

| Shape | Function | The question it answers |
| --- | --- | --- |
| Low pass | `iir_design_low_pass`, `fir_design_low_pass` | Take away everything fast |
| High pass | `iir_design_high_pass`, `fir_design_high_pass` | Take away the wander |
| Band pass | `iir_design_band_pass`, `fir_design_band_pass` | Keep one range of frequencies |
| Band stop, notch | `iir_design_band_stop`, `fir_design_band_stop`, `iir_design_notch` | Take away one frequency |
| Peak | `iir_design_peak` | Follow one frequency |

**The notch is the answer to the hum of the mains**, which is the most common
single unwanted frequency there is. Give the frequency as a part of the sample
rate and give the quality, which is that frequency divided by the width of the
stop. A quality of 30 at 50 Hz stops a band about 1.7 Hz wide.

A very narrow stop is not always better. It rings: it answers a step with a
tone at its own frequency that dies away slowly, and that tone can look like a
signal. It also needs the hum to stand still, which the mains does not always
do. A quality between 10 and 50 suits most work.

**A band pass shares its sections between its two edges**, half making a high
pass at the low edge and half a low pass at the high edge. The number of
sections must therefore be even. This suits a wide band. For a narrow band the
two edges reach into each other and the gain in the middle falls below one;
`iir_design_peak` holds its gain at 1 at the middle however narrow the band is.

**The peak and the notch are two sides of one thing.** They share their poles
and differ only in their zeros, thus what one passes the other stops. Where
only the SIZE of one frequency is wanted and not the signal itself, the
`goertzel` module costs far less than either.

**The middle of a band is the geometric mean of its edges** and not the plain
mean. A filter of this kind is symmetric in the RATIO of the frequencies and
not in their difference: a band from 100 to 400 has its middle at 200, because
200 is twice 100 and 400 is twice 200. `iir_design_band_stop` from 0.04 to 0.06
therefore nulls at 0.049 and not at 0.05.


## dcblock

**Put this first in almost every chain.** It takes the level of a signal away,
and until it has, every filter after it works with less precision than it
should.

Almost every reading arrives with a large constant part that carries nothing. A
converter of 24 bits sitting near the middle of its range gives about eight
million counts, and the signal on top may be a few thousand. A float holds
about seven digits, thus six of them are spent on the part that carries
nothing.

**A high pass makes this worse, not better.** Its poles lie at a radius near
0.9956 for a low cutoff, and a filter with such poles lifts whatever error
reaches it by about two hundred times. Measured, on a wave of 1000 counts
carried on a level, where the answer should not depend on the level at all.
The figures are what each filter added BEYOND its own shape:

| level | 0 | 1 000 | 100 000 | 8 300 000 |
| --- | --- | --- | --- | --- |
| `iir`, one section, 32 bits | 0.0 | 0.0 | 0.2 | 98.9 |
| `iir`, two sections, 32 bits | 0.0 | 0.0 | 0.1 | 137.8 |
| `dcblock`, 32 bits | 0.0 | 0.0 | 0.0 | 0.1 |
| either, 64 bits | 0.0 | 0.0 | 0.0 | 0.0 |

A hundred counts of false signal against a wave of a thousand is a tenth of the
answer, and it comes from nothing but the size of the number. It grows with the
order of the filter, because each section lifts the error of the one before it.

**It is one pole, and that is what saves it.** A single pole holds no two
nearly equal numbers to subtract, thus it has nothing to lose. At the default
width it is some eight hundred times better than a section; at 64 bits the
section has digits to spare and the two are alike, and this one is then simply
the gentler filter.

**It holds a far lower cutoff.** `IIR_MIN_CUTOFF` is 0.001 of the sample rate
and under that a section gives a wrong answer without saying so. This module
holds 0.000001, a thousand times lower, because one pole has no cancelling sums
in it. At 32 kHz that is 0.03 Hz, which no section could reach
at that rate.

**It primes itself on the first sample.** A filter that starts from zero sees
the first sample as a step of the whole level, and at eight million counts with
a cutoff under one hertz the answer to that step is larger than the signal for
tens of seconds. This module sets its level to the first sample it is given,
which says: assume the signal stood here for ever before now.

**It is one pole and nothing more.** It falls away at 6 dB for each octave,
which is gentle, and it is meant to take the level away and not a whole band.
Where a band must go, put this first to bring the signal near zero and then use
`iir`, which now has the precision to do its work.

**The level is worth reading on its own.** `dcblock_get_level` gives the slow
part of the signal, which carries the drift and the wander of a contact.


## hampel

Find the samples that are wrong and replace **only those**.

**Why a median alone is not enough.** A median filter removes a spike and also
changes every other sample it touches. Give it a clean signal and it gives back
a different clean signal: every peak narrower than half its window is gone and
the rest is flattened. That is a heavy price for a fault that happens once a
second.

This filter asks one question of each sample: how far does it stand from the
middle of its neighbours, measured against how far they usually stand from it?
A sample far outside is replaced by the middle; every other sample is passed
through **exactly** as it arrived. A recording with three bad samples in a
minute comes back with three samples changed.

**The spread is a median absolute deviation, and that is the whole of why it
works.** A standard deviation is moved by the very samples it is meant to
catch. This is called masking: one enormous spike raises the deviation so far
that a smaller fault beside it slips through the threshold built to catch
faults. Two tests hold that story — one shows this filter catching both a fault
of 100 000 and a fault of 50 three samples later, the other works the same
window out by hand and shows a three-deviation rule letting the small one
through.

**The answer comes late.** A sample can only be judged against its neighbours
on both sides, thus `hampel_delay` samples must pass before the answer for one
arrives. `hampel_process_block` puts that right and gives an output as long as
its input.

**Read `hampel_replaced_count`.** It is the measure of how much was wrong with
the signal. A recording where one sample in fifty is replaced is a recording to
look at rather than to trust.


## adaptive

A filter that finds its own coefficients while it runs. Every other filter here
is designed once and then applied; this one is given no design at all, only an
answer to aim at.

**Taking away noise that is measured somewhere else.** This is the use that
matters and the one a fixed filter cannot serve. A microphone near an engine, a
coil near a transformer, a lead near a motor: a second sensor sees the noise
**alone**. The noise reaches the first sensor changed in size and delayed, by
an amount nobody knows and which does not stay still. Give the noisy signal as
what to aim at and the second sensor as the reference.

**Take `adaptive_error`, not `adaptive_process_sample`.** The output is the
noise the filter has learned; the error is the signal with that noise gone.

This works where no filter of frequency can, because the noise and the signal
may hold exactly the same frequencies. A test holds that: random noise, which
covers every frequency, is cut to under a fiftieth while 99.7 percent of the
signal survives.

**The reference must not hold the signal.** If it does, the filter learns to
take the signal away too, because that also makes the error smaller. This is
the one way to use it that fails quietly: the error falls, everything looks
well, and the answer has had the signal removed. Measured, the part of the
signal that survives:

| reference | signal surviving |
| --- | --- |
| the noise alone | 0.997 |
| the noise with the signal in it | 0.293 |

**Reach for `ADAPTIVE_NORMALISED`.** The plain rule moves each coefficient by
the rate times the error times the reference, thus how far it moves follows how
**loud** the reference is: a rate that settles for a quiet reference makes the
filter run away for a loud one, and the safe rate depends on a signal the
designer has not heard yet. The normalised rule divides by the energy in the
filter, and any rate between 0 and 2 is then stable for any signal. Take 0.1 to
0.5.

**The rate is a trade, not a setting to get right.** A high rate follows a
change quickly and rattles about the answer; a low rate settles closer and
takes longer. Over the same run, a rate of 0.5 left about a fifth of the noise
and a rate of 0.02 about a hundredth.

**The coefficients are worth reading.** They are the answer to what the path
between the two sensors does, and where the largest one stands is the delay
between them in samples.


## resample

Changing the rate at which a signal is sampled.

**Keeping every fourth sample looks like the whole of it, and it is the half
that goes wrong.** A signal at 32 kHz may hold frequencies up to 16 kHz. Keep
every 64th sample and the new rate is 500 Hz, which can hold nothing above 250.
Every frequency above 250 does not disappear: it comes back at a frequency it
never had, and once it is there **nothing** can take it out, because it now
sits on top of the signal and looks exactly like part of it.

A hum at 4 kHz decimated by 64 arrives at 0 Hz and looks like a drift. A noise
at 300 Hz arrives at 200 Hz and looks like a signal. The reading looks
reasonable and is wrong.

Two tests hold that story: one shows a tone far above the new rate cut to under
a hundredth, and the other keeps every fourth sample with no filter and shows
the same tone coming back at full size.

Going up has the mirror of the problem. Putting zeros between samples leaves
copies of the signal at every multiple of the old rate, and the filter after
them takes the copies away.

**Only the samples that are kept are worked out.** A filter that ran at the
input rate and then threw most of its answers away would do work for nothing:
decimating by 64 with a filter of 128 coefficients, 8064 multiplications of
every 8192 would be wasted. This module works out the kept answers only, thus
the filtering costs the **output** rate and not the input rate. That is the
whole reason a long filter is affordable here.

**How long a filter.** The filter must pass what is wanted and stop everything
above half the new rate, and those two edges lie close together when the factor
is large. `resample_advised_length` gives a length that works — measured for a
factor of 4, it passes 0.95 to 1.00 in the band and stops to 0.002, which is
54 dB down.

**A large factor is better done in stages.** From 32 kHz to 500 Hz in one step
needs about 2000 coefficients. As 8 then 8 it needs two filters of about 40,
and the two together cost far less than the one.


## filtfilt

Filtering with no delay at all, by running the filter both ways.

**Every filter delays what it passes**, and one with feedback delays each
frequency by a different amount. That is why the shape of a signal changes
after filtering even when nothing was taken out of the band it lives in: the
parts arrive at slightly different times and no longer line up. The peak of a
heartbeat moves; the edge of a step leans; a pulse is no longer the same width.

Run the filter forwards, turn the answer round, run it again. The second pass
delays every frequency by exactly what the first did and in the opposite
direction, thus the two cancel exactly.

**Two prices, and both must be paid knowingly.**

The whole signal must be in hand. There is no way to run a filter backwards
over a signal that has not arrived. This is for a recording, not for a signal
as it comes in, at any delay.

The filter is applied twice, thus its gain is **squared**. A cutoff is where a
filter passes 0.707; run twice it passes 0.5, which is a different cutoff. The
band is narrower than the one designed and the edges are twice as steep. Design
for it — `filtfilt_iir_gain` gives what the filter really does.

**It needs no memory.** Both passes run over the caller's own output list, and
the input and the output may be the same list.

**The two ends.** A filter starting from nothing answers the first sample as a
step, and running both ways would put that swing at **both** ends. Two things
are done: the filter is first settled at the value it is about to meet, by
being fed it until its answer stops moving; and the signal is then carried
outwards past each end, turned about the end sample so that it begins with no
step and no corner. Settling alone is not enough, and carrying alone is not
either — a filter with a low cutoff takes thousands of samples to answer a
step where the carried part is a few dozen.

## What a design cannot do, and how it says so

Every design function gives a `bool`. It is `false` when the filter that was
asked for cannot be built, and then the filter is left exactly as it was.

There are two ways to ask for a filter that cannot be built, and neither one
announces itself in the answer of the filter. Both give back something that
looks like a filter and behaves like a wrong one. This is why the checks exist.

**A cutoff that is too low for an infinite impulse response.** A section holds
its poles near the circle when the cutoff is low, and when the digits of the
build run out the coefficients round to values that describe a different
filter. The gain that should be 1 at zero frequency, measured with the check
taken out so that every cutoff could be tried:

| cutoff | 0.001 | 0.0001 | 1e-5 | 1e-6 | 1e-7 |
| --- | --- | --- | --- | --- | --- |
| 32 bits | 0.996 | 0.685 | 0.000 | 0.000 | 0.000 |
| 64 bits | 1.000 | 1.000 | 1.000 | 1.000 | 1.001 |

**The limit follows the width of the build**, and it is a thousand times lower
at 64 bits. A high pass at 0.5 Hz against 32 kHz is a cutoff of 0.000016: out
of reach at 32 bits, and nothing at all at 64. `IIR_MIN_CUTOFF` holds the limit
of the build and `iir_is_valid_cutoff` answers for a given cutoff.

**A cutoff that is too low for the length of a finite impulse response.** Such
a filter turns from passing to stopping over a band about `2/length` wide. A
cutoff nearer to 0 than that has no room for the turn, thus the pass band never
reaches 1. For 101 coefficients, where the turn is 0.0198 wide:

| cutoff | 0.0500 | 0.0200 | 0.0100 | 0.0050 | 0.0020 |
| --- | --- | --- | --- | --- | --- |
| gain | 1.0024 | 1.0039 | 0.8443 | 0.5065 | 0.2140 |

`fir_is_valid_cutoff` and `fir_is_valid_band` answer for a given length.

**What to do when a design says false.** A cutoff below the limit usually means
the sample rate is too high for the work. A cutoff of 0.5 Hz against 32 kHz is
0.000016; the same cutoff against 500 Hz is 0.001, which any 32 bit build
holds. Bring the rate down first, then filter. Building at 64 bits answers the
first case as well, and making the filter longer answers the second, at the
cost of delay.

## detrend

**A block, not a stream.** `detrend` reads the whole block before it can give
the first answer. For a stream, `dcblock` follows the level as it goes and takes
it away one sample at a time. The two solve the same trouble at different ends.

**Why it matters before a transform.** A transform reads the block as one
period of something that repeats, thus a block that ends higher than it began
holds a step at the join. That step is not in the signal, and its energy spreads
across every frequency. Measured, on a wave of one unit at bin 8 with a drift of
4 units across the block, what stands at bin 1, which holds no signal at all:

| | at bin 1 |
|---|---|
| with the drift left in | 1.27 |
| with the drift taken out | 0.08 |

The false answer is larger than the true signal. A window softens the join but
cannot undo a drift.

**It takes a little of the signal as well, and this is not small.** A straight
line through a block always holds something in common with a wave in that block.
A wave **even** about the middle of the block loses `3/(n+1)` of itself, which
falls away as the block grows. A wave **odd** about the middle loses about
`3/(pi k)` where `k` is the number of periods it makes across the block, and
**the length of the block does not come into it**:

| periods across the block | 1 | 4 | 16 | 32 |
|---|---|---|---|---|
| part of the wave taken | 0.95 | 0.24 | 0.06 | 0.03 |

A wave that makes one period across the block loses 95 parts in 100, at 64
samples and at 4096 alike. The fit is not at fault: one period across the block
**is** a drift as far as anything looking at that block can tell. Keep the block
long compared with the lowest frequency wanted.

**The samples are numbered from the middle.** That makes their numbers sum to
zero and keeps every sum the size of the samples themselves. Measured on a block
of 4096 at 32 bits it is about four times better than numbering from the start,
at every level, for the same work. Because of it, the offset that `detrend_trend`
gives is the value of the trend at the **middle** of the block, which is the
mean, and not the value at the first sample. Use `detrend_trend_at` rather than
working it out by hand.

**Use `detrend_remove` where the signal itself rises.** Finding the trend afresh
in every block takes the signal away with the drift where the signal rises
across the block. Find the trend once from a block known to be quiet, and take
that same trend out of the blocks that follow.

## The four shapes of an IIR filter

**No filter is best at all three things.** How flat the band that passes is, how
sharply it falls, and how much of the stopped band gets through — every shape
trades these against each other. Measured on a low pass of order 8 at a cutoff
of 0.1, asked for 1 dB of ripple and a stop band 60 dB down:

| shape | at nothing | ripple | falls to 60 dB below |
|---|---|---|---|
| Butterworth | 1.000 | none | 0.209 |
| Chebyshev I | 0.891 | 1.000 dB | 0.151 |
| Chebyshev II | 1.000 | none | 0.100 |
| Elliptic | 0.891 | 1.000 dB | 0.110 |

**Ask `iir_sections_for` before choosing.** The same trade, seen through what it
costs. To pass below 0.1 and stop above 0.15 by 60 dB, with 1 dB of ripple
allowed:

| shape | sections | order |
|---|---|---|
| Butterworth | 9 | 18 |
| Chebyshev I | 5 | 10 |
| Chebyshev II | 5 | 10 |
| Elliptic | 3 | 6 |

**A third of the sections for the same work.** Every filter in that table was
built and measured, and every one really meets what was asked.

**The cutoff means a different thing for different shapes.** Butterworth counts
it where the answer has fallen 3 dB; Chebyshev I and elliptic where the answer
leaves the ripple; **Chebyshev II counts it where the band that is STOPPED
begins**, so the answer is already all the way down there. Giving all four the
same number does not give four filters that can be compared.

**An elliptic filter at 32 bits falls a little short of a deep stop band.** It
holds the stopped band down with notches, and a notch must be placed exactly to
reach all the way. At 32 bits the coefficients cannot always place them exactly.
Measured at a cutoff of 0.05 with 70 dB asked for, the delivered depth is 70.0,
70.0, 66.9, 69.2 and 69.6 dB for ripples of 0.5, 1, 2, 3 and 5 dB. The shortfall
is at most about 3 dB and **more sections do not mend it**. Ask for a few dB more
at 32 bits, or build at 64. No other shape shows this.

**Look at `iir_group_delay` before trusting the shape of a waveform.** Every
shape moves different frequencies by different times, and the sharper the fall
the worse it gets. A Butterworth of 4 sections at a cutoff of 0.1 holds the
signal back by 7.9 samples at 0.01 and 14.6 samples at 0.09 — that difference is
what bends a waveform out of shape. Where it matters, use `filtfilt`, which runs
the filter both ways and leaves no phase shift at all.

## Choosing the window of an FIR filter

**The plain sinc is the perfect filter and it runs for ever.** Cutting it to a
finite length is what a window does, and the window decides how wide the turn is
against how far down the stopped band lies. Measured for a low pass of 101
coefficients at a cutoff of 0.25, with the turn taken from where the answer last
stands at 0.9 to where it first reaches 0.1:

| window | turn is wide | times the length | stopped band |
|---|---|---|---|
| rectangular | 0.0090 | 0.90 | -26 dB |
| hamming | 0.0182 | 1.84 | -58 dB |
| hann | 0.0194 | 1.96 | -55 dB |
| kaiser, beta 6 | 0.0198 | 1.99 | -68 dB |
| blackman | 0.0238 | 2.40 | -75 dB |
| blackman-harris | 0.0281 | 2.83 | -104 dB |

**The third column is the same at 101 coefficients and at 201.** That is what
says the turn belongs to the shape of the window and to the length, and to
nothing else — so `fir_length_for` can give a length from a turn, and
`fir_transition_width` the turn from a length.

**A longer filter makes the turn narrower and changes nothing else.** The depth
of the stopped band belongs to the window alone. To go deeper, change the
window; to turn faster, lengthen the filter.

**And this is the reason to choose an FIR at all.** Every design here is
symmetric, so `fir_group_delay` gives the same answer at every frequency: half
the length less one half. A waveform comes out moved along and not bent. Against
the same specification, `iir` measures a Butterworth rising from 41 samples to
93 across the band that passes and an elliptic from 14 to 87. That is what an
FIR spends its length on.

## rls

**The adaptive module walks downhill; this one solves the whole problem at every
sample.** Measured on a filter of 16 learning an unknown response, the samples
taken to bring the coefficients 40 dB towards the truth: `adaptive` needs 163,
`rls` needs 24.

**But read the other half of that.** Left to run, the two settle at different
places — at 32 bits `adaptive` reaches −149 dB and `rls` stops at −97 dB, because
`rls` is limited by the precision of the matrix it carries and `adaptive` is
limited by nothing. **The choice is not which is better**: it is whether the
answer is wanted quickly or wanted exactly.

**It is a separate module because of what it costs.** At 32 bits:

| length | `adaptive` | `rls` |
|---|---|---|
| 16 | 0.1 kB | 1.3 kB |
| 64 | 0.5 kB | 17 kB |
| 256 | 2 kB | 266 kB |

A length of 256 is ordinary for an echo canceller and 266 kB is not ordinary for
a device. That had to be visible in the type rather than hidden behind an
enumeration.

**The matrix drifts, and the drift kills the filter.** This is the part that
surprises people: an RLS filter runs perfectly for thousands of samples and then
runs away. The matrix it carries should stay symmetric and nothing in the
arithmetic holds it to that. Measured on a filter of 16 over a million samples at
32 bits:

| forgetting | halves drift apart by | what happens |
|---|---|---|
| worked out apart, 1.000 | 1.03 | held |
| worked out apart, 0.999 | 1.82 | **fell over at sample 7230** |
| worked out apart, 0.990 | 1.84 | **fell over at sample 987** |
| worked out apart, 0.950 | 1.31 | **fell over at sample 216** |
| written together, all four | 0.00 | held |

The two halves come to differ by **more than the largest element of the matrix**.
This module works out one half and writes it to both, which holds them exactly
equal for nothing, for ever — the same thing `ukf` does to its covariance, for the
same reason. **Ask `rls_is_healthy`**: a filter that has fallen apart still
answers, and its answers are nonsense.

**A fading past costs cancellation.** Taking away interference of size 0.35 beside
a signal of size 1, what is left over is 0.030 at a forgetting factor of 1, 0.033
at 0.999 and 0.117 at 0.99 — that is 21 dB, 20 dB and 9 dB of the interference
removed. Fade only as fast as the thing being learned really moves.

## lattice

**A ladder of stages rather than a straight list of coefficients.** Each stage
takes away from the signal whatever the stages before it already explained, so
each learns at its own pace and none waits for another. That is what makes a
ladder quick on an input whose samples lean heavily on each other, where a
straight filter's coefficients are pulled about as a group.

**Read the measurement across, not down.** Twelve stages learning a response of
three taps from a strongly leaning input, showing how far the error stands below
the wanted signal:

| samples | 100 | 300 | 1000 | 3000 | 10000 | 30000 |
|---|---|---|---|---|---|---|
| `lattice` | -10.5 | -23.0 | -34.9 | -43.0 | -43.7 | -40.0 |
| `adaptive` | -14.4 | -21.3 | -25.2 | -63.6 | -139.7 | -139.5 |
| `rls` | -14.7 | -24.4 | -30.8 | -41.9 | -52.2 | -58.1 |

There is a window, from about 300 samples to about 3000, where the ladder is
ahead of both. Before it the ladder is still finding its stages. **After it the
ladder stops improving and the others do not** — it settles near −43 dB while
`adaptive` walks on down past −139 dB.

**So take a ladder where the middle is all there is.** Where the thing being
learned changes every few thousand samples, no filter ever reaches its floor and
only that window matters. Where it stands still and there is time, `adaptive`
ends up far ahead and costs less.

**The rate trades how fast it settles against how low.** The stages never stop
moving, and their movement keeps stirring what the weights are settling on:

| rate | 1.00 | 0.50 | 0.20 | 0.05 | 0.01 |
|---|---|---|---|---|---|
| at 10000 samples | +1.9 | -42.0 | -45.4 | -34.5 | -18.2 |
| at 100000 samples | -38.5 | -42.1 | -45.9 | -53.2 | -60.3 |

About **0.2** is the best of both for most work. A rate of 1 does not settle at
all in any useful time.

**It cannot fall apart the way `rls` can.** There is no matrix to lose its
footing: every stage holds one number and the arithmetic holds that number
between −1 and 1 rather than trusting it to stay there.

**The two errors are both offered, and the difference matters.** The error *a
priori* is what the filter would have said had it not been about to learn — the
honest measure, and the one to watch and record. The error *a posteriori* is what
is left after the sample has been learned from; it is always the smaller, and it
is the one to **use** where the filter is taking something away. Reporting the
second where the first belongs is how an adaptive filter comes to look better
than it is.
