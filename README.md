# ffitt

**Something great ruined by it.**

A C library for signal processing.

The library takes a signal apart, cleans it, and follows what it describes. It
holds the transforms that show which frequencies a signal carries, the filters
that take a band of frequencies away, the decompositions that split a signal
into simpler parts, and the estimators that follow a state behind a noisy
measurement. Underneath them sit the vectors, the matrices and the
interpolation that they need.

It takes no other library with it, it needs nothing from the heap unless the
caller asks for it, and it holds every number in one type that is built as a
float or as a double.

## The name

Take the `it` out of the name and what is left is `fft`, a key tool of signal
processing. The name is also symmetric about the `i`, the imaginary number.

## What the library gives

The library lies under [ffitt](ffitt), and each area of work has its own
directory there:

| Area | Directory | Modules | What you do with them |
| --- | --- | --- | --- |
| Transforms | `ffitt/transform` | `fft`, `bluestein`, `window`, `psd`, `csd`, `stft`, `spectrogram`, `correlate`, `convolve`, `goertzel`, `hilbert`, `hht`, `dwt`, `dct`, `cepstrum` | Find which frequencies a signal holds, and where |
| Filters | `ffitt/filter` | `fir`, `iir`, `savgol`, `movavg`, `medfilt`, `dcblock`, `detrend`, `hampel`, `adaptive`, `rls`, `lattice`, `resample`, `filtfilt`, `farrow` | Take a band of frequencies away, or smooth a signal |
| Estimation | `ffitt/estimate` | `kalman`, `ekf`, `ukf`, `propagate`, `pll` | Follow a state behind a noisy measurement |
| Decomposition | `ffitt/decompose` | `emd`, `imf` | Split a signal into simpler parts |
| Detection | `ffitt/detect` | `matched`, `delay`, `changepoint` | Say whether an event is in a reading, and when |
| Interpolation | `ffitt/interpolate` | `cspline`, `interp` | Give a smooth curve through a set of points |
| Linear algebra | `ffitt/linalg` | `matrix`, `cmatrix`, `pmatrix`, `cnum`, `quaternion`, `eigen`, `poly`, `lstsq`, `vector`, `vector2d` | The arithmetic that the areas above need |
| Utilities | `ffitt/util` | `generate`, `curve`, `quantise`, `stats`, `binarysearch`, `peakdetect`, `valleydetect` | Make a signal to test with, and find a place, a peak or a valley in a list |
| Core | `ffitt/core` | `real`, `ringbuf`, `callback`, `defs`, `point2d` | The type that holds every number, a buffer of the last samples, and the types that every module shares |

Include a module by its area:

```c
#include <ffitt/transform/fft.h>
#include <ffitt/filter/fir.h>
```

Each area holds a guide that says **how** its modules work and which one to
reach for: [transform](ffitt/transform), [filter](ffitt/filter),
[estimate](ffitt/estimate), [decompose](ffitt/decompose),
[detect](ffitt/detect), [interpolate](ffitt/interpolate),
[linalg](ffitt/linalg), [util](ffitt/util), [core](ffitt/core).

[docs/API.md](docs/API.md) gives the exact name and shape of every function.
The directory [examples](examples) holds a small program for each area. The
tests under [tests](tests) follow the same areas as the library.

## Three rules that shape the library

**It needs no external library.** It uses the C standard library only. Some
test tools need other software, but the library itself does not. A step of the
workflow reads the shared object and stops if it asks for anything but the C
library and the mathematics library.

**It can run without a heap.** Each module gives two ways to get memory: one
that uses the heap, and one that takes memory that the caller already holds.
The second way lets the library work on a target that has no heap. Where an
operation would need memory while it runs, the module gives a second form that
writes into a result that the caller gives, such as `matrix_multiply_into`
beside `matrix_multiply`.

**It holds every value in one type, and that type has one width for the whole
build.** Every sample, every coefficient and every result is a `real_t`. No
module anywhere spells `float` or `double`, thus no two modules can disagree
about how wide a number is.

The width is 32 bits by default and 64 bits with `-DFFITT_REAL_64=ON`. At 32
bits a number keeps about 7 digits and at 64 bits about 16. Each module says in
its header where that limit matters, and the tests hold both widths, so that
what the narrower one costs is written down. The guide of
[core](ffitt/core) says how to choose.

## An example

Take a noisy signal, filter it, and look at what frequencies are left:

```c
#include <ffitt/filter/fir.h>
#include <ffitt/transform/fft.h>

fir_t fir = fir_alloc(31);
fir_design_low_pass(&fir, 0.1f);
fir_process_block(&fir, noisy, clean, SIZE);

fft_t fft = fft_alloc(SIZE);
cnum_t spectrum[SIZE];
float magnitude[SIZE];

fft_forward_real(&fft, clean, spectrum);
fft_magnitude(spectrum, magnitude, SIZE);
```

## Build

```bash
cmake -S . -B build && cmake --build build
```

This gives the static library `libffitt.a`, with every number held in 32
bits.

### The width of a number

The library holds every number in `real_t`. One option decides whether that is
32 bits or 64:

```bash
cmake -S . -B build -DFFITT_REAL_64=ON && cmake --build build
```

Take 32 bits on a small processor: a number is half the memory, and a processor
with no unit for 64 bit arithmetic runs the wider build tens of times more
slowly. Take 64 bits where the numbers are large, the filters are slow, or the
answer matters more than the time.

The option is `PUBLIC`, thus a program that links against the library is built
the same way. A program and a library that disagreed about the width would not
fail to build, and would give nonsense.

The guide of [core](ffitt/core) gives the measured cost of each width.

To build an example program, first choose one in `examples/run_example.h`, and
then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
./build/ffitt_example
```

The directory [examples](examples) says which examples there are.

## Tests

The repository holds three kinds of test.

**Unit tests** with Ceedling and Unity examine known examples:

```bash
ceedling test:all
```

**Property based tests** with Hypothesis examine rules that must hold for every
input. See [tests/property/README.md](tests/property/README.md):

```bash
python3 -m venv .venv && .venv/bin/pip install -r tests/property/requirements.txt
.venv/bin/pytest tests/property/
```

**Conformation tests** compare the results with the GNU Scientific Library.
They need GSL, thus they are off by default:

```bash
cmake -S . -B build -DBUILD_CONFORMATION_TESTS=ON && cmake --build build
```

## Benchmark

The benchmark measures how long each operation takes, for several sizes of
input. It needs no external library:

```bash
cmake -S . -B build -DBUILD_BENCHMARK=ON && cmake --build build && ./build/benchmark
```

Each line gives the time of the fastest repeat, the mean time of all the
repeats, and the number of times the operation runs in one second. The fastest
repeat gives the best picture, because the other work of the system only makes
a repeat slower. The seed of the random numbers is the same at each run, thus
two runs compare with each other.

### What each operation costs

Every module of the library is here, with the operation a caller reaches for
most. The times were measured on one machine with GCC 13.3 and NO OPTIMISATION
ASKED FOR, which is what the build above gives. They are for comparing one
operation against another and not for promising a speed on another machine.

`scripts/benchmark_table.py` builds the benchmark at both widths, runs it, and
writes this table. Thus what the table says and what the benchmark measures
cannot drift apart. `--check` gives 1 when the table is out of date.

The 64 bit column is not always the slower one. A double is twice the memory of
a float, which costs, but a float must be widened before some arithmetic and
narrowed after it, which costs as well.

<!-- BENCHMARK TABLE BEGINS. scripts/benchmark_table.py writes it. -->

| Module | What it does | 32 bit | 64 bit |
|---|---|---:|---:|
| `ringbuf` | put one sample into a buffer of 256 | 0.03 us | 0.03 us |
| `ringbuf` | read a sample from a hundred steps ago | 0.03 us | 0.03 us |
| `ringbuf` | write the whole buffer of 256 out in order | 1.76 us | 1.76 us |
| `real` | one sine, at the width the library was built for | 0.03 us | 0.03 us |
| `real` | one square root | 0.02 us | 0.02 us |
| `real` | one exponential | 0.03 us | 0.03 us |
| `matrix` | add two 8 by 8 matrices | 0.96 us | 0.96 us |
| `matrix` | subtract two 8 by 8 matrices | 0.95 us | 0.95 us |
| `matrix` | multiply two 8 by 8 matrices | 5.22 us | 5.24 us |
| `matrix` | transpose an 8 by 8 matrix | 0.62 us | 0.62 us |
| `matrix` | multiply two 8 by 8 matrices into the caller's memory | 5.11 us | 5.15 us |
| `matrix` | invert an 8 by 8 matrix | 15.71 us | 15.79 us |
| `matrix` | the determinant of an 8 by 8 matrix | 4.07 us | 4.11 us |
| `vector` | the dot product of two vectors of 1024 | 2.53 us | 2.54 us |
| `vector` | the length of a vector of 1024 | 2.53 us | 2.54 us |
| `cmatrix` | multiply two 10 by 10 complex matrices | 26.07 us | 63.49 us |
| `cmatrix` | the determinant of a 10 by 10 complex matrix | 12.70 us | 30.22 us |
| `cmatrix` | invert a 10 by 10 complex matrix | 56.63 us | 138.24 us |
| `cnum` | multiply two complex numbers | 0.03 us | 0.03 us |
| `cnum` | divide one complex number by another | 0.04 us | 0.05 us |
| `cnum` | the size of a complex number | 0.03 us | 0.04 us |
| `eigen` | the eigenvalues and vectors of a 10 by 10 matrix | 4.12 us | 3.79 us |
| `eigen` | how badly conditioned that matrix is | 0.05 us | 0.05 us |
| `lstsq` | fit a curve of order 8 through 256 readings | 218.87 us | 219.49 us |
| `lstsq` | how well that curve holds the readings | 8.48 us | 8.74 us |
| `poly` | read a curve of order 8 at one place | 0.05 us | 0.05 us |
| `poly` | multiply two curves of order 8 | 0.27 us | 0.27 us |
| `poly` | every root of a curve of order 4 | 3.43 us | 10.03 us |
| `quaternion` | join two turns into one | 0.04 us | 0.03 us |
| `quaternion` | turn one point by a quaternion | 0.03 us | 0.03 us |
| `quaternion` | the same turn written as a 3 by 3 matrix | 0.09 us | 0.09 us |
| `quaternion` | the turn part way between two others | 0.16 us | 0.17 us |
| `quaternion` | carry a turn forward by a rate over a step | 0.11 us | 0.11 us |
| `pmatrix` | read a 10 by 10 matrix of functions at one place | 1.26 us | 1.26 us |
| `matrix` | the factor of Cholesky of a 10 by 10 matrix | 3.65 us | 3.70 us |
| `vector2d` | the dot product of two vectors of two | 0.03 us | 0.03 us |
| `vector2d` | the length of a vector of two | 0.03 us | 0.03 us |
| `cspline` | build a cubic spline through 512 points | 17.76 us | 21.45 us |
| `cspline` | read one point from a spline of 512 | 0.08 us | 0.10 us |
| `interp` | read one place from a table of 256, straight lines | 0.03 us | 0.03 us |
| `interp` | the same, smooth and never above the neighbours | 0.03 us | 0.03 us |
| `interp` | read 4096 places from that table at one call | 0.03 us | 0.03 us |
| `fft` | a 1024 point transform of a real signal | 170.30 us | 264.18 us |
| `fft` | a 1024 point transform of a complex signal | 162.81 us | 256.66 us |
| `fft` | a 1024 point inverse transform | 191.15 us | 306.62 us |
| `fft` | the size of every bin of a 1024 point spectrum | 4.86 us | 5.21 us |
| `bluestein` | a 1000 point transform, a size no power of two | 820.95 us | 1309.62 us |
| `dct` | a 1024 point cosine transform | 11909.43 us | 18039.26 us |
| `dct` | a 1024 point inverse cosine transform | 18695.38 us | 24987.68 us |
| `dwt` | one level of a wavelet over 1024 samples | 11.57 us | 12.28 us |
| `dwt` | rebuild 1024 samples from one wavelet level | 7.43 us | 7.19 us |
| `dwt` | four levels of a wavelet over 1024 samples | 25.20 us | 26.51 us |
| `dwt` | take the small wavelet values out of 1024 | 1.91 us | 1.89 us |
| `window` | build a window of Blackman and Harris over 1024 | 33.28 us | 58.14 us |
| `window` | put a window over 1024 samples | 2.55 us | 2.55 us |
| `window` | the noise bandwidth of a window of 1024 | 2.60 us | 2.65 us |
| `hilbert` | the analytic signal of 1024 samples | 369.99 us | 584.69 us |
| `hilbert` | the envelope of 1024 samples | 6.63 us | 16.90 us |
| `hilbert` | the frequency at each of 1024 samples | 49.14 us | 53.18 us |
| `cepstrum` | the cepstrum of 1024 samples | 393.69 us | 633.66 us |
| `cepstrum` | find the pitch in a cepstrum of 1024 | 0.88 us | 0.90 us |
| `goertzel` | watch one frequency over a block of 1024 | 6.48 us | 6.48 us |
| `goertzel` | read how much of that frequency was there | 0.03 us | 0.03 us |
| `correlate` | 4096 samples against themselves at every lag | 21122.19 us | 21243.04 us |
| `correlate` | the same by the transform | 3782.02 us | 5940.95 us |
| `correlate` | 4096 samples against another signal at 512 lags | 4996.05 us | 5009.27 us |
| `correlate` | find the period of 4096 samples | 13621.09 us | 13903.24 us |
| `convolve` | slide a shape of 512 along 4096 samples | 6405.12 us | 6447.49 us |
| `convolve` | the same by the transform | 5566.86 us | 8751.38 us |
| `psd` | the spectrum of 4096 samples by the way of Welch | 1113.87 us | 1700.13 us |
| `psd` | the power between two frequencies of that spectrum | 1.16 us | 1.12 us |
| `csd` | the cross spectrum of two signals of 4096 | 2341.83 us | 3618.57 us |
| `csd` | how much two signals of 4096 agree, bin by bin | 2342.04 us | 3616.55 us |
| `csd` | the transfer between two signals of 4096 | 2342.77 us | 3618.50 us |
| `stft` | 4096 samples into frames of 256, hopping 64 | 2177.67 us | 3331.12 us |
| `stft` | rebuild the 4096 samples from those frames | 2667.83 us | 4201.02 us |
| `spectrogram` | a picture in decibel from those frames | 124.76 us | 227.87 us |
| `spectrogram` | hold that picture against its own loudest point | 35.13 us | 35.43 us |
| `hht` | amplitude and frequency of one mode of 1024 | 426.47 us | 659.25 us |
| `hht` | the mean frequency of that mode | 3.30 us | 3.32 us |
| `fir` | design a low pass of 33 coefficients | 0.82 us | 1.30 us |
| `fir` | 4096 samples through a filter of 33 with no feedback | 415.58 us | 417.80 us |
| `iir` | design a low pass of two sections | 0.10 us | 0.13 us |
| `iir` | 4096 samples through a filter of two sections | 77.72 us | 81.47 us |
| `filtfilt` | 4096 samples both ways, thus with no delay at all | 153.35 us | 153.98 us |
| `filtfilt` | the same with a filter of 33 and no feedback | 855.76 us | 864.43 us |
| `adaptive` | 4096 samples through a filter of 32 that learns | 2169.41 us | 2178.54 us |
| `rls` | 4096 samples through a filter of 32 that learns fast | 70745.32 us | 71998.09 us |
| `lattice` | 4096 samples through eight stages that learn | 1146.62 us | 1227.90 us |
| `savgol` | design a smoother of 33 that keeps a cubic | 10.99 us | 11.09 us |
| `savgol` | smooth 4096 samples and keep the shape of the peaks | 484.05 us | 483.26 us |
| `medfilt` | the middle value of a window of 33 over 4096 samples | 847.82 us | 844.66 us |
| `hampel` | take the wild readings out of 4096 samples | 5117.78 us | 5106.74 us |
| `dcblock` | take the standing level out of 4096 samples | 23.73 us | 23.76 us |
| `detrend` | find the drift under 4096 samples | 32.24 us | 31.71 us |
| `detrend` | find that drift and take it away | 62.79 us | 61.65 us |
| `detrend` | take a drift already known away from 4096 samples | 30.48 us | 29.84 us |
| `farrow` | delay 4096 samples by half a sample | 393.01 us | 396.65 us |
| `resample` | 4096 samples down to a quarter of the rate | 676.45 us | 673.78 us |
| `resample` | 4096 samples up to four times the rate | 2869.69 us | 2910.24 us |
| `movavg` | one sample through a moving mean of 64 | 0.05 us | 0.05 us |
| `fir` | one sample through an equal fir of 64, the mean the slow way | 0.21 us | 0.21 us |
| `kalman` | one prediction of a Kalman filter over four states | 2.21 us | 2.23 us |
| `kalman` | one full step of a Kalman filter over four states | 4.60 us | 4.61 us |
| `ekf` | one step of a bending filter over four states | 5.49 us | 5.51 us |
| `ukf` | one step of a filter that places points, four states | 10.44 us | 10.39 us |
| `pll` | hold a loop on a tone through 4096 samples | 0.02 us | 0.03 us |
| `propagate` | carry four states one step by the way of Runge | 0.11 us | 0.10 us |
| `propagate` | carry four states across a hundred such steps | 8.98 us | 9.06 us |
| `changepoint` | watch 4096 readings for the moment a level moves | 43.44 us | 43.73 us |
| `matched` | look for a shape of 64 all through 4096 samples | 660.16 us | 660.38 us |
| `matched` | find where that shape fits best | 660.82 us | 664.61 us |
| `delay` | how far two signals of 1024 stand apart, by lag | 3192.81 us | 3196.34 us |
| `delay` | the same to a fraction of a sample, by phase | 344.68 us | 533.62 us |
| `emd` | sift 256 samples into three modes | 3.08 us | 3.04 us |
| `imf` | take the memory for a mode of 1024 points | 0.12 us | 0.08 us |
| `stats` | the mean of 4096 readings | 10.01 us | 10.01 us |
| `stats` | how far 4096 readings spread | 20.03 us | 20.02 us |
| `stats` | the root mean square of 4096 readings | 10.02 us | 10.02 us |
| `stats` | the middle of 4096 readings | 58.22 us | 58.14 us |
| `stats` | the spread of 4096 readings, wild ones and all | 178.53 us | 178.75 us |
| `peakdetect` | every peak in 4096 samples | 18.12 us | 16.81 us |
| `peakdetect` | the peaks in 4096 samples that pass the rules | 1648.88 us | 1628.88 us |
| `peakdetect` | how far one peak stands above its neighbours | 0.03 us | 0.03 us |
| `valleydetect` | every valley in 4096 samples | 17.01 us | 22.88 us |
| `generate` | make 4096 samples of a sine | 60.92 us | 81.32 us |
| `generate` | make 4096 samples of a square wave with no aliases | 90.94 us | 90.81 us |
| `generate` | make 4096 samples of noise | 51.50 us | 50.69 us |
| `quantise` | round 4096 samples to 12 bits | 55.26 us | 54.64 us |
| `quantise` | the same with a little noise added first | 139.98 us | 139.33 us |
| `curve` | draw a bell curve over 4096 places | 72.82 us | 87.66 us |
| `curve` | read a bell curve at one place | 0.04 us | 0.04 us |
| `binarysearch` | find a value in a sorted list of 256 | 0.05 us | 0.05 us |

<!-- BENCHMARK TABLE ENDS. -->

## Documentation

[docs/API.md](docs/API.md) is the index. Each module has its own file under
[docs/api](docs/api), which describes every type, macro and function of that
module.

The files come from the comments in the headers, thus the documentation and the
code cannot say two different things. After a change to a header, make the files
again:

```bash
python3 scripts/api_doc.py
```

To examine that every function has a comment, that the files are current, that
no file belongs to a module that went away, and that every area holds a guide:

```bash
python3 scripts/api_doc.py --check
```

## Naming

The names follow the scheme of the Linux kernel:

- A name is in lower case, with an underscore between the words.
- The name of a function starts with the name of its module, thus `matrix_add`
  and not `add_matrix`. The name of the file is the name of the module, thus
  every function of `ffitt/linalg/matrix.c` starts with `matrix_`.
- The name of a type is in lower case and ends with `_t`.
- The name of a macro is in upper case.
- A function that only its own file uses is static.

To examine the names:

```bash
python3 scripts/check_naming.py
```

## Contributing

This repository uses [Conventional Commits](https://www.conventionalcommits.org)
and semantic versioning.

Work goes into a branch with the name `feature/<name>` or `fix/<name>`. Such a
branch merges into `development`. When `development` is stable, a branch with
the name `release/vX.Y.Z` comes from it. Only fixes go into a release branch,
and that branch then merges into `main`.

### The feature freeze

**From the release of 0.17.0 this library is in a feature freeze.** It takes
fixes, tests and documentation. It does not take new modules or new public
functions.

The freeze is not a note in a file. `scripts/check_freeze.py` counts the public
functions in every header and compares them against the counts recorded at
0.17.0, and it runs as its own job in the workflow. Adding a function fails the
build, and so does taking one away: removing one changes what callers may rely
on, and a freeze is exactly the time not to do that by accident.

To lift it on purpose for a release, run

```bash
python3 scripts/check_freeze.py --show
```

and paste the answer into `FROZEN` in that file, so that the change is one a
reviewer can see in the diff.

**What the freeze is for.** An audit before it found 96.5 percent of lines
covered and no function between nothing and sixty percent, but 21 modules that
no generated test has ever exercised. Every catch-up round on that has found
real faults. The freeze is the time to close it.

**What the freeze has closed.** Every module that is not a handful of lines now
has a file of rules that hold it to what it IS and not to what its interface
looks like. Those rules found three faults in the library: a filter that gave a
different shape for the same reading measured in volts and in millivolts, a
step for a derivative that was five thousand times worse than the width could
do, and a decomposition whose envelope stood still while it took the same
amount away again and again, so that a signal of size 3 gave a residue of a
million and a half.

**How much is covered, and why the number is 98 and not 99.** The build fails
below 98 percent of lines and below 90 percent of branches. What is left is not untested behaviour: it is the
guards against the heap giving nothing and against numbers the width cannot
hold. Reaching those needs an allocator that fails on purpose, or calls that
break what the headers say a caller may do. Covering every line a caller CAN
reach still leaves the whole below 99, thus 99 is a number the code cannot meet
while those guards stand, and taking them out to meet it would be the wrong
trade. Two of them were found to be unreachable because the caller's own check
is stricter than the guard, and those are named where they stand.

The guards against the heap giving nothing ARE now reached. `Test_heap_refusal`
asks the linker to send `malloc` and `calloc` through a pair of functions that
refuse on command, which is the only way to ask for a heap that fails. Every
allocator of the library is held to what `ffitt/core/README.md` says it must
then do.

**The branch number leaves the assertions out, and it must.** An `ASSERT`
states what the CALLER must have got right, and its failing side calls abort. A
suite that passes has by construction never taken that side. There are 964 of
them, and counted in they hold the whole at 74 percent and hide every real gap
behind a wall of branches no test could ever turn green. Left out, the number
is 91.2 and it means something: what is still uncovered is mostly the operand
combinations inside the validators, where a refusal is tested and which half of
an `&&` caused it is not.

**Rules are run over and over, not once.** A rule that passes one run has been
given a few hundred cases. The suite is run 25 times at each width before a
release, and that has found five rules whose bound was too tight to be true.
None of them was a fault in the library, and two of them corrected what the
headers claimed: how far a resampler really stops a tone at the very edge of
the band, and what a coefficient of correlation needs of a signal before it
means anything.

### Making a release

The bump command of commitizen is not in use, thus a release is made by hand.
When `development` is stable:

1. Make the release branch from `development`:

   ```bash
   git checkout development && git checkout -b release/vX.Y.Z
   ```

2. Write the entry for the new version at the top of `CHANGELOG.md`. Take the
   text of each entry from the subject of each commit since the last tag:

   ```bash
   git log --reverse --format='%s' <last tag>..HEAD
   ```

3. Change the version in the `project` command of `CMakeLists.txt` to the new
   version.

4. Commit the two files, and give the commit the message
   `docs(changelog): Add the entry for the version X.Y.Z`.

5. Only fixes go into the release branch after this point. Run the tests, the
   naming check and the documentation check again after each fix.

6. Merge the branch into `main` and into `development`, and make the tag:

   ```bash
   git checkout main && git merge --no-ff release/vX.Y.Z -m "Merge branch 'release/vX.Y.Z'"
   ```

   ```bash
   git checkout development && git merge --no-ff release/vX.Y.Z -m "Merge branch 'release/vX.Y.Z' into development"
   ```

   ```bash
   git tag -a X.Y.Z -m "X.Y.Z" main
   ```

   ```bash
   git push origin main development X.Y.Z
   ```

**Both of those are merges and neither can be a fast forward.** The release
branch is merged into two branches, thus each of them gets a merge commit of its
own and from that moment neither branch holds the other. The tags up to 0.8.0
name a plain commit and a fast forward worked then; 0.9.0 is the first that
names a merge commit, and no fast forward onto `main` has been possible since.
These steps said `--ff-only` until 0.14.0, which aborts.

**The tag names `main`.** Both branches hold the same tree at this point but they
are different commits, and every tag from 0.1.0 onwards names the commit on
`main`. Leaving the branch off tags whichever branch happens to be checked out,
which is `development` if the steps are followed in the order above.

The name of the tag holds no letter v, but the name of the branch does. The
tag 0.1.0 set that rule.

Each push runs the workflow in
[.github/workflows/tests.yml](.github/workflows/tests.yml). It runs the
documentation check, the naming check, the example check, the unit tests, the
property based tests, the build, and a build with the warnings of the compiler
switched on. Everything but the first three runs at both widths, which makes
eleven jobs. A warning stops the workflow.
