# Property based tests

The unit tests in Unity examine known examples. These tests examine rules that
must hold for every input. Hypothesis makes the input, looks for an input that
breaks a rule, and then makes that input as small as it can. A small input
shows the cause of a fault more clearly than a large one.

## Why Python

There is one property based test library for C, which is `theft`. Its last
change to the code is from December 2020. Hypothesis gets changes each month,
and it makes float values with much more care. This library holds every value
in a float, thus the quality of the float values decides the value of the
tests.

**Python is a test tool only. The C library needs no external library, and
these tests change nothing in the library.** The tests build the library
sources into a shared object with `gcc` and read the functions from it with
`ctypes`. The build of the library with CMake does not know about Python.

## How to run the tests

Make an environment and get the two tools:

```bash
python3 -m venv .venv && .venv/bin/pip install -r tests/property/requirements.txt
```

Run the tests:

```bash
.venv/bin/pytest tests/property/
```

The tests build `build/property/libffitt.so` one time for each run. You need
`gcc`. You do not need CMake, and you do not need Ceedling.

## The files

| File | What it holds |
| --- | --- |
| `ffitt.py` | The build of the shared object and the C types for `ctypes` |
| `strategies.py` | The strategies that make matrices, vectors and points |
| `conftest.py` | The fixture that gives the loaded library |
| `test_bindings.py` | Examines that the Python types agree with the C types |
| `test_matrix_properties.py` | Rules for the matrix module |
| `test_vector_properties.py` | Rules for the vector module |
| `test_cspline_properties.py` | Rules for the cubic spline module |
| `test_kalman_properties.py` | Rules for the Kalman filter |
| `test_utils_properties.py` | Rules for the search, the peaks and the valleys |

## Two points to remember

**Keep the Python types and the C types together.** The tests read the
structures of the library directly. `test_bindings.py` builds a small C program
that gives the size of each structure and the position of each element, and it
compares those numbers with the Python types. If you change a structure of the
library, this test fails and shows you where.

**A float keeps about 7 digits.** The strategies hold the values in a range
where the arithmetic still gives a result that a user can trust. The comparison
helpers take a relative tolerance as well as an absolute one. For the
determinant the tolerance follows the bound of Hadamard, because the
calculation adds and subtracts products that are much larger than the result.

## What is covered

One file for each module. A property test earns its place where it proves
something a set of examples structurally cannot: a rule that must hold for
every input, or a second way to the same answer.

| file | what it holds |
|---|---|
| `test_bindings.py` | the Python types agree with the C types, at both widths |
| `test_interp_properties.py` | pchip never leaves the table and never turns back on rising data |
| `test_convolve_properties.py` | the direct way and the way through the transform give one answer |
| `test_fft_properties.py` | the transform against one written plainly, and the rule of Parseval |
| `test_bluestein_properties.py` | any size against a plain transform, and against `fft` where both can be used |
| `test_stft_properties.py` | the rebuild is exact inside the solid stretch and nothing outside it |
| `test_lstsq_properties.py` | the error left after a fit holds nothing of the model |
| `test_detrend_properties.py` | taking a trend out twice is taking it out once |
| `test_quaternion_properties.py` | a matrix written and read back is the same attitude, through all four branches |
| `test_window_properties.py` | every window is symmetric, and the three gains agree |
| `test_peakdetect_rules_properties.py` | a stricter rule never finds more peaks |
| `test_matrix_properties.py`, `test_vector_properties.py`, `test_stats_properties.py`, `test_medfilt_properties.py`, `test_cspline_properties.py`, `test_kalman_properties.py` | the rules of the modules they name |

**A test that fails is not always the library.** Several of the rules above were
wrong when they were first written, and the library was right: a discrete ramp
is not orthogonal to a sinusoid, an even-sized window has no sample at its peak,
and bin `size/2` is its own mirror. Measure before changing the library.

## Keeping the tests quick

Each example calls into C, thus each costs more than a pure Python one. The
profile in `conftest.py` holds the count to 200, and the files that run a
transform lower it further with `@settings(max_examples=40)`. The sizes are kept
small on purpose: a rule that holds at a block of 8 holds at a block of 8192, and
the larger block only costs time.
