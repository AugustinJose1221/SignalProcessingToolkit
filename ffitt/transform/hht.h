#ifndef HHT_H
#define HHT_H

#include <stdint.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/fft.h>
#include <ffitt/decompose/imf.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#include "imf.h"
#endif

// The Hilbert-Huang transform.
//
// The transform joins the two parts that this library already holds:
//
// 1. The empirical mode decomposition takes a signal apart into intrinsic mode
//    functions. Each function holds one frequency at a time.
// 2. The Hilbert transform gives the amplitude and the frequency at each point
//    of time of such a function.
//
// The result says which frequency the signal holds at which time, and how
// strong it is. A Fourier transform gives the frequencies of the whole signal
// and says nothing about the time. Thus the Hilbert-Huang transform suits a
// signal whose frequency changes, where a Fourier transform gives a wide band
// and no clear answer.
//
// The Hilbert transform needs the size to be a power of two, because it uses
// the fast Fourier transform. Give the decomposition a signal of such a size.

// Give the amplitude and the frequency at each point of time, for one
// intrinsic mode function.
//
// The function writes size values into the amplitude list, and size-1 values
// into the frequency list, because a frequency needs two points of the phase.
// The work buffer must hold size complex numbers. The function gets no memory.
//
// The size must be the same as the size of the transform, and it must be a
// power of two.
void hht_transform_imf(fft_t* fft, imf_t* imf, cnum_t* work,
                       real_t* amplitude, real_t* frequency, real_t sample_rate);

// Give the amplitude and the frequency for a list of intrinsic mode
// functions, one after the other.
//
// The lists amplitude and frequency hold the result of each function one after
// the other. Thus the amplitude list must hold count*size values, and the
// frequency list must hold count*(size-1) values. The work buffer must hold
// size complex numbers.
void hht_transform(fft_t* fft, imf_t* imf, uint32_t count, cnum_t* work,
                   real_t* amplitude, real_t* frequency, real_t sample_rate);

// Give the mean frequency of one intrinsic mode function, where each point
// counts as much as the square of its amplitude.
//
// A point with a small amplitude holds a phase that noise moves easily. This
// mean gives such a point little weight, thus it describes the function better
// than a plain mean does.
real_t hht_mean_frequency(const real_t* amplitude, const real_t* frequency,
                         uint32_t size);

#endif//HHT_H
