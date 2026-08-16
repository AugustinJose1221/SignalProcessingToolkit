# Transforms

A signal arrives as a list of samples through time. These modules say which
frequencies stand inside it. Each one answers a different question, and the
question decides which one you want.

| Module | The question it answers | What it costs |
| --- | --- | --- |
| `fft` | Which frequencies does this block hold? | Memory for the whole block |
| `goertzel` | How much of *this one* frequency does it hold? | Three float values |
| `hilbert` | What is the amplitude and the frequency at each moment? | One transform each way |
| `hht` | Which frequency at which moment, for a signal that changes? | A decomposition and a transform |
| `dwt` | Which frequencies, and *where* in the signal? | One pass for each level |

## fft

The fast Fourier transform changes a block of samples into a list of bins.
Each bin says how much the signal turns at one frequency.

The plain way to do this needs `n*n` multiplications. The method of Cooley and
Tukey cuts the block into two halves, transforms each half, and joins them.
That step repeats until each part holds one sample, thus the cost falls to
`n*log(n)`. For a block of 1024 samples that is about a hundred times less
work.

The cut into halves only works when the size divides by two down to one, thus
the size must be a power of two. `fft_is_valid_size` examines a size.

Two tables depend on the size only: the turning factors, which are points on a
circle, and the order of the bit reversal, which is the order in which the
halves come back together. `fft_alloc` calculates both one time, thus a
transform itself gets no memory.

**Read the result like this.** A signal of real values gives a result whose
second half mirrors the first half. Only the bins from 0 to `size/2` hold new
information. `fft_bin_frequency` gives the frequency of a bin in hertz.

**One trap.** A tone that does not lie on a bin gives its energy to many bins
around it, thus the peak becomes wide and low. Choose the size of the block so
that the tone you look for lands on a bin, if you can.

## goertzel

When you know which frequencies you look for, and there are only a few, the
whole transform is waste. This algorithm gives one frequency for the cost of
one multiplication and two additions for each sample, and it holds three float
values.

It works as a filter with two poles that rings at the frequency you chose.
After a whole block, the two values of its state give the answer.

Use it when you watch for a few known tones, such as the tones of a telephone
keypad. When you need more than about `log2(n)` frequencies, the whole
transform costs less.

## hilbert

The transform gives the analytic signal: the signal itself in the real part,
and the signal moved by a quarter turn in the imaginary part. From that pair
you read two things at each moment:

- the **amplitude**, which is the distance from zero. It follows the envelope.
- the **phase**, whose change from one sample to the next gives the
  **instantaneous frequency**.

The module builds it through the fast Fourier transform: take the spectrum,
set every negative frequency to zero, double every positive one, and transform
back. Thus the size must be a power of two.

**The rule that decides whether you can use it.** These values only mean
something for a signal that holds one frequency at a time. A signal that holds
several frequencies together gives a mean of them, which describes nothing.
That rule is why the next module exists.

## hht

The Hilbert-Huang transform joins two parts. The empirical mode decomposition
in `sptk/decompose` splits the signal into intrinsic mode functions, each of
which holds one frequency at a time. The Hilbert transform then reads the
amplitude and the frequency of each one.

The result says which frequency the signal holds at which moment. A Fourier
transform gives the frequencies of the whole block and says nothing about the
time, thus it answers badly for a signal whose frequency changes. This
transform answers well.

`hht_mean_frequency` weighs each moment by the square of its amplitude. A
moment with a small amplitude holds a phase that noise moves easily, thus it
gets little weight.

## dwt

A Fourier transform says which frequencies a signal holds but not where they
are. A wavelet transform says both.

One level splits the signal into an approximation, which holds the slow part
at half the number of samples, and a detail, which holds the fast part, also at
half. Together they hold as many values as the signal, thus nothing is lost and
`dwt_inverse` gives the signal back.

The main use is to take noise out of a signal:

1. take the transform,
2. set every small value of the detail to zero with `dwt_threshold`,
3. take the inverse transform.

The noise spreads over every value of the detail, while the signal itself holds
few large values. Thus this step takes the noise away and keeps a step or a
spike sharp, which a low pass filter would make round.

The module holds two wavelets. **Haar** looks at two samples at a time, thus it
finds a step very well and a smooth curve badly. **Daubechies with four
coefficients** looks at four samples at a time and follows a curve better.
