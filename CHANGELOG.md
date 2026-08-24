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
