# API reference

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Each module has its own file. Open the file of the module that you work with.

Each area also holds a guide that says how its modules work and which one to
reach for. The guide explains the method; the file of a module gives the exact
name and shape of every function.

## Transforms

[How the transform modules work](../sptk/transform/README.md)

| Module | What it holds |
| --- | --- |
| [`fft`](api/fft.md) | The fast Fourier transform |
| [`bluestein`](api/bluestein.md) | A transform of any size |
| [`window`](api/window.md) | Windows for a transform |
| [`psd`](api/psd.md) | Power at each frequency |
| [`csd`](api/csd.md) | What two signals have in common |
| [`stft`](api/stft.md) | The transform in short pieces |
| [`spectrogram`](api/spectrogram.md) | What the short pieces mean |
| [`correlate`](api/correlate.md) | How alike two signals are |
| [`convolve`](api/convolve.md) | Sliding one signal along another |
| [`goertzel`](api/goertzel.md) | Detection of one frequency |
| [`hilbert`](api/hilbert.md) | The Hilbert transform |
| [`hht`](api/hht.md) | The Hilbert-Huang transform |
| [`dwt`](api/dwt.md) | The discrete wavelet transform |

## Filters

[How the filter modules work](../sptk/filter/README.md)

| Module | What it holds |
| --- | --- |
| [`fir`](api/fir.md) | Filters with a finite impulse response |
| [`iir`](api/iir.md) | Filters with an infinite impulse response |
| [`savgol`](api/savgol.md) | The filter of Savitzky and Golay |
| [`movavg`](api/movavg.md) | The mean of the last samples |
| [`medfilt`](api/medfilt.md) | The median of the last samples |
| [`dcblock`](api/dcblock.md) | Taking the level of a signal away |
| [`detrend`](api/detrend.md) | Taking the level and the drift out of a block |
| [`hampel`](api/hampel.md) | Replacing only the samples that are wrong |
| [`adaptive`](api/adaptive.md) | A filter that finds its own coefficients |
| [`rls`](api/rls.md) | A filter that solves least squares at every sample |
| [`lattice`](api/lattice.md) | A filter built as a ladder of stages |
| [`resample`](api/resample.md) | Changing the rate of a signal |
| [`filtfilt`](api/filtfilt.md) | Filtering with no delay |

## Estimation

[How the estimate modules work](../sptk/estimate/README.md)

| Module | What it holds |
| --- | --- |
| [`kalman`](api/kalman.md) | The Kalman filter |
| [`ekf`](api/ekf.md) | The extended Kalman filter |
| [`ukf`](api/ukf.md) | The unscented Kalman filter |
| [`propagate`](api/propagate.md) | Carrying a state forward through a rate of change |

## Decomposition

[How the decompose modules work](../sptk/decompose/README.md)

| Module | What it holds |
| --- | --- |
| [`emd`](api/emd.md) | Empirical mode decomposition |
| [`imf`](api/imf.md) | Intrinsic mode functions |

## Interpolation

[How the interpolate modules work](../sptk/interpolate/README.md)

| Module | What it holds |
| --- | --- |
| [`cspline`](api/cspline.md) | Cubic splines |
| [`interp`](api/interp.md) | Reading between the points of a table |

## Linear algebra

[How the linalg modules work](../sptk/linalg/README.md)

| Module | What it holds |
| --- | --- |
| [`matrix`](api/matrix.md) | Matrices of float values |
| [`cmatrix`](api/cmatrix.md) | Matrices of complex numbers |
| [`pmatrix`](api/pmatrix.md) | Matrices with a parameter |
| [`cnum`](api/cnum.md) | Complex numbers |
| [`quaternion`](api/quaternion.md) | Which way something points |
| [`eigen`](api/eigen.md) | The directions a matrix stretches |
| [`poly`](api/poly.md) | Polynomials, and where they cross nothing |
| [`lstsq`](api/lstsq.md) | Fitting a curve through readings |
| [`vector`](api/vector.md) | Vectors of float values |
| [`vector2d`](api/vector2d.md) | Vectors with two values |

## Utilities

[How the util modules work](../sptk/util/README.md)

| Module | What it holds |
| --- | --- |
| [`generate`](api/generate.md) | Making the signals to test with |
| [`quantise`](api/quantise.md) | Putting a signal into steps |
| [`stats`](api/stats.md) | Measures of a list of samples |
| [`binarysearch`](api/binarysearch.md) | Binary search |
| [`peakdetect`](api/peakdetect.md) | Peak detection |
| [`valleydetect`](api/valleydetect.md) | Valley detection |

## Core

[How the core modules work](../sptk/core/README.md)

| Module | What it holds |
| --- | --- |
| [`real`](api/real.md) | The one type that holds every number |
| [`ringbuf`](api/ringbuf.md) | A buffer of the last samples |
| [`point2d`](api/point2d.md) | A point on a plane |
| [`callback`](api/callback.md) | The print callback |
