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
