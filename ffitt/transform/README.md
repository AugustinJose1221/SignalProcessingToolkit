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
| `convolve` | What comes out when a signal passes through a shape? | Size times shape, or three transforms |
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

## convolve

Sliding one signal along another, multiplying and adding at every place. This
is the most basic operation in the field: passing a signal through a filter
**is** a convolution with that filter's coefficients, and `fir` is this
operation done one sample at a time. A test holds that, running the same signal
through a filter and through a convolution with its coefficients and finding
the two agree.

**How it differs from correlation, which is the usual confusion.** The two are
the same sum with one difference: a convolution **turns one signal round**
before sliding it. For a shape that reads the same forwards and backwards they
agree, and a great deal of code is written on that assumption and then meets a
shape that does not. Take a convolution when a signal **passes through**
something; take a correlation when asking **how alike** two things are.

**The mode must be chosen deliberately.**

| Mode | Length | |
| --- | --- | --- |
| `CONVOLVE_FULL` | n+m-1 | every place; the ends are built from a signal assumed to be zero outside itself |
| `CONVOLVE_SAME` | n | the middle of the full answer, lined up with the input sample for sample |
| `CONVOLVE_VALID` | n-m+1 | only where the shape lies wholly inside; **nothing is assumed** |

Take `CONVOLVE_VALID` when the ends matter. The other two report values at the
ends that were partly invented, and they look no different from the rest.

**The fast way.** A convolution in time is a multiplication in frequency. For a
signal of 4096 and a shape of 512 the plain way costs two million operations
and the transform about 400 thousand. Below a shape of about 60 the plain way
wins. It takes its transform and working memory from the caller, thus it needs
no heap.

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
in `ffitt/decompose` splits the signal into intrinsic mode functions, each of
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

## bluestein

**A transform of any size.** `fft` takes a power of two and nothing else, which
is the right trade when the block size is a choice. Some sizes are not a choice:
at 3000 samples in a second one period of 50 hertz is 60 samples, a day is 1440
minutes, a turn of a shaft is however many readings the machine gives. Rounding
such a size up to a power of two and filling with zeros moves every bin off the
frequency that the size was chosen for.

**Use `fft` where the size is a power of two.** Measured on the same size,
`bluestein` takes 5.1 times as long and gives nothing extra:

| size | `fft` | `bluestein` |
|---|---|---|
| 256 | 0.0055 ms | 0.0284 ms |
| 1024 | 0.0244 ms | 0.1243 ms |
| 4096 | 0.1151 ms | 0.5882 ms |

**The accuracy is the same order as `fft`.** Measured against a transform of the
same size worked out directly, the worst error across all bins as a part of the
largest bin, at 32 bits: 0.0000004 at a size of 60 and 0.0000006 at a size of
1000. At 64 bits it is below what those figures can show.

**The square of the index is where it would fall apart.** The turning factors
follow `n` squared, and for a size of 200000 the last one asks for the sine of
an angle near ten to the eleventh. A number of 32 bits holding an angle that
large has no digits left for where in the turn it lands. The module folds the
square back into one turn before it forms any angle. Measured on a single tone,
the worst false answer as a part of the tone, at 32 bits:

| size | 1 000 | 10 000 | 50 000 | 200 000 |
|---|---|---|---|---|
| with the fold | 0.0000001 | 0.0000000 | 0.0000000 | 0.0000001 |
| without it | 0.0000076 | 0.0001036 | 0.0003396 | 0.0014121 |

The fold holds the error flat across the whole range. Without it the error grows
with the size, and by 200000 it is four orders worse.

**It holds working room of its own.** Beside the tables of the transform inside
it, it keeps one turning factor for each point of the size asked for and three
buffers of the larger size. `bluestein_static_alloc` takes all of that from the
caller, thus a device with no heap can still use it.

## stft

**One transform of a recording says which frequencies it holds and nothing
about when.** A recording of a bird and then a car gives the same answer as a
car and then a bird. `stft` cuts the signal into short overlapping pieces and
transforms each one, which is what almost every real question wants.

**The trade cannot be escaped.** A block of `n` samples at a rate of `r` covers
`n/r` seconds and its bins stand `r/n` hertz apart. The product is 1 whatever is
chosen:

| block at 8000 samples in a second | covers | bins stand apart |
|---|---|---|
| 128 | 16 ms | 62.5 Hz |
| 1024 | 128 ms | 7.8 Hz |
| 8192 | 1.02 s | 0.98 Hz |

Choose the block from the question. Speech changes every 20 ms; a shaft turning
at 50 hertz wants a block of a second or more. There is no default that suits
both.

**Two different things stop a signal being put back together.** The first is the
window and the hop together: `stft_can_rebuild` says whether every sample inside
the signal carries enough weight, and a hann window at a hop of the whole block
is refused because it multiplies the first sample of every block by zero.

The second **catches everyone**: the two ends of the signal itself. The very
first sample is under the first block only, where a sample in the middle is
under as many blocks as fit across it. `stft_solid_range` gives the stretch
where the cover is full; outside it `stft_inverse` **writes zero** rather than a
number that looks like an answer. Where the ends matter, put a block of zeros
before the signal and another after it.

Inside that stretch the rebuild is exact — the worst error at 32 bits is
0.0000005 for every window at a hop of a quarter or a half of the block, which
is the rounding of the transform and nothing more.

**The window is laid a second time on the way back.** That is what keeps the
joins from showing when the frames have been changed in between, which is the
usual reason for taking a signal apart at all.

## spectrogram

**A complex number is not something to look at.** This turns the frames of
`stft` into one real number for each bin, in one of four units.

**The scaling is the part that is usually wrong**, and the wrong answer looks
perfectly reasonable. A longer block gives larger numbers for the same signal, a
window makes them smaller, and half the power sits in the mirrored half that is
not there. Corrected, **a wave of amplitude A reads A**, whatever the block, the
window or the hop. Measured on a wave of amplitude 2:

| window | block 128 | block 512 | block 2048 |
|---|---|---|---|
| rectangular | 2.00000 | 2.00000 | 2.00000 |
| hann | 1.99999 | 2.00000 | 2.00000 |
| blackman | 2.00000 | 2.00000 | 2.00000 |

Ask for `SPECTROGRAM_AMPLITUDE` to read the size of a tone off a picture,
`SPECTROGRAM_DENSITY` to add the power of a band together, and
`SPECTROGRAM_DECIBEL` to draw the picture at all — anything real covers so many
factors of ten that a linear scale shows one bright line and black everywhere
else.

**The floor under the decibels is not a detail.** The logarithm of nothing has
no value, and a bin holding nothing is a thing that happens: a silent stretch, or
a bin above the cutoff of a filter. `spectrogram_against_the_largest` measures
everything from the loudest reading, which is how a spectrogram is nearly always
drawn.

## csd

**What two signals have in common at each frequency.** Both a machine and a
floor hold a peak at 50 hertz; that proves nothing, because half the building
holds a peak at 50 hertz. Coherence says whether the two peaks move together.

**A single block gives a coherence of exactly 1, always.** Two signals of pure
noise, with nothing whatever in common, read 1 at every frequency. It is not a
rounding matter and no width fixes it: with one block the arithmetic is a number
divided by itself. Measured on two unrelated noise signals, where the truth is 0:

| blocks | 1 | 2 | 4 | 8 | 16 | 32 | 64 |
|---|---|---|---|---|---|---|---|
| mean reading | 1.00 | 0.46 | 0.35 | 0.13 | 0.06 | 0.04 | 0.02 |

The reading falls as about one over the number of blocks. **A reading of 0.35 is
evidence of nothing if it came from 4 blocks.** The module refuses below
`CSD_SMALLEST_BLOCK_COUNT`, and above it the table is the rule of thumb.

**`csd_transfer` gives the gain and the phase of whatever lies between two
signals**, at every frequency at once, without putting a single tone through it.
It is blind to noise added to the output, which is the usual case — sensor noise
at the output does not bend the answer, while noise on the input does. **Look at
the coherence beside it**: where the coherence is low the gain is still a number,
and it is a number about nothing.

**Both signals must be measured at the same moments.** Two recordings started a
second apart hold the same events at different sample numbers, and every answer
here is then about a relation that is not there.

## slide

**One frequency, answered at every sample.**

`fft` needs the whole block in memory and answers when the block is full.
`goertzel` needs no block but answers once per block and must be reset between
them. Between the two sits the case that neither serves: a program that must
know, at EVERY sample, how much of two or three known frequencies the last N
samples held.

A watcher holds one running total for each frequency. Each new sample costs one
complex multiplication and two additions per frequency, whatever N is. The
window itself is still kept, so that the sample which leaves can be taken away,
and that is the only part whose memory follows N.

**Read the damping before using it.** The recurrence is only just stable: the
turning factor has a magnitude of exactly one, thus every rounding error stays
in and circles for ever. The damping makes the factor a shade smaller so that
errors fade, and the price is an answer that reads about 1.3 percent low. The
header measures both halves, at both widths, and the answer is not the same at
each: at 64 bits the plain recurrence drifted by four parts in ten thousand
million over twenty million samples, and a caller there who wants the answer to
read true should switch the damping off.

Take `fft` when the whole spectrum is wanted. Past about log2(N) frequencies
the whole transform is cheaper, which is the same crossover `goertzel` names.

## dct

`fft` turns a signal into sines **and** cosines, which is what you need to say
where in its turn each frequency stands. **Where the phase is not wanted, half of
that is wasted**: a real signal of n samples becomes n complex numbers holding 2n
numbers, of which n are the mirror of the others.

This turns n samples into n cosines and nothing else. It is the transform behind
every compression of a picture or a sound anybody uses, and the reason is one
property: **it gathers a smooth signal into its first few numbers.**

Measured on a slow curve of 64 samples that does not come back to where it
started — how many numbers hold each share of it:

| share kept | `dct` | `fft` |
|---|---|---|
| 0.99 | 4 | 20 |
| 0.999 | 8 | — |
| 0.99999 | 30 | — |

The `fft` column counts **numbers, not bins**: ten of its bins carry 0.99 of that
curve and each bin is a complex number. The same curve, five times the room.

**Why it wins, and it is not the arithmetic.** A transform treats the block as
one turn of something that repeats, so a signal that starts low and ends high has
a **step** where the end meets the beginning — and a step needs every frequency
there is. This treats the block as half a turn of something mirrored, so the end
meets its own mirror and there is no step.

A signal of noise needs all 64 either way. There is nothing to gather.

**What it cannot do.** It says nothing about phase, so it cannot be used to
filter by multiplying and transforming back, and it cannot say where in its turn
a tone stands. Reach for `fft` for those.

**What it costs.** Time proportional to the **square** of the size, where `fft`
is the size multiplied by its logarithm — about four times the work at 64 and a
hundred times at 1024. Against it, this takes any size at all rather than a power
of two.

## cepstrum

A voice, a violin string and an engine all make a tone with harmonics. In the
spectrum those are a row of peaks **evenly spaced**, and how far apart they stand
is the frequency of the note. A row of evenly spaced peaks is itself a thing that
repeats, and the way to find a thing that repeats is a transform — so this takes
a transform **of a spectrum**, and the row comes out as one peak.

The place of that peak is called a quefrency, and it is a time: a peak at 80
means the note repeats every 80 samples.

**Why not just correlate.** `correlate_best_lag` is cheaper and for a plain
repeating signal it is the right answer. Measured on a note whose true period is
64 samples:

| harmonics present | `cepstrum` | `correlate_best_lag` |
|---|---|---|
| 1 to 12 | 64 ✓ | 192 ✗ |
| 2 to 12, no fundamental | 64 ✓ | 128 ✗ |
| 3 to 12 | 64 ✓ | 64 ✓ |

A small loudspeaker cannot make 100 Hz, so a note at 100 Hz arrives as 200, 300,
400 and nothing at 100. The ear still hears 100. Correlation sees a signal that
repeats at a shorter period and says so **with a strength of 1.000 while it
does**; this sees harmonics 100 apart and answers 100.

**It also finds an echo.** A sound and the same sound again a little later
multiply the spectrum by a ripple, and a ripple in the spectrum is a peak here.
The logarithm is what makes that work: an echo *multiplies* the spectrum, and a
logarithm turns multiplying into adding, so what the room did and what the source
did stand side by side instead of one wrapped around the other.

**The block is windowed, and that is not a detail.** It was written once without
a window, on the reasoning that a note whose period divides the block needs none.
The *noise* on that note does not divide the block: it leaks across every bin and
its leakage has strong structure in the logarithm. Without a window, a note with
no fundamental under a twentieth of noise came back at **255** where 64 was
right, and moved about with the floor and with the width of the build. With a
window every one of those cases gives 64, at both widths.

**What it cannot do.** The answer is a whole number of samples and a real period
rarely is — a period of 100 comes back as 99. Give the cepstrum to
`peakdetect_refine` to get between the samples. It needs several harmonics;
below about eight, treat the answer as a hint. And the strength tells structure
from noise (0.06 against 0.2–0.7) but **not a single tone from a note**: one peak
in a spectrum is structure too.
