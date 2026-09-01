## 0.18.1 (2026-09-01)

### Fix

- **ci**: Say which number the coverage missed, and hold the functions too
- **build**: Read the width that the build asks for

## 0.18.0 (2026-09-01)

### Breaking

- Rename the library to ffitt

### Fix

- **changelog**: List the changes that break what a caller relied on

## 0.17.2 (2026-08-31)

Nothing here changes what a caller sees. The work of this release went into the tests, the documentation or the build.

## 0.17.1 (2026-08-31)

### Fix

- **test**: Allow each road to the determinant what its own products cost
- **filter**: Say what the resampler really stops at the edge of the band
- **test**: Set the bound of the ekf rule from the corners, not a spread
- **transform**: Say what the coefficient needs of a signal
- **decompose**: Find the envelope of the signal in hand at every step
- **estimate**: Choose the derivative step of ekf by the width
- **filter**: Measure the settling of filtfilt against the signal alone

## 0.17.0 (2026-08-28)

### Feat

- **cepstrum**: Find what repeats in a spectrum
- **filter,estimate,detect**: Give the five streaming modules a block form

## 0.16.0 (2026-08-28)

### Feat

- **dct**: Turn a signal into cosines, and back
- **pll**: Follow a tone whose frequency will not stay still
- **farrow**: Delay a signal by a part of a sample

## 0.15.0 (2026-08-28)

### Feat

- **peakdetect**: Refine where a peak stands and how tall it is
- **curve**: Add the shapes a peak can have
- **generate**: Add six kinds, a part for the pulses, and mend a corner

## 0.14.0 (2026-08-28)

### Feat

- **detect**: Add matched, delay and changepoint in a new area
- **fir**: Add a band stop of a chosen length

### Fix

- **poly**: Hold the root finder to the polynomial it was given
- **cnum**: Give the size of a number whose square does not fit
- **lattice**: Give a real mean when nothing is forgotten

## 0.13.1 (2026-08-27)

### Feat

- **examples**: Draw the waveform in the survey example
- **examples**: Add the survey to take before writing a canceller

### Fix

- **examples**: Check that every example is wired in, and wire in the one that was not

## 0.13.0 (2026-08-26)

### Feat

- **poly**: Add polynomials and where they cross nothing
- **quantise**: Add putting a signal into steps, and choosing the error's shape
- **generate**: Add the making of signals to test with

## 0.12.0 (2026-08-26)

### Feat

- **propagate**: Carry a state forward through a rate of change
- **lattice**: Add a filter built as a ladder of stages
- **rls**: Add a filter that solves least squares at every sample
- **eigen**: Add the directions a symmetric matrix stretches

## 0.11.1 (2026-08-25)

### Fix

- **examples**: Declare that every main takes no argument

## 0.11.0 (2026-08-25)

### Feat

- **fir**: Add the phase and the group delay
- **iir**: Add the shapes of filter, the order estimate and the phase
- **stft**: Say how many frames a rebuild needs

### Fix

- **vector2d**: Declare that the allocator takes no argument
- **window**: Say which sizes a tapered window can be built at
- **lstsq**: Refuse a fit that the scaling would do better

## 0.10.1 (2026-08-25)

Nothing here changes what a caller sees. The work of this release went into the tests, the documentation or the build.

## 0.10.0 (2026-08-24)

### Feat

- **stft**: Add the transform in short pieces, and what two signals share
- **fft**: Add a transform of any size and the real inverse transform

### Fix

- **examples**: Add the missing number of the fitcurve example

## 0.9.0 (2026-08-24)

### Feat

- **detrend**: Take the level and the drift out of a block
- **lstsq**: Fit a curve through more readings than it has room for
- **peakdetect**: Add the rules that say which peaks are real
- **interp**: Add reading between the points of a table
- **convolve**: Add sliding one signal along another

## 0.8.0 (2026-08-24)

### Feat

- **quaternion**: Add which way something is pointing
- **ukf**: Add the unscented Kalman filter
- **matrix**: Add the factor of Cholesky

### Fix

- **ukf**: Give the turned gain a matrix of its own shape

## 0.7.1 (2026-08-23)

Nothing here changes what a caller sees. The work of this release went into the tests, the documentation or the build.

## 0.7.0 (2026-08-23)

### Feat

- **filtfilt**: Add filtering with no delay by running the filter both ways
- **resample**: Add changing the rate of a signal
- **adaptive**: Add a filter that finds its own coefficients
- **hampel**: Add replacing only the samples that are wrong
- **psd**: Add the power at each frequency by the method of Welch
- **correlate**: Add how alike two signals are at each lag

## 0.6.2 (2026-08-23)

### Fix

- **filter**: Make the lowest cutoff follow the width of the build

## 0.6.1 (2026-08-23)

### Fix

- **property**: Ask the bindings for a buffer instead of building one

## 0.6.0 (2026-08-23)

### Feat

- **real**: Hold every number in one type whose width the build chooses

## 0.5.0 (2026-08-19)

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

### Fix

- **perf**: Remove the variables that nothing reads, and widen the warnings job

## 0.3.0 (2026-08-16)

### Feat

- **goertzel**: Add Goertzel, the wavelet transform and the filter of Savitzky and Golay
- **ekf**: Add the extended Kalman filter
- **fir**: Add filters with a finite and an infinite impulse response
- **hilbert**: Add the Hilbert transform and complete the Hilbert-Huang transform
- **fft**: Add the fast Fourier transform

### Fix

- **examples**: Restore the names of the new examples

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

## 0.1.0 (2025-03-03)

### Feat

- Add valley detection functionality and unit tests
- Add peak detection functionality and unit tests
- Add callback header file with print function type definition
- Add tests/vector2d directory to project configuration and updated mock prefix to 'Mock_'
- Add common definitions and assertion macros for matrix operations
- **incomplete**: Add kalman filtering implementation
- Add faster determinant calculation implementation
