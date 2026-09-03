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
| `matrix` | multiply two 8 by 8 matrices | 5.24 us | 5.43 us |
| `matrix` | transpose an 8 by 8 matrix | 0.62 us | 0.62 us |
| `matrix` | multiply two 8 by 8 matrices into the caller's memory | 5.33 us | 5.27 us |
| `matrix` | invert an 8 by 8 matrix | 15.64 us | 15.75 us |
| `matrix` | the determinant of an 8 by 8 matrix | 4.06 us | 4.10 us |
| `vector` | the dot product of two vectors of 1024 | 2.53 us | 2.53 us |
| `vector` | the length of a vector of 1024 | 2.53 us | 2.54 us |
| `cmatrix` | multiply two 10 by 10 complex matrices | 26.04 us | 63.51 us |
| `cmatrix` | the determinant of a 10 by 10 complex matrix | 12.71 us | 30.22 us |
| `cmatrix` | invert a 10 by 10 complex matrix | 56.95 us | 138.28 us |
| `cnum` | multiply two complex numbers | 0.03 us | 0.04 us |
| `cnum` | divide one complex number by another | 0.04 us | 0.05 us |
| `cnum` | the size of a complex number | 0.03 us | 0.04 us |
| `eigen` | the eigenvalues and vectors of a 10 by 10 matrix | 4.14 us | 3.81 us |
| `eigen` | how badly conditioned that matrix is | 0.05 us | 0.05 us |
| `lstsq` | fit a curve of order 8 through 256 readings | 218.68 us | 219.03 us |
| `lstsq` | how well that curve holds the readings | 8.49 us | 8.74 us |
| `poly` | read a curve of order 8 at one place | 0.05 us | 0.05 us |
| `poly` | multiply two curves of order 8 | 0.27 us | 0.27 us |
| `poly` | every root of a curve of order 4 | 3.44 us | 10.04 us |
| `quaternion` | join two turns into one | 0.04 us | 0.03 us |
| `quaternion` | turn one point by a quaternion | 0.03 us | 0.03 us |
| `quaternion` | the same turn written as a 3 by 3 matrix | 0.09 us | 0.09 us |
| `quaternion` | the turn part way between two others | 0.16 us | 0.17 us |
| `quaternion` | carry a turn forward by a rate over a step | 0.11 us | 0.11 us |
| `pmatrix` | read a 10 by 10 matrix of functions at one place | 1.28 us | 1.26 us |
| `matrix` | the factor of Cholesky of a 10 by 10 matrix | 3.68 us | 3.69 us |
| `vector2d` | the dot product of two vectors of two | 0.03 us | 0.03 us |
| `vector2d` | the length of a vector of two | 0.03 us | 0.03 us |
| `cspline` | build a cubic spline through 512 points | 17.86 us | 21.53 us |
| `cspline` | read one point from a spline of 512 | 0.09 us | 0.11 us |
| `interp` | read one place from a table of 256, straight lines | 0.03 us | 0.03 us |
| `interp` | the same, smooth and never above the neighbours | 0.03 us | 0.03 us |
| `interp` | read 4096 places from that table at one call | 0.03 us | 0.03 us |
| `fft` | a 1024 point transform of a real signal | 70.22 us | 71.01 us |
| `fft` | a 1024 point transform of a complex signal | 62.72 us | 63.40 us |
| `fft` | a 1024 point inverse transform | 91.06 us | 113.38 us |
| `fft` | the size of every bin of a 1024 point spectrum | 4.86 us | 5.21 us |
| `bluestein` | a 1000 point transform, a size no power of two | 380.51 us | 458.62 us |
| `dct` | a 1024 point cosine transform | 11908.80 us | 18037.17 us |
| `dct` | a 1024 point inverse cosine transform | 18372.96 us | 24972.76 us |
| `dwt` | one level of a wavelet over 1024 samples | 11.60 us | 12.08 us |
| `dwt` | rebuild 1024 samples from one wavelet level | 7.46 us | 7.18 us |
| `dwt` | four levels of a wavelet over 1024 samples | 25.24 us | 26.36 us |
| `dwt` | take the small wavelet values out of 1024 | 1.92 us | 1.89 us |
| `window` | build a window of Blackman and Harris over 1024 | 33.31 us | 58.16 us |
| `window` | put a window over 1024 samples | 2.56 us | 2.55 us |
| `window` | the noise bandwidth of a window of 1024 | 2.64 us | 2.61 us |
| `hilbert` | the analytic signal of 1024 samples | 169.85 us | 198.30 us |
| `hilbert` | the envelope of 1024 samples | 6.63 us | 17.45 us |
| `hilbert` | the frequency at each of 1024 samples | 49.41 us | 53.25 us |
| `cepstrum` | the cepstrum of 1024 samples | 193.54 us | 246.77 us |
| `cepstrum` | find the pitch in a cepstrum of 1024 | 0.87 us | 0.87 us |
| `goertzel` | watch one frequency over a block of 1024 | 6.47 us | 6.47 us |
| `goertzel` | read how much of that frequency was there | 0.03 us | 0.03 us |
| `correlate` | 4096 samples against themselves at every lag | 21077.33 us | 21262.99 us |
| `correlate` | the same by the transform | 1701.47 us | 1925.64 us |
| `correlate` | 4096 samples against another signal at 512 lags | 4945.99 us | 5009.29 us |
| `correlate` | find the period of 4096 samples | 13621.04 us | 13910.90 us |
| `convolve` | slide a shape of 512 along 4096 samples | 6450.03 us | 6449.57 us |
| `convolve` | the same by the transform | 2446.38 us | 2729.34 us |
| `psd` | the spectrum of 4096 samples by the way of Welch | 492.68 us | 497.09 us |
| `psd` | the power between two frequencies of that spectrum | 1.16 us | 1.13 us |
| `csd` | the cross spectrum of two signals of 4096 | 1102.27 us | 1217.20 us |
| `csd` | how much two signals of 4096 agree, bin by bin | 1101.98 us | 1215.43 us |
| `csd` | the transfer between two signals of 4096 | 1103.16 us | 1216.33 us |
| `stft` | 4096 samples into frames of 256, hopping 64 | 950.96 us | 960.16 us |
| `stft` | rebuild the 4096 samples from those frames | 1439.71 us | 1838.19 us |
| `spectrogram` | a picture in decibel from those frames | 124.80 us | 232.30 us |
| `spectrogram` | hold that picture against its own loudest point | 35.15 us | 35.16 us |
| `hht` | amplitude and frequency of one mode of 1024 | 226.60 us | 274.03 us |
| `hht` | the mean frequency of that mode | 3.30 us | 3.31 us |
| `fir` | design a low pass of 33 coefficients | 0.82 us | 1.30 us |
| `fir` | 4096 samples through a filter of 33 with no feedback | 412.65 us | 412.35 us |
| `iir` | design a low pass of two sections | 0.10 us | 0.13 us |
| `iir` | 4096 samples through a filter of two sections | 77.97 us | 75.66 us |
| `filtfilt` | 4096 samples both ways, thus with no delay at all | 153.42 us | 154.33 us |
| `filtfilt` | the same with a filter of 33 and no feedback | 875.80 us | 847.59 us |
| `adaptive` | 4096 samples through a filter of 32 that learns | 2148.73 us | 2202.41 us |
| `rls` | 4096 samples through a filter of 32 that learns fast | 68925.02 us | 70241.43 us |
| `lattice` | 4096 samples through eight stages that learn | 1139.99 us | 1257.39 us |
| `savgol` | design a smoother of 33 that keeps a cubic | 11.03 us | 11.10 us |
| `savgol` | smooth 4096 samples and keep the shape of the peaks | 483.47 us | 483.00 us |
| `medfilt` | the middle value of a window of 33 over 4096 samples | 850.37 us | 849.15 us |
| `hampel` | take the wild readings out of 4096 samples | 5063.10 us | 5135.48 us |
| `dcblock` | take the standing level out of 4096 samples | 23.56 us | 23.75 us |
| `detrend` | find the drift under 4096 samples | 32.54 us | 32.01 us |
| `detrend` | find that drift and take it away | 62.96 us | 64.91 us |
| `detrend` | take a drift already known away from 4096 samples | 30.38 us | 30.08 us |
| `farrow` | delay 4096 samples by half a sample | 396.08 us | 397.23 us |
| `resample` | 4096 samples down to a quarter of the rate | 670.56 us | 671.64 us |
| `resample` | 4096 samples up to four times the rate | 2895.42 us | 2906.97 us |
| `movavg` | one sample through a moving mean of 64 | 0.05 us | 0.05 us |
| `fir` | one sample through an equal fir of 64, the mean the slow way | 0.21 us | 0.21 us |
| `kalman` | one prediction of a Kalman filter over four states | 2.23 us | 2.22 us |
| `kalman` | one full step of a Kalman filter over four states | 4.64 us | 4.64 us |
| `ekf` | one step of a bending filter over four states | 5.50 us | 5.53 us |
| `ukf` | one step of a filter that places points, four states | 10.39 us | 10.40 us |
| `pll` | hold a loop on a tone through 4096 samples | 0.02 us | 0.02 us |
| `propagate` | carry four states one step by the way of Runge | 0.10 us | 0.10 us |
| `propagate` | carry four states across a hundred such steps | 9.01 us | 9.09 us |
| `changepoint` | watch 4096 readings for the moment a level moves | 43.49 us | 43.69 us |
| `matched` | look for a shape of 64 all through 4096 samples | 660.15 us | 659.58 us |
| `matched` | find where that shape fits best | 661.25 us | 660.52 us |
| `delay` | how far two signals of 1024 stand apart, by lag | 3192.89 us | 3194.26 us |
| `delay` | the same to a fraction of a sample, by phase | 144.50 us | 147.83 us |
| `emd` | sift 256 samples into three modes | 3.05 us | 3.06 us |
| `imf` | take the memory for a mode of 1024 points | 0.10 us | 0.08 us |
| `stats` | the mean of 4096 readings | 10.01 us | 10.01 us |
| `stats` | how far 4096 readings spread | 20.01 us | 20.02 us |
| `stats` | the root mean square of 4096 readings | 10.01 us | 10.02 us |
| `stats` | the middle of 4096 readings | 57.39 us | 59.55 us |
| `stats` | the spread of 4096 readings, wild ones and all | 177.73 us | 180.68 us |
| `peakdetect` | every peak in 4096 samples | 17.56 us | 23.67 us |
| `peakdetect` | the peaks in 4096 samples that pass the rules | 1632.92 us | 1634.52 us |
| `peakdetect` | how far one peak stands above its neighbours | 0.03 us | 0.03 us |
| `valleydetect` | every valley in 4096 samples | 14.31 us | 14.47 us |
| `generate` | make 4096 samples of a sine | 60.91 us | 81.50 us |
| `generate` | make 4096 samples of a square wave with no aliases | 90.99 us | 90.82 us |
| `generate` | make 4096 samples of noise | 51.47 us | 50.73 us |
| `quantise` | round 4096 samples to 12 bits | 54.54 us | 55.39 us |
| `quantise` | the same with a little noise added first | 139.72 us | 139.51 us |
| `curve` | draw a bell curve over 4096 places | 73.66 us | 87.69 us |
| `curve` | read a bell curve at one place | 0.04 us | 0.04 us |
| `binarysearch` | find a value in a sorted list of 256 | 0.05 us | 0.05 us |

<!-- BENCHMARK TABLE ENDS. -->
