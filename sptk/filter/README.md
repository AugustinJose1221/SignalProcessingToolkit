# Filters

A filter takes a band of frequencies away and keeps the rest. The three
modules here reach that goal in three different ways, and each way pays a
different price.

| Module | How it works | Choose it when |
| --- | --- | --- |
| `fir` | A weighted sum of the last few samples | The shape of the signal must stay |
| `iir` | Feeds its own output back into itself | The work for each sample must be small |
| `savgol` | Lays a polynomial through a window | A peak must keep its height |

## fir

A filter with a finite impulse response multiplies the last `n` samples by `n`
coefficients and adds the products. It holds no feedback.

Two things follow from that. It can never run away, because nothing feeds back.
And it moves every frequency by the same time, which is half the length of the
filter. Thus the shape of the signal stays as it is, and a peak arrives later
but not deformed.

The price is the length. A sharp edge between the band that passes and the band
that stops needs many coefficients. A rule of thumb: a filter of about `4/w`
coefficients gives an edge of the width `w`, where `w` is a part of the sample
rate.

**How the design works.** The perfect low pass filter is a sinc curve, which
runs for ever. The design cuts it to the length of the filter, which brings
large waves into the band that stops. A window takes the two ends of the
coefficients down to almost zero and makes those waves small. This module uses
the window of Hamming.

A high pass filter is the whole band less a low pass filter, thus the design
changes the sign of every coefficient and adds one in the middle. A band pass
filter is a wide low pass filter less a narrow one.

`fir_get_gain` says how much of a frequency the filter lets through, without
running a signal through it.

## iir

A filter with an infinite impulse response feeds its output back into itself.
Thus a small number of coefficients gives a sharp edge: one section needs five
multiplications, where a finite filter of the same sharpness needs dozens.

The price is that it moves different frequencies by different times, thus the
shape of the signal changes. And a filter with bad coefficients can run away,
because the feedback can grow.

**How the design works.** The shape of a filter is easy to describe for a
signal that runs on, and this module holds a filter of Butterworth, whose band
that passes is as flat as it can be. The bilinear transform brings that shape
to a signal of samples. It bends the frequency, thus the design first bends the
cutoff the other way. The test of the module examines the result: the gain at
the cutoff must be `1/sqrt(2)` for every order, which is what a filter of
Butterworth gives.

The filter is a chain of biquad sections. Each section holds two poles, thus
the order of the whole filter is two times the number of sections. Each section
keeps its state in the form of Direct Form II transposed, which holds the error
of a float better than the plain form.

## savgol

This filter smooths a signal and keeps its shape. It takes a window of samples,
lays a polynomial through them by the method of the least squares, and gives
the value of that polynomial at the middle of the window.

A plain mean of a window makes a peak lower and wider, because it pulls the
middle down towards its neighbours. A polynomial can follow a peak, thus this
filter keeps the height. That matters when the height and the width of a peak
carry the information, as in a spectrometer or a chromatograph.

**It also gives a derivative.** The derivative of the polynomial at the middle
of the window is a far better answer than the difference of two samples, which
noise disturbs strongly. Give the number of the derivative to `savgol_design`:
0 for the smoothed signal, 1 for the first derivative, 2 for the second.

**Where the coefficients come from.** The design builds the matrix of the
powers of the positions in the window and solves the normal equations of the
least squares with the module `sptk/linalg`. That work happens one time, inside
`savgol_design`. The filter itself then multiplies and adds only, thus it gets
no memory while it runs.

The window must hold an odd number of samples, so that it has a middle. The
order of the polynomial must be below the size of the window. A higher order
follows the signal more closely and takes away less noise.
