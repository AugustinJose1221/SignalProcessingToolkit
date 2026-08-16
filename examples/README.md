# Examples

Each file holds one small program that shows one part of the library.

Every main function stands inside a condition on `RUN_EXAMPLE`, thus one build
gives one example. Choose the example in
[run_example.h](run_example.h), and then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
./build/signalproc_example
```

| Value of `RUN_EXAMPLE` | File | What it shows |
| --- | --- | --- |
| `RUN_FFT_EXAMPLE` | [fft.c](fft.c) | The frequencies that a signal holds, and why a tone should lie on a bin |
| `RUN_FILTER_EXAMPLE` | [filter.c](filter.c) | A low pass filter of each kind, and what each one costs |
| `RUN_HHT_EXAMPLE` | [hht.c](hht.c) | Which frequency a signal holds at which time |
| `RUN_GOERTZEL_EXAMPLE` | [goertzel.c](goertzel.c) | Watching for eight known tones with three float values each |
| `RUN_DWT_EXAMPLE` | [dwt.c](dwt.c) | Taking noise out of a signal and keeping its edges |
| `RUN_SAVGOL_EXAMPLE` | [savgol.c](savgol.c) | Smoothing a peak without making it lower |
| `RUN_EKF_EXAMPLE` | [ekf.c](ekf.c) | Following an object through a radar, which is not a linear model |
| `RUN_EMD_EXAMPLE` | [emd.c](emd.c) | Taking a signal apart into intrinsic mode functions |
| `RUN_MATRIX_EXAMPLE` | [matrix.c](matrix.c) | The operations of the matrix module |

## What each example shows

**fft.c** gives a signal with two tones and prints the size of each bin. Both
tones lie on a bin, and a comment says why that matters: a tone between two
bins gives its energy to many bins around it.

**filter.c** builds a low pass filter of each kind with the same cutoff and
prints how much of each frequency each one lets through. The filter with the
infinite impulse response gives the same edge with 10 coefficients where the
other one needs 41.

**hht.c** gives a signal whose frequency rises with the time. The empirical
mode decomposition takes it apart, and the Hilbert transform then gives the
frequency at each point of time. A Fourier transform would give a wide band and
would say nothing about the time.

**goertzel.c** takes a block that holds the two tones of one key of a telephone
keypad and names the key. Eight detectors hold three float values each, where a
transform would need memory for the whole block.

**dwt.c** cleans a signal that holds two steps. The wavelet transform takes
almost 90 percent of the noise away and leaves both edges sharp, which a low
pass filter could not do.

**savgol.c** smooths a peak with noise and compares the result with a plain
mean of the same window. The plain mean makes the peak lower, and this filter
keeps it. The example also prints the first derivative, which goes through zero
at the peak.

**ekf.c** follows an object with a radar that reads a distance and an angle.
Neither reading is a linear function of the state. The example says why the
radar must read two values: with the distance alone, every point of a circle
around the radar fits the reading.
