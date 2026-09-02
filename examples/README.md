# Examples

Each file holds one small program that shows one part of the library.

Every main function stands inside a condition on `RUN_EXAMPLE`, thus one build
gives one example. Choose the example in
[run_example.h](run_example.h), and then give:

```bash
cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
./build/ffitt_example
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
| `RUN_RESAMPLE_EXAMPLE` | [resample.c](resample.c) | A vibration sensor on a bearing | How do I log at a lower rate without inventing a tone? |
| `RUN_PSD_EXAMPLE` | [psd.c](psd.c) | An accelerometer on a pump | Which band holds the energy, and is the bearing failing? |
| `RUN_CORRELATE_EXAMPLE` | [correlate.c](correlate.c) | Two microphones | Which way did that sound come from? |
| `RUN_ADAPTIVE_EXAMPLE` | [adaptive.c](adaptive.c) | Two microphones, one at a fan | How do I hear a voice over a fan? |
| `RUN_CLEAN_EXAMPLE` | [clean.c](clean.c) | A load cell on a filling line | The level drifts and some samples are nonsense. What order do I clean in? |
| `RUN_FILTFILT_EXAMPLE` | [filtfilt.c](filtfilt.c) | A pressure sensor on a hydraulic line | How wide was the pulse, and when exactly? |
| `RUN_KALMAN_EXAMPLE` | [kalman.c](kalman.c) | A distance sensor on a rail | Where is the trolley, and how fast is it going? |
| `RUN_STREAM_EXAMPLE` | [stream.c](stream.c) | A converter giving blocks | How do I keep a window that crosses the blocks? |
| `RUN_CALIBRATE_EXAMPLE` | [calibrate.c](calibrate.c) | A thermistor | What temperature is this resistance? |
| `RUN_LINALG_EXAMPLE` | [linalg.c](linalg.c) | A robot joint, two coils | — |
| `RUN_ATTITUDE_EXAMPLE` | [attitude.c](attitude.c) | A gyroscope and an accelerometer | Which way is the board pointing? |
| `RUN_FITCURVE_EXAMPLE` | [fitcurve.c](fitcurve.c) | A pressure sensor read as a count | What curve turns a count into a pressure? |
| `RUN_SPECTROGRAM_EXAMPLE` | [spectrogram.c](spectrogram.c) | A tone that slides from low to high | When was each frequency there? |
| `RUN_COHERENCE_EXAMPLE` | [coherence.c](coherence.c) | A machine, a floor and a second machine | Which of these two is shaking the floor? |
| `RUN_SHAPES_EXAMPLE` | [shapes.c](shapes.c) | One specification, built four ways | Which shape of filter should I use? |
| `RUN_CONTINUOUS_EXAMPLE` | [continuous.c](continuous.c) | A pendulum measured noisily | How do I use a model written as a rate of change? |
| `RUN_SURVEY_EXAMPLE` | [survey.c](survey.c) | Two sensors, before any filter is written | Will a canceller work, and how well? |

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

**resample.c — logging a bearing sensor at a lower rate.** The sensor reads at
32 kHz and the log holds 500 samples a second. Keeping every 64th sample does
not lose the 4100 Hz rattle: it MOVES it, to 100 Hz, where it looks exactly
like a reading of the machine. The example runs the same signal both ways and
prints the power at 100 Hz: 1.12 the careless way and 0.00 the careful way.

**psd.c — watching a pump for a failing bearing.** The rattle grows from 0.0008
to 0.0451 while the shaft is unchanged, which is enough to set a threshold on.
The example then measures the same signal three ways, with different blocks,
windows and overlaps, and gets the same answer each time. That is what the
scaling is for: leave out any one of its three corrections and the three lines
would disagree by a factor that nobody would notice.

**correlate.c — which way a sound came from.** Two microphones 0.34 m apart. The
lag where the two recordings are most alike is the delay, and the delay is the
angle. The last of its four cases is the one that matters: a room full of echo
gives a delay that looks reasonable and a strength of 0.27, and only the
strength says the answer means nothing.

**adaptive.c — hearing a voice over a fan.** A second microphone at the fan
hears the noise alone. No filter of frequency could help, because the fan
covers the whole band the voice lives in. The example also runs the case that
fails quietly: when the reference microphone can hear the voice too, the filter
removes the voice as well, and the part of it that survives falls from 1.00 to
0.17 with nothing in the numbers to say so.

**clean.c — a load cell that drifts and is knocked.** Three faults need three
answers, and the ORDER is not free to choose. The example runs the same chain
both ways: bad samples out first finds 12 of them, and smoothing first finds 0,
because the mean has already spread each knock over its whole window and there
is no single bad sample left to find.

**filtfilt.c — measuring a pressure pulse.** One pass of a filter moved the peak
by 19 samples; running it both ways left it where it was. The example then
prints what the filter really does at four frequencies, one pass against two,
because running it twice squares its gain and a design that does not allow for
that takes out more than it meant to.

**kalman.c — a trolley on a rail.** The sensor measures distance and nothing
measures speed, yet the filter gives a speed, because it is told that a
position changes by the speed times the time. Differencing two readings gives a
speed wrong by 0.335 m/s; the filter gives one wrong by 0.047. The example runs
three settings of the process noise to show that it is a trade and not a value
to look up.

**stream.c — a window that crosses the blocks.** A converter gives 320 samples
every 10 ms. The detector fires in block 19 and the event it found stands in
block 18, thus a program holding only the block in hand could not have found it
at all. The ring buffer moves nothing when a sample arrives, where copying the
window to the front of an array would move 500 values for every 320 that come.

**calibrate.c — a thermistor.** A table of 12 points, and every reading falls
between them. Straight lines between the points are wrong by 0.54 C on the
mean; a cubic spline is wrong by 0.05 C. Both pass through every point of the
table exactly, and they differ only between the points, which is where every
reading falls.

**linalg.c — three jobs that need more than a matrix of plain numbers.** A
robot joint whose rotation matrix has FUNCTIONS for its elements, two coupled
coils whose impedance is complex and where the phase is the whole of the
answer, and two readings compared by the angle between them. It also shows
where a module writes to, by handing it a function that writes nowhere.

**attitude.c — which way a board is pointing.** A gyroscope is smooth and
drifts; an accelerometer never drifts and is noisy. Neither answers alone. The
board pitches to straight up, which is exactly where three angles lose a
number, thus the attitude is held as four. The measurement is where gravity
lands after the attitude has turned it, which is not a straight operation, thus
the filter that needs no derivative is the one to reach for.

The example reports the tilt apart from the total, and the split is the point.
The gyroscope alone drifts to 25 degrees of tilt in thirty seconds; the filter
holds it at under one. The total grows for both, and that is not a fault of the
filter: an accelerometer sees gravity and nothing else, thus nothing here can
say which way the board faces about the vertical. A real device adds a
magnetometer for that.

**fitcurve.c — fitting a calibration curve, and the trap that ruins it.** A
pressure sensor is measured against a reference at twelve points and a curve of
the third order is fitted through them. The trap is not the order. The sensor
is read as a count from a converter of 16 bits, thus the readings run from 6100
to 60000, and **a plain fit through those counts is refused at either width**,
on data that lies on a perfect cubic. The same readings brought to a range of
about -1 to 1 first fit exactly. The example runs both, and then shows what the
scaled coefficients give when they are read as a plain polynomial in the count:
a pressure of minus twenty-five million million, where 131 kilopascal is right.

**spectrogram.c — when each frequency was there.** A tone slides from 100 Hz to
800 Hz across two seconds, and the example draws it as a rising line on the
terminal. One transform of the whole recording cannot show that at all: it gives
a smear covering every frequency the tone ever visited, with no way to tell a
rising tone from a falling one. The same recording is drawn at a block of 64 and
a block of 256, so that the trade can be seen rather than believed — the long
block draws a thin line in frequency and a smeared one in time, and the short
block does the reverse. The product of the two is fixed and no setting is good
at both.

**coherence.c — which of two machines is shaking the floor.** A machine runs at
50 Hz and a second one nearby at 53 Hz. At the block used, those two fall in the
**same bin**, thus no spectrum can separate them: both recordings show one peak
in the same place. Coherence separates them completely, reading 0.80 for the
machine that really is shaking the floor and 0.00 for the one that is not. The
example also runs the block count up from 1, so that the trap can be watched:
below eight blocks the module refuses, because at one block any two signals
whatever read exactly 1.00.

**shapes.c — which shape of filter to use.** One specification — pass below 500
Hz, stop above 750 Hz by 60 dB, 1 dB of ripple allowed — built four ways. Every
one meets it; they differ only in what they cost to run, and an elliptic filter
does it with 15 multiplications for each sample where a Butterworth needs 50.

Then the part a gain measurement never shows. The example prints the group delay
across the band that passes, and every shape climbs steeply near its cutoff — but
by different amounts. A Butterworth rises by a little over twice across the band;
the elliptic by more than six times. **The shape that costs the fewest
multiplications costs the most in the shape of the waveform**, and that is the
trade the table of gains hides.

**continuous.c — a model written as a rate of change.** Every estimator in this
library asks for a function that takes the state now and gives the state at the
next sample, and **nobody writes a model that way**. A pendulum is written as how
fast it is turning and how fast that is changing, and turning one into the other
is what `propagate` does.

The example runs the same `ukf` three times, differing only in how the model is
carried between measurements, and reports how far the **rate** ends up — the part
that is never measured at all, and so the part where a badly carried model shows
first. Euler is twice as far out as the other two.

Then the lesson that matters more: **midpoint and Runge give the same answer
here**, though Runge asks for the rate twice as often. At this step the midpoint
error has already fallen below the noise on the measurements, and nothing below
that noise can help. Carry the model well enough that it is not the worst thing
in the answer, then stop.

**survey.c — what to measure before writing a canceller.** The `adaptive` example
shows a canceller running; this one shows the work that comes first, in four
steps, each of which can tell you the loop is not worth writing.

**Step one** takes the coherence between the two sensors, which is the ceiling on
cancellation — the part of what the primary sensor hears that the reference can
account for at all. Away from the signal it reads 0.99, allowing about 20 dB. At
the signal's own frequency it dips to 0.25, **and that dip is what a good
reference looks like**: it has never heard the thing being measured.

**Step two** runs a deliberately over-long `rls` probe and reads the path off its
coefficients. It recovers the delay of 7 samples and the shape of the path
(0.5995, −0.3002, 0.2002, 0.1005 against a true 0.6, −0.3, 0.2, 0.1) — none of
which the program was told — and says 14 taps will do.

**Step three** measures the learning rate against **two** things, because the
usual advice to pick it for convergence speed is half the story:

| rate | noise removed | signal kept |
|---|---|---|
| 0.50 | −13.8 dB | 0.534 |
| 0.10 | −18.5 dB | 0.902 |
| 0.05 | −20.4 dB | 0.957 |
| 0.01 | −15.4 dB | 0.992 |

At 0.5 the filter takes half the signal away with the noise **and cancels worse
for it**. At 0.01 it keeps the signal whole and has not finished learning.
Neither column alone would have found the middle.

**Step four** moves the reference sensor nearer the thing being measured:

| leak | coherence at 312 Hz | noise removed | signal kept |
|---|---|---|---|
| 0.0 | 0.251 | −20.4 dB | 0.957 |
| 1.0 | 0.260 | −13.4 dB | 0.988 |
| 4.0 | 0.602 | −5.5 dB | 0.448 |
| 8.0 | 0.669 | −3.7 dB | 0.087 |

The coherence and the cancellation go wrong at once. **The signal column lies for
a while** — at a leak of 1 it reads *better* than with no leak, because the filter
is too busy with the reference to eat it — and then the signal goes quickly. That
is why the coherence is the measurement to trust: it went wrong first, it went
wrong steadily, and it needed no canceller to say so.

**It also draws the waveform**, down the page rather than across it, because a
terminal holds far more rows than columns — and because the two traces can then
stand side by side at the **same scale**, which is what makes the comparison
honest. Drawn at scales of their own they would look alike. The raw trace jumps
about while the filtered one hardly leaves the centre; drawn again four times
larger against the true signal, the two snake down the page together. The extra
width of the filtered trace, 0.46 against the signal's 0.25, is the residual
noise made visible. The 4 parts in a hundred of signal the filter ate cannot be
seen at all — some things are only ever numbers.
