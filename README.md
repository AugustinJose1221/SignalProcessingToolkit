# SignalProcessingToolkit

A C library for your complex signal processing needs.

The library holds a matrix module, a vector module, a cubic spline, an
empirical mode decomposition and a Kalman filter. Each module gives two ways to
get memory: one that uses the heap, and one that takes memory from the caller.
The second way lets the library work on a target that has no heap.

**The library needs no external library.** It uses the C standard library only.
Some test tools need other software, but the library itself does not.

## Build

```bash
cmake -S . -B build && cmake --build build
```

This gives the static library `libsignalproc.a`.

To build the example program, first choose an example in
`examples/run_example.h`, and then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
```

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

## Naming

The names follow the scheme of the Linux kernel:

- A name is in lower case, with an underscore between the words.
- The name of a function starts with the name of its module, thus `matrix_add`
  and not `add_matrix`.
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

Each push runs the workflow in
[.github/workflows/tests.yml](.github/workflows/tests.yml). It runs the naming
check, the unit tests, the property based tests, the build, and a build with
the warnings of the compiler switched on. A warning stops the workflow.
