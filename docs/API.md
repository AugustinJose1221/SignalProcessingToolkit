# API reference

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Each module has its own file. Open the file of the module that you work with.

## Frequency

| Module | What it holds |
| --- | --- |
| [`fft`](api/fft.md) | The fast Fourier transform |
| [`goertzel`](api/goertzel.md) | Detection of one frequency |
| [`hilbert`](api/hilbert.md) | The Hilbert transform |
| [`hht`](api/hht.md) | The Hilbert-Huang transform |
| [`dwt`](api/dwt.md) | The discrete wavelet transform |

## Filters

| Module | What it holds |
| --- | --- |
| [`fir`](api/fir.md) | Filters with a finite impulse response |
| [`iir`](api/iir.md) | Filters with an infinite impulse response |
| [`savgol`](api/savgol.md) | The filter of Savitzky and Golay |
| [`kalman`](api/kalman.md) | The Kalman filter |
| [`ekf`](api/ekf.md) | The extended Kalman filter |

## Decomposition

| Module | What it holds |
| --- | --- |
| [`emd`](api/emd.md) | Empirical mode decomposition |
| [`imf`](api/imf.md) | Intrinsic mode functions |
| [`cspline`](api/cspline.md) | Cubic splines |

## Mathematics

| Module | What it holds |
| --- | --- |
| [`matrix`](api/matrix.md) | Matrices of float values |
| [`cmatrix`](api/cmatrix.md) | Matrices of complex numbers |
| [`pmatrix`](api/pmatrix.md) | Matrices with a parameter |
| [`cnum`](api/cnum.md) | Complex numbers |
| [`vector`](api/vector.md) | Vectors of float values |
| [`vector2d`](api/vector2d.md) | Vectors with two values |

## Utilities

| Module | What it holds |
| --- | --- |
| [`binarysearch`](api/binarysearch.md) | Binary search |
| [`peakdetect`](api/peakdetect.md) | Peak detection |
| [`valleydetect`](api/valleydetect.md) | Valley detection |
| [`point2d`](api/point2d.md) | A point on a plane |
| [`callback`](api/callback.md) | The print callback |
