# What each operation costs

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

## The table

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
