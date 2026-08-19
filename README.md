# SignalProcessingToolkit

A C library for signal processing.

The library takes a signal apart, cleans it, and follows what it describes. It
holds the transforms that show which frequencies a signal carries, the filters
that take a band of frequencies away, the decompositions that split a signal
into simpler parts, and the estimators that follow a state behind a noisy
measurement. Underneath them sit the vectors, the matrices and the
interpolation that they need.

## What the library gives

The library lies under [sptk](sptk), and each area of work has its own
directory there:

| Area | Directory | Modules | What you do with them |
| --- | --- | --- | --- |
| Transforms | `sptk/transform` | `fft`, `window`, `goertzel`, `hilbert`, `hht`, `dwt` | Find which frequencies a signal holds, and where |
| Filters | `sptk/filter` | `fir`, `iir`, `savgol`, `movavg` | Take a band of frequencies away, or smooth a signal |
| Estimation | `sptk/estimate` | `kalman`, `ekf` | Follow a state behind a noisy measurement |
| Decomposition | `sptk/decompose` | `emd`, `imf` | Split a signal into simpler parts |
| Interpolation | `sptk/interpolate` | `cspline` | Give a smooth curve through a set of points |
| Linear algebra | `sptk/linalg` | `matrix`, `cmatrix`, `pmatrix`, `cnum`, `vector`, `vector2d` | The arithmetic that the areas above need |
| Utilities | `sptk/util` | `stats`, `binarysearch`, `peakdetect`, `valleydetect` | Find a place, a peak or a valley in a list |
| Core | `sptk/core` | `ringbuf`, `callback`, `defs`, `point2d` | A buffer of the last samples, and the types that every module shares |

Include a module by its area:

```c
#include <sptk/transform/fft.h>
#include <sptk/filter/fir.h>
```

Each area holds a guide that says **how** its modules work and which one to
reach for: [transform](sptk/transform), [filter](sptk/filter),
[estimate](sptk/estimate), [decompose](sptk/decompose),
[interpolate](sptk/interpolate), [linalg](sptk/linalg), [util](sptk/util),
[core](sptk/core).

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

**It holds every value in a float.** A float keeps about 7 digits. Each module
says in its header where that limit matters, such as the size of a transform
or the order of a determinant.

## An example

Take a noisy signal, filter it, and look at what frequencies are left:

```c
#include <sptk/filter/fir.h>
#include <sptk/transform/fft.h>

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

This gives the static library `libsignalproc.a`.

To build an example program, first choose one in `examples/run_example.h`, and
then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
./build/signalproc_example
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
  every function of `sptk/linalg/matrix.c` starts with `matrix_`.
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
   git checkout main && git merge --ff-only release/vX.Y.Z
   git checkout development && git merge --ff-only release/vX.Y.Z
   git tag -a X.Y.Z -m "X.Y.Z"
   git push origin main development X.Y.Z
   ```

The name of the tag holds no letter v, but the name of the branch does. The
tag 0.1.0 set that rule.

Each push runs the workflow in
[.github/workflows/tests.yml](.github/workflows/tests.yml). It runs the
documentation check, the naming check, the unit tests, the property based
tests, the build, and a build with the warnings of the compiler switched on. A
warning stops the workflow.
