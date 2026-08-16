# hilbert

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The Hilbert transform. Declared in `hilbert/hilbert.h`.

[Back to the index](../API.md)

## Functions

### `hilbert_analytic_signal`

```c
void hilbert_analytic_signal(fft_t* fft, const float* signal, cnum_t* analytic);
```

Give the analytic signal of a real signal.

The signal and the work buffer must hold as many values as the size of the
transform. The function writes the result into the work buffer, thus it gets
no memory.

### `hilbert_amplitude`

```c
void hilbert_amplitude(const cnum_t* analytic, float* amplitude, uint32_t size);
```

Write the instantaneous amplitude of each point into the amplitude list. The
amplitude follows the envelope of the signal, and it is never less than
zero.

### `hilbert_phase`

```c
void hilbert_phase(const cnum_t* analytic, float* phase, uint32_t size);
```

Write the instantaneous phase of each point into the phase list. The phase
lies between -pi and pi.

### `hilbert_frequency`

```c
void hilbert_frequency(const cnum_t* analytic, float* frequency, uint32_t size, float sample_rate);
```

Write the instantaneous frequency of each point into the frequency list.

The frequency comes from the change of the phase between two samples. The
function takes the change into the range from -pi to pi before it makes the
frequency, because the phase itself jumps from pi to -pi.

The list holds one value less than the signal, because a change needs two
points. The caller gives the sample rate in samples for each second, and the
result is in hertz.
