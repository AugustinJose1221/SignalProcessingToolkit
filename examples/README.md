# Examples

Each file holds one small program that shows one part of the library.

Every main function stands inside a condition on `RUN_EXAMPLE`, thus one build
gives one example. Choose the example in
[run_example.h](run_example.h), and then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
./build/signalproc_example
```

Each example stands on a real device, and each one holds one function that
gives the readings. **Replace that one function with a read from your own
device, and the rest of the example works as it is.** The function carries a
comment that says so.

| Value of `RUN_EXAMPLE` | File | The device | The question |
| --- | --- | --- | --- |
| `RUN_FFT_EXAMPLE` | [fft.c](fft.c) | A pulse sensor on a finger | What is the heart rate? |
| `RUN_FILTER_EXAMPLE` | [filter.c](filter.c) | An electrode of a heart monitor | How do I take the breathing and the mains hum out of an ECG? |
| `RUN_EKF_EXAMPLE` | [ekf.c](ekf.c) | A gyroscope and an accelerometer | Which way is the device tilted? |
| `RUN_GOERTZEL_EXAMPLE` | [goertzel.c](goertzel.c) | A telephone line | Which key did the caller press? |
| `RUN_DWT_EXAMPLE` | [dwt.c](dwt.c) | A load cell on a shaking table | What does the item weigh, and when did it arrive? |
| `RUN_SAVGOL_EXAMPLE` | [savgol.c](savgol.c) | A spectrometer | How high is the peak, and where does it stand? |
| `RUN_HHT_EXAMPLE` | [hht.c](hht.c) | An accelerometer on a motor | How fast does the motor turn at each moment? |
| `RUN_EMD_EXAMPLE` | [emd.c](emd.c) | — | Taking a signal apart into intrinsic mode functions |
| `RUN_MATRIX_EXAMPLE` | [matrix.c](matrix.c) | — | The operations of the matrix module |

## What each example shows

**fft.c — a heart rate from a pulse sensor.** The reading holds the pulse, a
slow drift because the sensor moves against the skin, and noise from the light
of the room. The example looks only at the bins that lie between 0.7 and 3.0
hertz, which is 42 to 180 beats in a minute, thus the drift and the noise fall
outside and no filter is needed first. It also says how finely the answer can
be read: one bin holds 5.9 beats here, and a longer block gives a finer step.

**filter.c — an ECG from an electrode.** Two filters in a row take the
breathing away at 0.3 hertz and the mains hum away at 50 hertz. The example
says why each one is of a different kind: reaching a cutoff of 0.5 hertz with a
finite filter would need about 2000 coefficients, and an infinite one needs
ten. It then shows the price of that choice. Sending the clean heart alone
through each filter shows that the low pass changes it by almost nothing while
the high pass changes it much more, because a filter with feedback does not
delay every frequency by the same time.

**ekf.c — a tilt angle from a gyroscope and an accelerometer.** The gyroscope
is smooth but drifts, and the accelerometer never drifts but every knock
disturbs it. The filter holds the angle and the bias of the gyroscope in its
state, and it finds a bias of 0.0199 against a true 0.0200 from the two sensors
alone. Without the bias in the state the angle would drift by 69 degrees in a
minute.

**goertzel.c — a key press from a telephone line.** Eight detectors, one for
each tone of the keypad, name the key. The example says what that saves: a
transform of the block would need 1640 bytes and would give 205 answers of
which the device wants eight, where the detectors together hold under 300
bytes.

**dwt.c — a weight from a load cell on a shaking table.** A low pass filter
would take the shaking away but would make each step round, thus the moment
when an item arrived would blur. The wavelet transform takes the shaking away
and leaves both steps sharp on the very reading where the item arrives.

**savgol.c — a peak from a spectrometer.** The height of a peak says how much
of a substance is there. A plain mean of the window pulls that height down by
17 percent, thus it would report too little; this filter loses 1.3 percent. The
example then uses the derivative to name the place of the peak more finely than
the step between two readings.

**hht.c — the speed of a motor from its vibration.** While the motor starts up,
the frequency of the shaking rises. A Fourier transform would give a wide band
and would say nothing about the moment. This transform gives the speed at each
moment, and the example says what the single mean number hides.
