#ifndef HILBERT_H
#define HILBERT_H

#include <stdint.h>

#ifndef TEST
#include <sptk/linalg/cnum.h>
#include <sptk/transform/fft.h>
#else
#include "cnum.h"
#include "fft.h"
#endif

// The Hilbert transform and the analytic signal.
//
// The analytic signal of a real signal holds the signal itself in the real
// part and the Hilbert transform of the signal in the imaginary part. The
// analytic signal gives two values at each point of time:
//
// - the instantaneous amplitude, which is the distance of the point from zero.
//   It follows the envelope of the signal.
// - the instantaneous phase, which is the angle of the point. The change of
//   the phase from one sample to the next gives the instantaneous frequency.
//
// The module builds the analytic signal with the fast Fourier transform. It
// takes the spectrum of the signal, sets every negative frequency to zero,
// doubles every positive frequency, and takes the inverse transform. Thus the
// size of the signal must be a power of two, as the fft module asks.
//
// These values only have a meaning for a signal that holds one frequency at a
// time. A signal that holds several frequencies together gives a mean of them,
// which describes nothing. For that reason the Hilbert transform goes together
// with the empirical mode decomposition, which takes a signal apart into such
// single frequency parts. The hht module joins the two.

// Give the analytic signal of a real signal.
//
// The signal and the work buffer must hold as many values as the size of the
// transform. The function writes the result into the work buffer, thus it gets
// no memory.
void hilbert_analytic_signal(fft_t* fft, const float* signal, cnum_t* analytic);

// Write the instantaneous amplitude of each point into the amplitude list. The
// amplitude follows the envelope of the signal, and it is never less than
// zero.
void hilbert_amplitude(const cnum_t* analytic, float* amplitude, uint32_t size);

// Write the instantaneous phase of each point into the phase list. The phase
// lies between -pi and pi.
void hilbert_phase(const cnum_t* analytic, float* phase, uint32_t size);

// Write the instantaneous frequency of each point into the frequency list.
//
// The frequency comes from the change of the phase between two samples. The
// function takes the change into the range from -pi to pi before it makes the
// frequency, because the phase itself jumps from pi to -pi.
//
// The list holds one value less than the signal, because a change needs two
// points. The caller gives the sample rate in samples for each second, and the
// result is in hertz.
void hilbert_frequency(const cnum_t* analytic, float* frequency, uint32_t size,
                       float sample_rate);

#endif//HILBERT_H
