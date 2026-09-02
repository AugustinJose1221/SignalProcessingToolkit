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

The repository holds four kinds of test: unit tests for known examples,
property based tests for rules that must hold for every input, conformation
tests that compare against the GNU Scientific Library, and cost tests that hold
the headers to what they claim an operation costs.

```bash
ceedling test:all
```

[docs/TESTING.md](docs/TESTING.md) says what each kind is for, how to run it at
either width, and what the build holds them to.

## What it costs

[docs/COSTS.md](docs/COSTS.md) holds a table of what every operation of every
module costs, at both widths, written from the benchmark itself.

```bash
cmake -S . -B build -DBUILD_BENCHMARK=ON && cmake --build build && ./build/benchmark
```

## Documentation

[docs/API.md](docs/API.md) is the index. Each module has its own file under
[docs/api](docs/api), which describes every type, macro and function of that
module.

The files come from the comments in the headers, thus the documentation and the
code cannot say two different things. [CONTRIBUTING.md](CONTRIBUTING.md) says
how they are made again after a header changes.

## Contributing

[CONTRIBUTING.md](CONTRIBUTING.md) holds the naming scheme, the branch flow, the
feature freeze that has been in force since 0.17.0, and how a release is made.
