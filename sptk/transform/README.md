# Transforms

A signal arrives as a list of samples through time. These modules say which
frequencies stand inside it. Each one answers a different question, and the
question decides which one you want.

| Module | The question it answers | What it costs |
| --- | --- | --- |
| `fft` | Which frequencies does this block hold? | Memory for the whole block |
| `window` | How do I stop one tone smearing over the others? | One multiplication for each sample |
| `psd` | How much power at each frequency, measured steadily? | One transform for each block |
| `correlate` | How long is the delay, does this repeat, is this shape in there? | Size times lags, or three transforms |
| `goertzel` | How much of *this one* frequency does it hold? | Three float values |
| `hilbert` | What is the amplitude and the frequency at each moment? | One transform each way |
| `hht` | Which frequency at which moment, for a signal that changes? | A decomposition and a transform |
| `dwt` | Which frequencies, and *where* in the signal? | One pass for each level |

## window

**Read this before `fft`, because a transform without a window is nearly
always wrong.**

A transform reads a block of samples and takes it to be one period of a signal
that repeats for ever. Almost no real signal fits a block exactly. The end of
the block then does not meet its start, and the transform sees a step there. A
step holds every frequency, thus one tone smears across the whole result and a
small tone beside a large one disappears under it.

A window is a list of numbers that the block is multiplied by first. It falls
to nothing at both ends, thus the block always meets itself and there is no
step.

**Which one to take.** Every window trades two things. A tone that does not sit
exactly on a bin spreads over the bins beside it, and a wider spread hides a
tone that stands close. What is left over reaches further out as side lobes,
and higher ones hide a tone that stands far away but is weak. Measured for a
window of 64 samples:

| Window | Highest side lobe | Noise bandwidth | Take it when |
| --- | --- | --- | --- |
| Rectangular | -13 dB | 1.00 bins | The block already fits |
| Hann | -32 dB | 1.52 bins | Nothing else is known |
| Hamming | -42 dB | 1.38 bins | A near tone must be seen |
| Blackman | -58 dB | 1.75 bins | A weak tone beside a strong one |
| Blackman-Harris | -92 dB | 2.04 bins | The same, when the gap is very wide |
| Tukey | follows its parameter | | Only the ends may fall |
| Kaiser | follows its parameter | | The side lobes must meet a number |

**Do not forget the gain.** A window makes the signal smaller, thus every
height in the result is too low. `window_coherent_gain` gives the number to
divide a tone by, and `window_noise_gain` gives the number to divide noise by.
A Hann window has a coherent gain of 0.5, thus a reading that does not divide
by it is wrong by a factor of two. This is the usual fault.

**One trap in the rule of Kaiser.** `window_kaiser_beta` takes the stop band
of a FILTER that the window builds. That is not the level of the side lobes of
the window itself: the side lobes always lie about 18 to 27 dB higher. A beta
for a stop band of 60 dB gives a window whose own side lobes stand only 42 dB
down. The header holds a table of both.

The module gets no memory. It writes into a list that the caller holds.

## psd

How much power a signal holds at each frequency, by the method of Welch.

**One transform of a long signal is a poor measurement.** Making the signal
twice as long gives twice as many bins, each still as noisy: the answer becomes
finer and no more certain. Welch cuts the signal into overlapping blocks,
transforms each, and takes the mean. The bins are coarser and the noise in each
falls as the number of blocks grows:

| | frequency | steadiness |
| --- | --- | --- |
| fewer, longer blocks | fine | noisy |
| more, shorter blocks | coarse | steady |

**The scaling is the part that is usually wrong.** A density is power for each
hertz, thus its numbers must not change when the block, the window or the
overlap changes. Three corrections get there, and leaving any out gives an
answer that looks reasonable and is wrong by a factor nobody notices:

- **the window** makes the signal smaller. The correction is the sum of the
  SQUARES of the window and not the sum of it, because power follows the
  square. Using the wrong one is out by about a quarter for a Hann window.
- **the sample rate** turns power for each bin into power for each hertz.
- **the other half of the spectrum** holds the same power again at the negative
  frequencies, thus every bin but the first and the last is doubled.

With all three, a wave of amplitude A has an area under the curve of `A*A/2`,
whatever the block, the window or the overlap. Three tests hold exactly that,
each varying one of the three.

**Read the area, not the height.** The number for one bin means little on its
own. `psd_band_power` adds the density over a band, which gives the power the
signal holds between those two frequencies.

**Overlap by half the block.** A window throws away the samples at the ends of
every block; overlapping uses them again in the next one. More than half costs
work and gains little, because blocks that overlap heavily hold much the same
samples and their noise no longer averages away.

## correlate

How much one signal is like another when one of them is moved in time. Three
questions that come up again and again are all this one question with different
signals put into it.

**How long is the delay** between two recordings of the same thing? Correlate
the two; the lag where the answer is largest is the delay.

**Does this signal repeat, and how often?** Correlate the signal with itself. A
signal that repeats every 100 samples has a peak at a lag of 100.
`correlate_best_lag` is that whole job in one call.

**Is this shape in that signal?** Correlate the signal with the shape.

**The scaling decides what the number means.** The raw sum grows with the
length of the signal and with how large the samples are, thus two answers
cannot be set beside each other:

| Scaling | What it gives |
| --- | --- |
| `CORRELATE_RAW` | The sum of the products |
| `CORRELATE_BIASED` | Divided by the number of samples |
| `CORRELATE_UNBIASED` | Divided by how many samples overlapped at that lag |
| `CORRELATE_COEFFICIENT` | Between -1 and 1 |

**Take the coefficient when the answer must be judged** and not only compared
with itself. It is the only one that means the same for every signal: 1 is a
perfect match, 0 no likeness, -1 the same shape upside down. A threshold on it
holds from one recording to the next.

**The mean must come off first, and the coefficient takes it off.** A signal
that never goes below zero correlates well with itself at every lag, because
the product of two positive numbers is positive whatever the lag. The mean then
swamps the part that actually repeats.

The coefficient is worked out over the samples that overlap at each lag and no
others. The shorter way, the sum at each lag divided by the sum at no lag,
falls away as the lag grows: at a lag of an eighth of the signal it comes out
an eighth too small, and a threshold on it would then hold for one length of
signal and not another.

**The fast way, and when it is not.** The plain method costs size times lags:
for 4096 samples and 4096 lags, 17 million operations. A correlation in time is
a multiplication in frequency, thus `correlate_auto_by_transform` does the same
work in three transforms, about 300 thousand operations. Below about 300
samples the plain method wins, because the transform has a fixed cost that the
plain method has not.

The fast way serves the three scalings that are sums and not the coefficient,
because a transform gives the sum at each lag and nothing else. It takes its
transform and its working memory from the caller, thus it needs no heap.

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
