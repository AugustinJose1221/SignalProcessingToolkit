## 0.16.0 (2026-08-28)

The other half of measuring a delay, a way to follow a tone that will not stay
still, and the transform behind every compression there is.

**farrow delays a signal by a part of a sample.** `delay_by_phase` has measured
delays below a sample since 0.14.0 and nothing could apply one. Lining two
readings up, steering an array of microphones, resampling by a ratio that is not
whole, and following a clock that drifts all come down to it.

The weights are the ones Lagrange wrote down, held as **polynomials in the delay**
and worked out once when the filter is built. Read as arithmetic they are a
division for every sample at every step; read as polynomials they are fixed
numbers, and moving the delay is then reading them at another number. That is
what lets the delay move at every sample for almost nothing.

**The header says what it costs, and the cost is not the obvious one.** The
obvious one is that the delay comes out a little different from the delay asked
for: at a fifth of the sample rate an order of 3 is out by 0.0076 of a sample.
The one that matters is that it **quietens the signal**, because working out a
value between two samples averages them. At four tenths of the rate an order of 1
puts the delay out by a seventh of a sample and throws away seven tenths of the
signal. A caller watching only the delay would call that filter good. Both tables
are measured.

**And a high order costs something at 32 bits that it does not cost at 64.** The
weights come from products and divisions that grow quickly with the order, and
they should add up to exactly one at every delay. They miss by a part in six
hundred at an order of 8 at 32 bits and by 4.1e-12 at 64.

The generated tests close the loop the two halves make: a delay applied by
`farrow` is measured back off the pair by `delay_by_phase`, and what comes out is
what went in. Neither half could be checked against anything but itself before
the other existed.

**pll follows a tone whose frequency will not stay still.** A transform reads a
block and gives the frequencies in it, and that is a trade no code can escape: a
block long enough to tell 50.0 Hz from 50.1 is longer than the frequency stays
still, and one short enough to follow the movement cannot tell the two apart. A
loop does not have that trade. It gives a new answer at every sample, and how
quickly it follows a change is a number the caller sets.

It can be wrong in three ways a transform cannot, and the header names all three.
It must be started near the answer. It takes time to arrive. **And it can settle
onto something that is not there**: given noise and no tone it reports a
frequency with exactly the confidence it reports a real one, and
`pll_lock_quality` is the only thing that tells the two apart.

**The loop measures how loud the signal is and divides by it.** Without that the
gain would be the gain the caller asked for multiplied by the loudness of
whatever arrived: a tone at a tenth of the expected loudness would answer at a
tenth of the bandwidth and might never arrive, and one ten times as loud would be
unstable. The bandwidth would then be a number about the signal and not about the
loop.

Measured, at a bandwidth of 0.0005 the answer wanders by 0.0011 and the tone is
found in 4603 samples; at 0.01 it wanders by 0.0127 and is found in 42. Twenty
times the bandwidth finds the tone a hundred times as fast and wanders eleven
times as far.

**dct turns a signal into cosines, and back.** `fft` gives sines and cosines,
which is what is needed to say where in its turn each frequency stands. Where the
phase is not wanted, half of that is wasted.

Measured on a curve of 64 samples that does not come back to where it started,
four numbers hold 0.99 of it where the transform needs ten bins, and a bin is two
numbers: **the same curve in a fifth of the room**. A signal of noise needs all 64
either way, which is the point.

Why it wins is not the arithmetic. A transform treats the block as one turn of
something that repeats, thus a signal that starts low and ends high has a step
where the end meets the beginning again, and a step needs every frequency there
is. This treats the block as half a turn of something mirrored, thus the end meets
its own mirror and there is no step.

It takes any size and not only a power of two, and it costs time proportional to
the square of the size.

**cepstrum was written and not shipped.** It was to have been in this release. It
finds a note whose fundamental is missing, which correlation cannot, and on a
clean note of twelve harmonics it did: it answered 64 where correlation answered
128, and correlation said so with a strength of 1.000. It also found an echo at
100 samples in a burst of noise.

**It did not survive a noisy note.** With a hundredth of noise on the same signal
the answer moved to 192, and with a twentieth to 255. Sweeping the floor that
holds the logarithm moved which cases passed and which failed without making any
of them steady, and the answer differed between the two widths on a clean signal.
A module whose header claims what its measurements do not support is worse than
no module, thus it waits for the design work it needs.

**feature is dropped.** It was a name without a use case, and no use case has
turned up. It goes the way of the tensors.

## 0.15.0 (2026-08-28)

Signals to test with, the shapes a peak can have, and a fault in a corner that
had been there since the square wave was written.

**The noise this library assumes it has, and did not.** `matched_threshold_for`
turns a rate of false alarms into a threshold by inverting the tail of a normal
spread. The table of thresholds in `changepoint.h` was measured on normal noise.
`kalman`, `ekf` and `ukf` all take the noise of the process and of the
measurement to be normal. `GENERATE_WHITE_NOISE` is drawn EVENLY, thus none of
those claims could be examined with what the module had: measured, the same
`changepoint` threshold gave one wrong alarm in every 372 samples on an even
spread and one in every 465 on a normal one.

`GENERATE_GAUSSIAN_NOISE` is drawn by the method of Box and Muller and not by
adding a dozen even draws together. The shortcut is bounded and has no tails
past about four standard deviations, and the tails are the whole point: a rate
of one in a million is a question about what happens past five. Measured over
200 000 samples it puts 4.55 in every hundred past two standard deviations and
0.27 past three, which are the shares a normal spread has.

**Five more kinds beside it.** `GENERATE_BROWN_NOISE` is a random walk, which is
what drift looks like; what is added is scaled by the root of what is not kept,
thus the spread of the walk matches the white noise that built it rather than
growing twentyfold. `GENERATE_BLUE_NOISE` is the mirror of pink, made by taking
the difference of pink rather than of white, which would rise twice as fast and
be violet. `GENERATE_PULSE` is high for a chosen part of each turn and band
limited at both corners, and the square wave is that with the part set to a
half. `GENERATE_GAUSSIAN_PULSE` is a bump once each turn, which is the shape
`matched` and `delay` are built to find. `GENERATE_IMPULSE` is one sample of one
at the start of each turn.

The header of `generate` now holds a measured table of what every kind comes out
at, and says plainly which three leave the range of one and which two do not add
up to nothing.

**A fault in the corner smoothing, there since the square wave was written.**
How far a sample stands past a corner was held from nothing to one, thus a
sample standing a hair BEFORE the corner had a distance a hair below nothing and
one was added to bring it round. At 64 bits a hair below nothing plus one rounds
to EXACTLY one, and exactly one was then read as a hair AFTER the corner. The
two sides are moved in opposite directions, thus the sample was moved by one the
wrong way and the wave jumped by two: a pulse at 100 Hz in 8000 with a part of an
eighth gave 2.0 at sample 10, where the whole shape stands between -1 and 1. The
distance is now held in the half turn either side of the corner, where a sample
before it keeps a distance below nothing and there is nothing to round.

The square wave and the sawtooth ran the same risk all along and were saved only
by which numbers their corners happened to land on. **The fault could not be
shown without the pulse**: no frequency reaches it through the two shapes that
existed.

**A new module, `curve`, for the shapes a peak can have.** Nothing in this
library could make a peak whose top was known, and that matters because every
module that finds or refines a peak gives an answer that depends on the shape it
was given.

`curve_gaussian` is the kind shape. `curve_lorentzian` is the shape of a
resonance and its tails are enormous beside a gaussian's: at three widths from
the middle the gaussian holds a hundredth of its top and the lorentzian a
seventh, and at twenty widths the gaussian has been nothing for a long time and
the lorentzian still holds a two hundred and sixtieth. `curve_skewed_gaussian`
is the shape of anything that arrives quickly and leaves slowly, and its top
does not stand at its middle.

Every width means the same thing: each shape falls at one width from the middle
to the share a normal spread has at one standard deviation. Written any other
way two widths could not be set beside each other.

**Where a peak really stands, and how tall it is.** `peakdetect_refine` and
`peakdetect_refine_height` fit a curve through a peak and its two neighbours.
`delay_refine_peak` asked the same question of a correlation and answered it
with its own copy of the same arithmetic; it now calls `peakdetect_refine`, thus
there is one implementation and not two.

Both are now measured, which could not be done before `curve` existed. Sampled
five to a width with the top moved through a hundred places between two samples,
worst case, in samples:

| shape | refined | rounded |
| --- | --- | --- |
| gaussian | 0.0019 | 0.5000 |
| lorentzian | 0.0049 | 0.5000 |
| skewed gaussian, skew 2 | 0.0346 | 0.5070 |
| skewed gaussian, skew 4 | 0.1263 | 0.5256 |
| skewed gaussian, skew 8 | 0.3403 | 0.5948 |

Refining beats rounding on every shape, and how much by falls away as the peak
leans: by two hundred and sixty times on a gaussian and by only one and three
quarter times at a skew of 8.

**And the height stops helping before the place does.** The largest sample stands
below the real top and the fitted curve stands above it, and on a peak that leans
hard the curve overshoots by more than the sample undershoots: at a skew of 8 the
fitted height is out by 0.0303 where the largest sample is out by 0.0270. The
header says to take the largest sample as the height past about a skew of 4.

**Generated tests for `correlate` and `csd`**, which `delay` leans on and which
had unit tests only. The property suite now runs 362 tests at each width, against
313 before.

**The release steps in the README did not work.** They said to merge the release
branch with `--ff-only`, which has not been possible since 0.9.0: the branch is
merged into two branches, thus each gets a merge commit of its own and from that
moment neither holds the other. The tag now names `main` as well, because leaving
the branch off tags whichever branch is checked out, which the steps' own order
makes `development`.

## 0.14.0 (2026-08-28)

A new area for detection, generated tests for the ten modules that had none,
and four faults those tests found.

**Three new modules in a new area, `sptk/detect`.** They answer questions of the
form *did something happen, and when*, and they share one shape: each turns a
reading into a number AND gives the threshold to judge that number by. A
detector without a threshold is a number nobody can act on, and every one of
these will answer whatever it is given.

`matched` looks for a known shape buried under noise. The score is divided by
the root of the energy of the shape, thus a reading of pure noise gives a score
whose spread is the spread of the noise and one threshold serves every shape.
`matched_threshold_for` turns a wanted rate of false alarms into that threshold
and takes the number of offsets that will be looked at: a rate that is right for
one offset cries wolf ten times across ten thousand of them. The inverse of the
normal tail is worked out inside the module, because the library must need no
other library, and it is held to within seven millionths at 32 bits and a
millionth at 64 across rates from a half down to a thousand millionth.

`delay` says how far one reading stands behind another, to below a sample. Two
ways, and they fail differently: the correlation works on anything and leans by
the shape of its peak, and the phase is finer and settles as the reading grows
but asks for a reading that fills a band bin by bin. Measured on nine tones
spread across the band, a delay of 7 samples came back from the phase as 1.6,
which is the trap the header names. Running both and comparing is the only
warning either of them gives.

`changepoint` says when a reading has changed, where the change is smaller than
the noise and no threshold on a single sample can find it. The header holds a
measured table of what each threshold costs, taken on twenty million samples: a
threshold of 5.0 finds a change the size of the noise in ten samples and is
wrong once in every 465.

**The fir side gained a band stop**, plain and with a chosen window. The iir
side had one and a notch; the fir side had a band pass and no way to turn it
round.

**Generated tests for the ten modules that had only unit tests**: `eigen`,
`poly`, `rls`, `lattice`, `propagate`, `generate`, `quantise`, and the three new
ones. The property suite runs 313 tests at each width, against 190 before.

They found four faults.

**The lattice filter ran away when told to forget nothing.** The weights divide
by the mean loudness of each stage, and that mean came from multiplying the
running sum by one less the forgetting factor. At a factor of exactly one, which
`lattice_is_valid_forgetting` takes, the multiplier is nothing: the whole
normalisation went away, each weight was left dividing by its own sample alone,
and a quiet sample moved a weight by thousands. The answer reached infinity by
sample 244 at a rate of 1.0. The filter now counts how many samples the sum
stands for, fading that count as the sum fades, and divides by it. Settled
behaviour does not change; the start of a run is now right as well.

**The size of a complex number was nothing where its square did not fit.** It
was the root of the sum of the squares, and at 32 bits the square of 6.1e-30
falls below the smallest ordinary number there is. It cost a root: `poly_roots`
walked onto a root at 6.1e-30, asked how large the polynomial was there, was
told nothing, and took a plain real root for a complex pair. A pole outside the
circle was missed and `poly_is_inside_circle` called an unstable filter stable.
`hypot` is part of the standard library of C and holds both ends of the range.

**The root finder measured against a fixed number rather than against the
polynomial.** The walk stopped when the value reached 100 times the smallest
step the width can tell, which says nothing about a polynomial whose whole size
is near it. On `0.000061 - x^4` at 32 bits it stopped with a fifth of the
polynomial still there, a root standing straight up the imaginary line was then
called real, a line was divided out where a quadratic belonged, and the first
root came back as -1.61 where all four stand at 0.088.

**And polishing could make a root worse.** Where several roots stand on top of
each other both the value and the slope are nearly nothing, thus one step can go
anywhere. At 32 bits on a root of four at -0.8125 the divisions had left every
root within 0.07 of the truth and the polishing threw two of them out to 114,
where the polynomial reaches 170 million. A step that does not make the value
smaller is no longer taken, which also made the answers better where they were
already right. The walk is also given more steps where more roots stand
together, because it creeps in rather than rushing: the flat budget of 200
covered six roots together and not seven, and `poly_roots` refused x to the
seventh, whose seven roots all stand at nothing.

**Two limits of the width are now written down rather than left to be found.**
`eigen_solve` decides how far to turn from the SQUARES of the elements, thus it
needs a matrix whose elements the width can square: about 1e-19 at 32 bits.
Scaling the matrix up costs nothing and the header says so. `matched.h` says
what the width costs its threshold and not only what the fit costs it.

**The detect example** walks a depth sounder through all three questions, and
each step is built around the number that says whether to believe the answer. It
scores a block with no echo in it and the filter still reports a loudest offset;
it runs both ways of measuring a delay across five loudnesses, where at the top
one says 7 samples and the other says 250 and at the bottom they agree on 2.30
against the 2.35 put in; and it shows a watcher pointing at ping 366 for a
change put in at ping 300, which is right, because a slope has nothing to walk
away from until it has grown.

## 0.13.1 (2026-08-27)

A fault that had come into the repository twice, the check that now catches it,
and the survey example.

**The continuous example gave a main function in the default build**, which is
the one build that must give none. The name `RUN_CONTINUOUS_EXAMPLE` was never
written into `run_example.h`, and the header itself says what that costs: a name
that is not there counts as zero inside a condition, and zero is the value of
`RUN_NONE`.

This is the second time. It was fixed for the fitcurve example in 0.11.1 and came
back with the continuous example in 0.12.0, both times through the same slip: the
loop that runs what the workflow runs ends by putting `run_example.h` back, which
throws the new name away along with the line it meant to restore. Both times it
passed every other check, because each example is built BY NAME and the one whose
name is missing is the one that compiles.

**Being more careful has now failed twice, so the check is written down instead.**
`scripts/check_examples.py` runs as its own job and finds five faults: an example
naming a `RUN_` value that `run_example.h` does not define, an example source
holding no `RUN_` condition at all, two examples given the same number, an example
source that `CMakeLists.txt` does not build, and an example the workflow does not
walk through. It found the fault in the commit before it.

**The survey example** shows the work to do before a canceller is written, in four
steps, each of which can say the loop is not worth writing.

Step one takes the coherence between the two sensors, which is the ceiling on
cancellation: 0.99 away from the signal, allowing about 20 dB, and a dip to 0.25
at the signal itself. That dip is what a good reference looks like, because it has
never heard the thing being measured.

Step two runs a deliberately over-long `rls` probe and reads the path off its
coefficients, recovering a delay of 7 samples and a shape of 0.5995, -0.3002,
0.2002 and 0.1005 against a true 0.6, -0.3, 0.2 and 0.1 — none of which the
program is told.

Step three measures the learning rate against two things, because picking it for
the speed of convergence is half the story:

| rate | noise removed | signal kept |
| --- | --- | --- |
| 0.50 | -13.8 dB | 0.534 |
| 0.05 | -20.4 dB | 0.957 |
| 0.01 | -15.4 dB | 0.992 |

At 0.5 the filter takes half the signal away with the noise **and cancels worse
for it**. At 0.01 it keeps the signal whole and has not finished learning.

Step four moves the reference nearer the thing being measured. The coherence and
the cancellation go wrong at once, from 0.251 and 20.4 dB to 0.669 and 3.7 dB. The
signal column lies for a while: at a small leak it reads better than with no leak,
because the filter is too busy with the reference to eat it. **The coherence is the
measurement to trust — it went wrong first, went wrong steadily, and needed no
canceller to say so.**

It also draws the waveform, down the page rather than across it, with both traces
at the same scale because drawn at scales of their own they would look alike. The
filtered trace reaches 0.46 where the signal reaches 0.25, and that extra width is
the noise the filter could not reach, seen rather than read. The 4 parts in a
hundred of signal the filter ate is less than one character wide at that size, and
the example says so rather than claiming the picture shows it.

## 0.13.0 (2026-08-26)

Making the signals to test with, the numbers they are stored in, and finding
where a polynomial crosses nothing.

**`generate` makes signals without the faults that come free with them.** Every
test and example in this library used to write its own sine wave, which is fine
for a sine and a trap for anything else. A square wave written the obvious way
holds every odd harmonic out to infinity, and each one above half the sample rate
folds back to a frequency that has nothing to do with the note being played.

Measured at 8000 samples a second: the loudest thing in the answer that is **not**
a harmonic of the tone, against the tone itself.

| tone Hz | 100 | 300 | 700 | 1300 | 1900 | 3100 |
| --- | --- | --- | --- | --- | --- | --- |
| samples a turn | 80 | 27 | 11 | 6.2 | 4.2 | 2.6 |
| naive | -39.3 | -23.9 | -17.3 | -13.9 | -9.2 | -9.2 |
| `generate` | -49.3 | -33.7 | -29.6 | -39.6 | -25.7 | -39.6 |

At 1900 Hz the naive wave holds a false tone only 9 dB below the one asked for. It
does not remove the folding altogether — the best it reaches is 50 dB down and the
worst 26 — and the header says so with the same table. A test needing better than
that wants a sine.

The phase is carried and folded rather than worked out from the sample number,
which is what allows `generate_design_sweep`: a chirp visits every frequency in
one run, and one chirp through a filter shows the whole of what the filter does.

**`quantise` chooses what shape the error of a converter takes.** The error is the
same size whatever is done; what changes is whether it can be got rid of
afterwards. Measured, a sine of 300 Hz at a hundredth of full scale into 8 bits:

| | worst false tone | noise below 1 kHz | noise above it |
| --- | --- | --- | --- |
| rounded plainly | -15.6 dB | -15.2 dB | -8.0 dB |
| with dither | -30.9 dB | -7.6 dB | -2.9 dB |
| with dither and shape | -25.4 dB | -14.2 dB | +1.2 dB |

Plain rounding leaves a false tone 15.6 dB below the signal, a harmonic of it,
which **no amount of averaging removes** because it is not noise but a signal.
Dither takes 15 dB off that and leaves noise, which averages away. Shaping takes
the noise back out of the band the signal is in — it does not remove noise, it
moves it, and a signal filling the whole band gains nothing.

**`poly` finds where a polynomial crosses nothing**, which is what says whether a
filter is stable. A pole outside the unit circle is a filter whose answer doubles
every few samples until it is nothing but infinities, and `poly_is_inside_circle`
finds out before it happens.

Order 1 and 2 have a closed form, so every pole of every filter in this library is
reached exactly. The quadratic is written so no two nearly equal numbers are
subtracted, and a pair of roots twelve orders apart still comes back right.

**The order is capped, and it is not the method that caps it.** Measured at 32
bits, at order 5 the roots come back a twentieth away from the ones they were
built from **and the polynomial is still nearly nothing there** — the module found
the right roots of the wrong polynomial, because by that order the coefficients
themselves can no longer describe the polynomial that was meant. No method finds
roots the coefficients no longer hold. At 64 bits every order to 12 is exact. For
a filter this is rarely a limit, since a chain of biquads is order 2 a section.

**A trap worth recording.** Unity's assertion macros use their *expected* argument
more than once. A call that moves a generator on must be stored in a local first,
or the two sides fall out of step with each other rather than with the test. The
existing suite is safe — every case there has a constant in that position — but it
cost a while to find.

## 0.12.0 (2026-08-26)

Four modules about carrying a covariance forward and what finite precision does
to it, and one about carrying a state forward through a model written the way
models are really written.

**`eigen` finds the directions a symmetric matrix stretches**, and how far it
stretches each, by the rotations of Jacobi. Symmetric only, and on purpose:
every covariance is symmetric, so that is the case signal processing asks for,
and it is also the case that behaves.

**The error does not follow the conditioning**, which is what parts this method
from the ones that are quicker. Measured at 32 bits on matrices of order 5 built
to a chosen conditioning, checking that the matrix really stretches each
direction by its value:

| condition of the matrix | 1 | 10 | 1 000 | 100 000 | 10 000 000 |
| --- | --- | --- | --- | --- | --- |
| worst of `A·v` less `λ·v` | 0.0 | 5e-8 | 7e-8 | 9e-8 | 3e-8 |

A matrix whose widest direction is ten million times its narrowest still gives
directions right to seven digits. `eigen_condition` is the number behind two
things the library already records: why `lstsq` refuses a fit, and how far a
covariance can be trusted.

**`rls` solves the whole least squares problem at every sample.** Measured on a
filter of 16, the samples to bring the coefficients 40 dB towards the truth are
163 for `adaptive` and 24 for this. But left to run, `adaptive` reaches −149 dB
at 32 bits where `rls` stops at −97 dB, because `rls` is limited by the precision
of the matrix it carries. The choice is not which is better; it is whether the
answer is wanted quickly or wanted exactly.

**It is a module of its own because of what it costs**: at 32 bits a length of
256 takes 266 kB against 2 kB, and a length of 256 is ordinary for an echo
canceller. That had to be visible in the type.

**And because the matrix it carries can lose its footing.** Written the usual
way, with each half worked out on its own, measurement over a million samples at
32 bits shows the two halves coming to differ by more than the largest element of
the matrix, and the filter running away:

| forgetting | halves drift apart by | what happens |
| --- | --- | --- |
| 0.999 | 1.82 | fell over at sample 7230 |
| 0.990 | 1.84 | fell over at sample 987 |
| 0.950 | 1.31 | fell over at sample 216 |

This module works out one half and writes it to both, which holds them exactly
equal for nothing, and all four factors then hold for a million samples. `ukf`
holds its covariance together the same way. **Ask `rls_is_healthy`**: a filter
that has fallen apart still answers.

**`lattice` is a ladder of stages** rather than a straight list of coefficients,
so each learns at its own pace on an input whose samples lean on each other. The
measurement is recorded rather than the summary, because the summary is not the
whole of it:

| samples | 100 | 300 | 1000 | 3000 | 10000 | 30000 |
| --- | --- | --- | --- | --- | --- | --- |
| `lattice` | -10.5 | -23.0 | -34.9 | -43.0 | -43.7 | -40.0 |
| `adaptive` | -14.4 | -21.3 | -25.2 | -63.6 | -139.7 | -139.5 |
| `rls` | -14.7 | -24.4 | -30.8 | -41.9 | -52.2 | -58.1 |

There is a window from about 300 samples to about 3000 where the ladder is ahead
of both, and after it the ladder stops improving and the others do not. **Take a
ladder where the middle is all there is** — where the thing being learned changes
every few thousand samples and no filter ever reaches its floor. It also cannot
run away as `rls` can: every stage holds one number and the arithmetic holds it
between −1 and 1.

Both errors are offered. The error **a priori** is what the filter would have
said had it not been about to learn, which is the honest measure. The error **a
posteriori** is always the smaller, and reporting it where the first belongs is
how an adaptive filter comes to look better than it is.

**`propagate` carries a state forward through a rate of change.** Every
estimator here asks for a discrete step and nobody writes a model that way.
Measured on a turning with a known answer at 64 bits, halving the step halves the
error of Euler, quarters the midpoint and cuts Runge to a sixteenth — exactly, in
every case.

**At 32 bits the method outruns the width**: Runge stops at about a part in ten
million however small the step is made, because by then its own error is below
the rounding of the state itself. There is nothing to gain from a smaller step
than that, and a little to lose.

The new `continuous` example follows a pendulum through a `ukf` with the model
written as a rate. Euler is twice as far out as the other two on the state that
is never measured — and then midpoint and Runge give the same answer, because at
that step the midpoint error has already fallen below the noise on the
measurements. Carry the model well enough that it is not the worst thing in the
answer, then stop.

## 0.11.1 (2026-08-25)

Two examples and one support file were written with `int main()` rather than
`int main(void)`, which in C says nothing about what the function takes rather
than saying it takes nothing. The support of the conformation runs did not
include its own header either, so its one function had no declaration anywhere
the compiler saw.

The `-Wstrict-prototypes` and `-Wmissing-prototypes` that 0.11.0 added to the
build are what found these, on the first run after that release. They are the
whole reason those warnings were added, and the release that added them is the
release they caught.

## 0.11.0 (2026-08-25)

The shapes of filter, the order estimate, the phase, and any window for an FIR.
Also the fixes for the three faults that the property based tests of 0.10.1
brought out, and three more that measuring this release brought out.

**Four shapes of IIR filter instead of one.** Chebyshev I ripples in the band
that passes and falls faster for it; Chebyshev II keeps that band flat and puts
the ripple where it is stopped; elliptic ripples in both and falls fastest of
all. Measured on a low pass of order 8 at a cutoff of 0.1, asked for 1 dB of
ripple and a stop band 60 dB down:

| shape | at nothing | ripple | falls to 60 dB below |
| --- | --- | --- | --- |
| Butterworth | 1.000 | none | 0.209 |
| Chebyshev I | 0.891 | 1.000 dB | 0.151 |
| Chebyshev II | 1.000 | none | 0.100 |
| Elliptic | 0.891 | 1.000 dB | 0.110 |

**`iir_sections_for` says how many sections a specification needs**, which is
the whole trade in one number. To pass below 0.1 and stop above 0.15 by 60 dB
with 1 dB of ripple allowed:

| shape | sections | order |
| --- | --- | --- |
| Butterworth | 9 | 18 |
| Chebyshev I | 5 | 10 |
| Chebyshev II | 5 | 10 |
| Elliptic | 3 | 6 |

A third of the sections for the same work. Every filter in that table was built
and measured, and every one really meets what was asked.

**`iir_phase` and `iir_group_delay`, and the same two for `fir`.** The group
delay is the number that says what a filter does to the SHAPE of a waveform,
which no measurement of gain shows. Measured across the band that passes, a
Butterworth of 10 sections rises from 41 samples to 93 and an elliptic of 3
from 14 to 87: the shape that costs the fewest multiplications costs the most
here. An FIR built by any design in this library holds every frequency back by
exactly half its length less one half, and that is the reason to choose one.

**`fir_design_low_pass_with` and its two companions take any window.** The turn
of a window is a fixed number divided by the length, and measuring it at 101
coefficients and again at 201 gives the same number, which is what says it
belongs to the shape of the window alone:

| window | times the length | band that is stopped |
| --- | --- | --- |
| rectangular | 0.90 | -26 dB |
| hamming | 1.84 | -58 dB |
| hann | 1.96 | -55 dB |
| blackman | 2.40 | -75 dB |
| blackman-harris | 2.83 | -104 dB |

`fir_length_for` turns a wanted turn into a length and `fir_transition_width`
does the reverse.

### The faults that 0.10.1 found, now fixed

**`lstsq_polyfit` gave a wrong answer and said nothing.** The guard on the
diagonal of the factor cannot catch the digits lost in forming the normal
equations. Reading the answer back does not catch it either: measured over
20000 random sets at 32 bits, the answers that were right leaned on a column of
the model by as much as 8.6 parts in ten thousand and the answers that were
wrong by as little as 1.2 parts in a hundred thousand, and the two ranges lie
across each other. What does part them is the same fit done with the places
brought near zero. The module now does both and refuses where the plain one
leaves more error: not one wrong answer in 20000 sets, against 103 before, and
1.6 fits in every 100 refused. At 64 bits nothing is refused.

**`stft_fewest_frames`** says how many frames a rebuild needs, which
`stft_inverse` refused without explaining.

**`window_is_valid_size`** says which sizes a window can be built at. A
symmetric window of two values is its two ends, and the ends are where a taper
is nothing, so a hann window of 2 has a coherent gain of nothing and the header
tells callers to divide by that.

### And three that building this release brought out

**A Chebyshev I with an odd number of sections amplified the band it was meant
to pass**, reaching 1.122 where it should have reached 1. The scaling that
holds the ripple down from 1 was applied only when the number of sections was
even, and every order this module builds is even.

**An elliptic filter refused every stop band deeper than 60 dB at 32 bits.**
For a deep stop band the selectivity is very small, its square falls below what
the width can tell from nothing, and the modulus across it rounds to exactly 1,
whose measure is not finite. That measure has a closed form there, and using it
lets a 32 bit build reach 110 dB.

**The same rounding gave `iir_sections_for` an order of eight million**, which a
caller would have tried to allocate. The measure across a modulus now has one
place where it is worked out, and no answer above `IIR_LARGEST_SECTIONS` is
given back at all.

**What remains is recorded rather than hidden.** An elliptic filter at 32 bits
holds its stop band down with notches that the coefficients cannot always place
exactly, and the floor then sits up to 3 dB above what was asked. More sections
do not mend it. No other shape shows this, and no shape shows it at 64 bits.

### The build

`-Wmissing-prototypes` and `-Wstrict-prototypes` are now among the warnings the
build must pass, and the warnings check compiles with optimisation. The first
found a public function with no declaration in its header; the last found a
buffer read before it was written. `vector2d_alloc` was written with empty
brackets, which in C says nothing about what it takes rather than saying it
takes nothing.

## 0.10.1 (2026-08-25)

Property based tests for eleven modules, and the three faults that writing them
brought out. No behaviour of the library changed in this release; the only
changes to the C sources are to what the headers say.

Every module written since 0.5.0 had unit tests only. The suite goes from 68
property based tests to 177.

**The tests were chosen where a rule holds for every input, or where two ways to
the same answer can be set against each other.** `interp` keeps its two claims
for every table and not only for the one that was measured. `convolve` sets its
direct way against its way through the transform, which share no arithmetic at
all. `fft` and `bluestein` are set against a transform written plainly in
Python. `stft` rebuilds exactly inside its solid stretch for every window and hop
that `stft_can_rebuild` accepts. `lstsq` leaves an error that holds nothing of
any column of the model, which is what a least squares fit means. `quaternion`
writes an attitude as a matrix and reads it back, which reaches all four of the
branches that `quaternion_from_matrix` chooses between.

**The guard of `lstsq` does not do what its header claimed.** It was said to
refuse an answer made of rounding. It cannot: it reads the diagonal of the
factor, and by the time the factor is taken the digits have already been spent
in forming the normal equations. Measured on 14 readings whose x runs from 2 to
6, at 32 bits:

| order | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- |
| plain fit | 0.171 | 0.482 | 0.769 | **0.527** | refused |
| scaled fit | 0.171 | 0.482 | 0.769 | 0.930 | 0.983 |
| ratio of the diagonals | 0.86 | 0.75 | 0.67 | 0.64 | - |

A curve of order 4 can always do whatever a curve of order 3 did, thus the
quality can never fall as the order rises. It falls, and the guard sees a ratio
of 0.64, a thousand times above where it sits. The header now says what the
guard catches, which is columns of the model that say the same thing, and what
it does not, which is the digits lost in forming the normal equations. **Use
`lstsq_polyfit_scaled`**, which is right on the same data at the same width, and
**look at `lstsq_fit_quality` afterwards**, which is what shows this.

**`stft_inverse` refuses when there are too few frames**, and the header did not
say so. A sample in the middle of a signal is under as many blocks as fit across
it, thus the block divided by the hop is the fewest frames that leave any sample
covered fully. A block of 8 at a hop of 2 needs 4 frames, and 3 frames leave
nothing to give back.

**A window of two values is degenerate for every window that tapers.** A
symmetric window of two values is its two ends, and the ends are where a taper is
nothing. The values are right, but the header told callers to divide by
`window_coherent_gain`, and for a hann window of 2 that is a division by nothing.
Use no tapered window below a size of 3.

**The probe that compares the C structures with the Python types was never built
at 64 bits.** It could not show until a structure held a `real_t` by value, and
every structure mirrored before now held pointers alone, whose size does not
follow the width. The check that guards every other property test was examining
one width only. It is now built at the width of the library it measures.

## 0.10.0 (2026-08-24)

Everything in this release is about answering a question that one transform of
a whole recording cannot: WHEN was that frequency there, WHAT do two signals
share, and what if the size that matters is not a power of two.

**`bluestein` transforms any size.** `fft` takes a power of two, which is the
right trade when the block size is a choice. Some sizes are not a choice: at
3000 samples in a second one period of 50 hertz is 60 samples, a day is 1440
minutes, and a turn of a shaft is however many readings the machine gives.
Rounding such a size up and filling with zeros moves every bin off the
frequency the size was chosen for.

The turning factors of the method follow the SQUARE of the index, and for a
size of 200000 the last one asks for the sine of an angle near ten to the
eleventh, which a number of 32 bits cannot hold. The module folds the square
back into one turn first. Measured on a single tone at 32 bits, the worst false
answer as a part of the tone:

| size | 1 000 | 10 000 | 50 000 | 200 000 |
| --- | --- | --- | --- | --- |
| with the fold | 0.0000001 | 0.0000000 | 0.0000000 | 0.0000001 |
| without it | 0.0000076 | 0.0001036 | 0.0003396 | 0.0014121 |

Use `fft` where the size is a power of two: measured on the same size,
`bluestein` takes 5.1 times as long and gives nothing extra.

**`fft_inverse_real`** brings a half spectrum back to a signal of real values
and rebuilds the mirrored half itself. Bin 0 and the bin at half the sample rate
sit on their own mirror, thus no real signal can give either an imaginary part;
the function drops whatever is there rather than carrying it into an answer that
no real signal could have.

**`stft` cuts a signal into short overlapping pieces**, and guards the way back
twice. `stft_can_rebuild` examines the window and the hop together and refuses a
pairing that multiplies samples by nearly nothing, such as a hann window at a
hop of the whole block.

**`stft_solid_range` is the one that catches everyone.** The first sample of a
signal is under the first block alone, where a sample in the middle is under as
many blocks as fit across it, and a window that is zero at its first sample has
taken that sample away for good. Outside the solid stretch the output is set to
nothing rather than left as a number that looks like an answer. Inside it the
rebuild is exact: the worst error at 32 bits is 0.0000005, which is the rounding
of the transform itself.

**`spectrogram` turns those frames into a unit that can be read**, and corrects
the three things that are usually left out. A wave of amplitude A reads as A,
whatever the block, the window or the hop. Measured on a wave of amplitude 2:

| window | block 128 | block 512 | block 2048 |
| --- | --- | --- | --- |
| rectangular | 2.00000 | 2.00000 | 2.00000 |
| hann | 1.99999 | 2.00000 | 2.00000 |
| blackman | 2.00000 | 2.00000 | 2.00000 |

The decibel unit holds a floor under it, because the logarithm of nothing has no
value and a bin that holds nothing is a thing that happens.

**`csd` gives what two signals share**, the coherence between them, and the gain
and phase of whatever lies between them. A single block gives a coherence of
exactly 1 for any two signals whatever, because the arithmetic there is a number
divided by itself. Measured on two signals of noise with nothing in common,
where the truth is 0 at every frequency:

| blocks | 1 | 2 | 4 | 8 | 16 | 32 | 64 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| mean reading | 1.00 | 0.46 | 0.35 | 0.13 | 0.06 | 0.04 | 0.02 |

The reading falls as about one over the number of blocks, thus a reading of 0.35
is evidence of nothing if it came from 4 blocks. The module refuses below eight.

**Two examples.** `spectrogram` draws a tone sliding from 100 to 800 hertz at a
block of 64 and a block of 256, so that the trade between time and frequency can
be seen rather than believed. `coherence` separates a machine at 50 hertz from a
second machine at 53 hertz: the two fall in the SAME BIN and no spectrum can
tell them apart, while the coherence reads 0.80 for the machine that is really
shaking the floor and 0.00 for the one that is not.

**A fix.** The name `RUN_FITCURVE_EXAMPLE` was missing from `run_example.h`, and
the header itself says what that costs: a name that is not there counts as zero,
and zero is the value of `RUN_NONE`. The fitcurve example therefore gave a main
function in the default build, which is the one build that is supposed to give
none.

## 0.9.0 (2026-08-24)

Five pieces that a signal has to pass through before anything clever can be
done with it: sliding one signal along another, reading between the points of
a table, saying which peaks are real, fitting a curve through readings, and
taking the drift out of a block.

**`convolve`** slides one signal along another, in the three modes that are
wanted: the whole overlap, the middle piece of the same length as the input,
and only the part where the two fully overlap. It gives both the direct way
and the way through the transform, and `convolve_transform_size` says which
size of transform a given pair needs. Neither way takes memory of its own; the
transform and its working room come from the caller.

**`interp`** reads between the points of a table, by straight lines or by the
shape-keeping curve of pchip. The difference is not a matter of taste. On a
table that only rises, from 0 to 10, measured across 600 places:

| | reaches | goes down |
| --- | --- | --- |
| `cspline` | -1.09 to 11.08 | at 262 places |
| `interp`, pchip | 0.00 to 10.00 | nowhere |
| `interp`, linear | 0.00 to 10.00 | nowhere |

A smooth curve through a table that only rises can still go down between the
points, and 22 parts in 100 of the answer can sit outside the table it came
from. A calibration read that way reports a temperature the reference never
showed. `cspline` remains for the case where a smooth second derivative is what
is wanted; where the shape of the data is what is wanted, pchip keeps it.

**`peakdetect` gained the rules that say which peaks are real.** A height rule
alone keeps the wrong ones: measured, a wobble at a height of 7.9 has a
prominence of 0.2, and a lone peak at 6.0 has a prominence of 5.0, thus any
height rule that keeps the first drops the second. On ten beats carried on
noise, the local maxima number more than a hundred and the prominence and
distance rules give exactly ten. `peakdetect_prominence` gives how far a peak
stands above the ground around it, `peakdetect_width` gives how wide it is at a
given part of its height, and `peakdetect_find` applies height, prominence,
width and distance together.

**`lstsq`** fits a curve through more readings than it has room for, on top of
the factor of Cholesky. It refuses rather than answering badly: it reads the
diagonal of the factor, and where two columns say so nearly the same thing that
the answer would be made of rounding, it says no. Measurement puts that refusal
one order past the last order that still follows the readings.

**Where the x of the readings sits matters as much as the order.** The same 60
points, the same width, the highest order that still follows them to three
digits:

| x runs from | 32 bits | 64 bits |
| --- | --- | --- |
| 0 to 1 | 5 | 11 |
| -1 to 1 | 10 | 23 |

Move 50 points to x from 1000 to 1001 and even a cubic is refused, at 64 bits,
on data that fits perfectly. A thermistor read in ohms or a sensor read as a
count from a converter is exactly that case, thus `lstsq_polyfit_scaled` brings
x to a range of about -1 to 1 first and gives back the centre and the width it
used. The new `fitcurve` example runs a plain fit and a scaled fit through the
same calibration, and shows what the scaled coefficients give when they are
read the wrong way.

**`detrend`** takes the level, or the level and the drift, out of a block. It
is the block cousin of `dcblock`, which does the same for a stream. Before a
transform it is not optional: on a wave of one unit at bin 8 with a drift of 4
units across the block, bin 1 holds 1.27 with the drift left in and 0.08 with
it taken out, and bin 1 holds no signal at all.

**It takes a little of the signal along with the drift, and that is recorded
rather than hidden.** A wave even about the middle of the block loses `3/(n+1)`
of itself. A wave odd about it loses about 3 divided by pi times the number of
periods it makes across the block, and the length of the block does not come
into it:

| periods across the block | 1 | 4 | 16 | 32 |
| --- | --- | --- | --- | --- |
| part of the wave taken | 0.95 | 0.24 | 0.06 | 0.03 |

A wave of one period across a block is a drift, as far as anything looking at
that block can tell, and no method can separate the two. The answer is to keep
the block long compared with the lowest frequency wanted.

## 0.8.0 (2026-08-24)

Three pieces that belong together: a factor that shapes a spread, a filter that
uses it to follow a state through a model that bends, and a way of holding
which direction something points that has no hole in it.

**`matrix_cholesky`** gives the lower triangle whose product with its own
transpose gives the matrix back. Its use is that a covariance says how far a
set of numbers spreads, and the factor is the SHAPE of that spread: a step of
unit length multiplied by it lands on the edge of the spread, whichever way it
points. A test holds exactly that, taking unit steps all the way round a circle.

**`ukf`**, the unscented Kalman filter, follows a state through a model that
bends without taking a derivative at all. It puts a handful of chosen points
through the model itself and looks at where they land. Measured, a spread put
through a square, where the true middle of what comes out is the middle squared
plus the spread:

| middle in | spread in | truth | `ukf` | a straight line |
| --- | --- | --- | --- | --- |
| 0.0 | 9.0 | 9.0 | 9.0 | 0.0 |
| 3.0 | 1.0 | 10.0 | 10.0 | 9.0 |

The header says what that does not mean as plainly as what it does: for a
smooth model with many measurements both this and `ekf` settle to the same
answer, and `ekf` often gets there with slightly less work. The gain is in one
step through a bend, and in needing no derivative.

**How small alpha may be follows the width of the build.** The weights of the
points are about `1/(alpha*alpha*nx)` and must add to 1, thus a small alpha
makes very large weights adding to a very small number. At 32 bits and an alpha
of 0.001, which the literature gives, the weights come out 6 percent wrong
before the filter has done anything. `UKF_DEFAULT_ALPHA` therefore follows the
width, and `ukf_is_valid_spread` says whether a given alpha can be held.

**`quaternion`** holds which way something is pointing as four numbers. Three
angles have a hole in them at a pitch of straight up, which is where an
aircraft, a robot arm and a camera all go on purpose, and it is not a rounding
trouble that a wider number would fix. A rotation matrix has no hole and holds
nine numbers keeping six rules that arithmetic wears away; four numbers keep
one rule, and a test carries an attitude through 100 000 steps and finds its
length still 1.

**An example for both.** `attitude.c` gives the filter a real model: an
attitude from a gyroscope and an accelerometer. It reports the tilt apart from
the total, because an accelerometer can answer the first and can say nothing
about the second. The gyroscope alone drifts to 25 degrees of tilt in thirty
seconds where the filter holds it under one.

### Feat

- **matrix**: Add the factor of Cholesky
- **quaternion**: Add which way something points
- **ukf**: Add the unscented Kalman filter

### Fix

- **ukf**: Give the turned gain a matrix of its own shape

## 0.7.1 (2026-08-23)

### Docs

- **examples**: Add an example for every module that had none

Twenty-two modules of thirty-seven had no example. Ten are added, each standing
on a real device with a real question, and each showing the trap that makes its
module easy to use wrongly: a bearing sensor logged at a lower rate, a pump
watched for a failing bearing, two microphones finding a direction, a voice
over a fan, a load cell that drifts and is knocked, a pressure pulse measured
for its width, a trolley on a rail, blocks from a converter, a thermistor, and
three jobs that need more than a matrix of plain numbers.

Every module of the library now appears in an example, and the workflow builds
and runs all nineteen at both widths.

## 0.7.0 (2026-08-23)

Six modules join the library. Each one answers a question that a caller could
previously only answer by writing the arithmetic by hand, and each one carries
the trap that makes that arithmetic easy to get wrong.

**`correlate`** — how alike two signals are at each lag. Three questions are
one question with different signals put into it: how long is the delay between
two recordings, does this signal repeat and how often, and is this shape in
that signal. `correlate_best_lag` is the whole of finding a period in one call,
and it gives a strength that can be judged rather than only compared.

**`psd`** — how much power at each frequency, by the method of Welch. The
scaling is the part that is usually wrong, and it is the reason this is a
module and not a page of notes. Three corrections are needed, and with all
three a wave of amplitude A has an area under the curve of `A*A/2` whatever the
block, the window or the overlap.

**`hampel`** — replacing only the samples that are wrong. A median filter
removes a spike and changes every other sample it touches; this changes only
the samples it has a reason to change. The spread is a median absolute
deviation and not a standard one, because a standard deviation is moved by the
very samples it is meant to catch.

**`adaptive`** — a filter that finds its own coefficients. It takes away noise
that a second sensor measures on its own, which works where no filter of
frequency can, because the noise and the signal may hold the same frequencies.

**`resample`** — changing the rate of a signal. Keeping every fourth sample
looks like the whole of it and is the half that goes wrong: a frequency above
the new rate does not disappear, it comes back at a frequency it never had and
nothing afterwards can find out. Only the samples that are kept are worked out,
thus the filtering costs the output rate and not the input rate.

**`filtfilt`** — filtering with no delay, by running the filter both ways. The
peak of a heartbeat stays where it was. Both prices are stated: the whole
signal must be in hand, and the gain is squared.

Every module states what it cannot do as plainly as what it can, and the tests
hold those statements rather than leaving them as claims in a comment: that a
reference holding the signal makes an adaptive filter remove the signal, that
throwing samples away without a filter makes a false tone, that a three
deviation rule lets a small fault through when a large one stands beside it.

### Feat

- **adaptive**: Add a filter that finds its own coefficients
- **correlate**: Add how alike two signals are at each lag
- **filtfilt**: Add filtering with no delay by running the filter both ways
- **hampel**: Add replacing only the samples that are wrong
- **psd**: Add the power at each frequency by the method of Welch
- **resample**: Add changing the rate of a signal

## 0.6.2 (2026-08-23)

### Fix

- **filter**: Make the lowest cutoff follow the width of the build

`IIR_MIN_CUTOFF` and `DCBLOCK_MIN_CUTOFF` were each one number, written when
the library had one width. When the width became a choice they stayed where
they were, thus a build at 64 bits was refused filters that it can hold
exactly. A high pass at 0.5 Hz against 32 kHz is a cutoff of 0.000016: out of
reach at 32 bits, and nothing at all at 64.

Both limits are now a thousand times lower at 64 bits, and the guides hold the
measurement that sets each one.

## 0.6.1 (2026-08-23)

### Fix

- **property**: Ask the bindings for a buffer instead of building one

The property tests that examine the peak and the valley detection built the
buffers they hand to the library as `ctypes.c_float`. That was right while the
library held every number in a float and wrong the moment it could hold one in
a double, thus five tests could not run at all in a 64 bit build. They now ask
`sptk.real_buffer` for the memory, and no test names a type of `ctypes` any
more.

## 0.6.0 (2026-08-23)

**Every signature that held a float now holds a real_t.** This breaks every
caller, and the change is mechanical: a program that spelled `float` for a
value of the library spells `real_t` instead.

The library spelled `float` in six hundred places and `double` in three
modules. That made the width of a number a decision taken module by module, and
it made the accuracy of the library a thing a caller could not choose. Both are
now one decision, taken one time for the whole build:

```bash
cmake -S . -B build                        # 32 bits, the default
cmake -S . -B build -DSPTK_REAL_64=ON      # 64 bits
```

The option is `PUBLIC`, because a program and a library that disagreed about
the width would not fail to build and would give nonsense.

**Write every number with `REAL_C`.** A number written as `0.5` in a 32 bit
build quietly makes the arithmetic around it run in 64 bits and then throws the
extra away; measured on one line, that turned three instructions into six. A
number written as `0.5f` in a 64 bit build is rounded to seven digits before
the wider arithmetic ever sees it. `REAL_C(0.5)` writes the right one.

**Give `real_sin` and not `sinf` to `pmatrix`.** That module holds a function
for each of its elements. A caller could give it `sinf` directly, and after
this change that still builds and gives nonsense at 64 bits, because a function
that takes a float is called through a pointer that takes a `real_t`. The
functions `real_sin`, `real_cos`, `real_tan`, `real_sqrt`, `real_exp`,
`real_log` and `real_abs` have addresses that always agree with `real_t`.

**What the narrower width costs is written down.** The three modules that held
a double now hold a `real_t` like everything else. The tests record the cost
rather than hide it, and each one holds a different number for each width:

| measurement | 32 bits | 64 bits |
| --- | --- | --- |
| `stats_variance`, five samples at eight million, true value 2 | 2.25 | 2.00 |
| `movavg` deviation, the same samples, true value 1.414 | 1.50 | 1.414 |

The `dcblock` module keeps its worth at both widths, and the measurement now
says why more clearly: what it gains over a section comes from being one pole
and not from any wider number.

**Both widths are examined.** The unit tests, the property based tests, the
builds, the examples, the benchmark and the compiler warnings all run at both
widths in the workflow. A fault can live in one width and not in the other.

### Feat

- **real**: Hold every number in one type whose width the build chooses

## 0.5.0 (2026-08-19)

Six modules join the library, and two faults that gave a wrong answer without
saying so are put right. Every one of them answers a gap that showed itself
while a whole chain was built with the library rather than a single module.

**A design now says when it cannot hold what it was asked for.** Both faults
were of one kind: the library was given a filter it could not build, and it
built something else and said nothing.

A section of an infinite impulse response holds its poles near the circle when
the cutoff is low, and a float holds about seven digits. Below a cutoff of
0.001 of the sample rate those digits run out. The gain that should be 1 at
zero frequency falls to 0.685 at a cutoff of 0.0001. A filter with a finite
impulse response turns from passing to stopping over a band about `2/length`
wide, thus a cutoff nearer to 0 than that has no room for the turn: for 101
coefficients the gain in the pass band falls to 0.507 at a cutoff of 0.005.

Every design function now gives a `bool`, and gives `false` and leaves the
filter as it was when it cannot build what was asked. `iir_is_valid_cutoff`,
`fir_is_valid_cutoff` and `fir_is_valid_band` answer the same question first.
A caller that ignores the answer builds as it did before.

**The size of a reading matters more than it looks.** A reading that sits at
eight million counts with a signal of a few thousand on top spends six of the
seven digits of a float on the part that carries nothing, and a high pass then
lifts the rounding error by about two hundred times. The new `dcblock` module
follows the level in double and hands back the difference, which takes that
error away and holds a cutoff a thousand times lower than a section can.

### Feat

- **dcblock**: Add the tracker that takes the level of a signal away
- **iir**: Add the band pass, the band stop, the notch and the peak
- **medfilt**: Add the median of the last samples
- **movavg**: Add the mean of the last samples in a fixed time
- **ringbuf**: Add a buffer that holds the last samples
- **stats**: Add the measures of a list of samples
- **window**: Add the windows that a transform needs

### Fix

- **filter**: Refuse a cutoff that the design cannot hold

## 0.4.0 (2026-08-17)

**Every include path changes.** The modules of the library moved from the root
of the repository into `sptk/`, grouped by the area of work. An include now
names that area:

```c
#include <matrix/matrix.h>          // 0.3.0
#include <sptk/linalg/matrix.h>     // 0.4.0
```

The name of every function, type and macro stays as it was. Only the paths
move. The table at the head of README.md gives the area of each module.

### Fix

- **perf**: Remove the variables that nothing reads, and widen the warnings job

### Refactor

- Group the modules of the library by their area of work

### Docs

- **scripts**: Correct the number of faults that the check finds
- **examples**: Build each example on a real device and a real question
- Give each area of the library a guide that says how its modules work

## 0.3.0 (2026-08-16)

### Feat

- **goertzel**: Add Goertzel, the wavelet transform and the filter of Savitzky and Golay
- **ekf**: Add the extended Kalman filter
- **fir**: Add filters with a finite and an infinite impulse response
- **hilbert**: Add the Hilbert transform and complete the Hilbert-Huang transform
- **fft**: Add the fast Fourier transform

### Fix

- **examples**: Restore the names of the new examples

### Docs

- **examples**: Add an example for each of the new modules
- **readme**: Describe the library by what it does and not by its matrices
- **api**: Give each module its own file
- **readme**: Describe the modules of the library by their area

### Build

- **cmake**: Give the version of the project to CMake

## 0.2.0 (2026-08-16)

### Feat

- **pmatrix**: Add support for a matrix with a parameter
- **cmatrix**: Add support for matrices of complex numbers
- **matrix**: Add operations that write into a matrix that already holds memory
- **kalman**: Complete the Kalman filter
- **matrix**: Add the subtract operation for two matrices
- Add support for computing inverse of a matrix using Gaussian-Jordan elimination method

### Fix

- **cz**: Correct the tag format in the commitizen configuration
- Remove three unbounded memory accesses
- **cmake**: Give a library target that always builds
- **perf**: Add the missing include for the random number function
- **cspline.c**: Correct the interval index of the interpolation
- **matrix.c**: Add a partial pivot to the inverse operation
- **matrix.c**: Correct the row count in the get column function
- **matrix.c**: Correct the memory release in the determinant function
- **cspline.c**: Fix memory leak when cspline coefficients are computed during initialization

### Perf

- **benchmark**: Add a benchmark that measures the speed of each operation
- **conformation**: Add the conformation tests for the matrix and the vector
- **conformation**: Add the support code for the conformation tests

### Refactor

- Remove every warning of the compiler
- **perf**: Give the conformation code names in the style of the kernel
- Modify the example configuration to run nothing by default
- Modify the default behavior of the example code to run nothing
- Modify print function to accept print function pointer in structure defined in callback.h
- Modify the source to make it testable

### Docs

- **api**: Describe every function of the interface

### Test

- **property**: Add property based tests with Hypothesis and pytest
- **unity**: Add unit tests for the sift operation and the release functions
- **unity**: Add unit test for the Kalman filter implementation
- **example**: Add example for using the matrix_inverse method
- **unity**: Add unit test for Emphirical Mode Decomposition implementation
- **cmock**: Add mock files for cspline.h, imf.h, peakdetect.h and valleydetect.h
- **unity**: Add unit tests for intrinsic mode function implementation
- **unity**: Add unit test for cspline implementation

### Build

- **ceedling**: Correct the test build so that every test executable links
- **cmake**: Add an optional target for the conformation tests
- Add tests/emd directory to the project configuration
- Add tests/imf directory to project configuration
- Add tests/cspline directory to project configuration

### CI

- Add a workflow that runs the tests and examines the names
- Add commitizen configuration file

## 0.1.0 (2025-03-03)

### Feat

- Add valley detection functionality and unit tests
- Add peak detection functionality and unit tests
- Add callback header file with print function type definition
- Add tests/vector2d directory to project configuration and updated mock prefix to 'Mock_'
- Add common definitions and assertion macros for matrix operations
- **incomplete**: Add kalman filtering implementation
- Add faster determinant calculation implementation

### Refactor

- Modify binary search implementation for improved clarity and performance
- Modify vector_printf function to use print_t callback type
