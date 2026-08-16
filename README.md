# SignalProcessingToolkit

A C library for your complex signal processing needs.

Each module gives two ways to get memory: one that uses the heap, and one that
takes memory from the caller. The second way lets the library work on a target
that has no heap.

| Area | Modules |
| --- | --- |
| Frequency | `fft`, `goertzel`, `hilbert`, `hht`, `dwt` |
| Filters | `fir`, `iir`, `savgol`, `kalman`, `ekf` |
| Decomposition | `emd`, `imf`, `cspline` |
| Mathematics | `matrix`, `cmatrix`, `pmatrix`, `cnum`, `vector`, `vector2d` |
| Utilities | `binarysearch`, `peakdetect`, `valleydetect` |

`fft` reaches the frequency domain, and `goertzel` watches one frequency with
three float values only. `hilbert` gives the amplitude and the frequency at
each point of time, and `hht` joins it with `emd` to give the Hilbert-Huang
transform. `dwt` says which frequencies a signal holds and where they lie.

`fir` and `iir` take a band of frequencies out of a signal. `savgol` smooths a
signal and keeps the height of a peak. `kalman` follows a state through a
linear model, and `ekf` follows it through a model that a function describes.

## The three kinds of matrix

| Module | Element | Use it for |
| --- | --- | --- |
| `matrix` | a float value | The common case |
| `cmatrix` | a complex number, from the `cnum` module | The frequency domain |
| `pmatrix` | a pointer to a function of one parameter | A matrix such as `[sin(x) cos(x)]` |

`cmatrix` gives the same names as `matrix` for the same operations, and two
more that belong to complex numbers only: `cmatrix_conjugate_transpose` and
`cmatrix_is_hermitian`.

`pmatrix` holds no arithmetic of its own. Give it a value for the parameter,
and it gives a `matrix_t` that every other module can take:

```c
pmatrix_t rotation = pmatrix_alloc(2, 2);
pmatrix_add_element(&rotation, 0, 0, cosf);
pmatrix_add_element(&rotation, 0, 1, negative_sine);
pmatrix_add_element(&rotation, 1, 0, sinf);
pmatrix_add_element(&rotation, 1, 1, cosf);

matrix_t values = pmatrix_evaluate(&rotation, angle);
```

A function of the standard library such as `sinf` fits the type of an element
directly.

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

## Documentation

[docs/API.md](docs/API.md) describes every module, type, macro and function.

The file comes from the comments in the headers, thus the documentation and the
code cannot say two different things. After a change to a header, make the file
again:

```bash
python3 scripts/api_doc.py
```

To examine that every function has a comment and that the file is current:

```bash
python3 scripts/api_doc.py --check
```

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
