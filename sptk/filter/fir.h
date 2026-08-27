#ifndef FIR_H
#define FIR_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/transform/window.h>
#else
#include "real.h"
#include "window.h"
#endif

// A filter with a finite impulse response.
//
// The filter multiplies the last few samples of the signal by a set of
// coefficients and adds the products. It holds no feedback, thus it is always
// stable, and it moves every frequency by the same time. That second point
// matters when the shape of the signal must stay as it is.
//
// The cost is the length: such a filter needs many more coefficients than an
// IIR filter for the same sharpness. Use the iir module when the number of
// operations for each sample matters more than the shape.
//
// The design functions build the coefficients with the method of the windowed
// sinc, with the window of Hamming. Give the cutoff as a part of the sample
// rate, thus 0.25 means one quarter of the sample rate. The value must lie
// between 0 and 0.5, because half the sample rate is the highest frequency
// that a sampled signal can hold.
//
// A longer filter gives a sharper edge between the band that passes and the
// band that stops. A length of about 4/width gives an edge of that width,
// where the width is also a part of the sample rate.

// How wide the change from the pass band to the stop band is, as a number
// divided by the length of the filter.
//
// A filter with a finite impulse response cannot turn from passing to stopping
// at once. The turn takes a band of frequencies, and that band is narrower only
// when the filter is longer. This is the width of that turn, and it is the
// reason a low cutoff needs a long filter.
#define FIR_TRANSITION      REAL_C(2.0)

// CHOOSING THE WINDOW, WHICH IS THE ONE DECISION THIS MODULE ASKS OF YOU
//
// The plain sinc is the perfect filter and it runs for ever. Cutting it to a
// finite length is what a window does, and the window decides two things that
// trade against each other:
//
//   HOW WIDE THE TURN IS from passing to stopping, which wants a narrow window
//   HOW FAR DOWN THE BAND THAT IS STOPPED LIES, which wants a gentle one
//
// Measured, for a low pass of 101 coefficients at a cutoff of 0.25. The turn
// is from where the answer last stands at 0.9 to where it first reaches 0.1:
//
//   window             turn is wide   times the length   band that is stopped
//   rectangular           0.0090            0.90              -26 dB
//   hamming               0.0182            1.84              -58 dB
//   hann                  0.0194            1.96              -55 dB
//   kaiser, beta 6        0.0198            1.99              -68 dB
//   blackman              0.0238            2.40              -75 dB
//   blackman-harris       0.0281            2.83             -104 dB
//
// READ BOTH ENDS TOGETHER. A rectangular window turns three times as sharply
// as a Blackman-Harris for the same length, and lets 26 dB through where the
// Blackman-Harris lets 104. Neither is better; they answer different
// questions.
//
// The third column is the second multiplied by the length, and it is the same
// at 101 coefficients and at 201. That is what says the turn belongs to the
// shape of the window and to the length, and to nothing else.
//
//   TAKE HAMMING where nothing else is known. It is the default of this module
//   and a reasonable answer to most questions.
//   TAKE BLACKMAN or BLACKMAN-HARRIS where a weak signal must be seen beside a
//   strong one, and the filter can afford to be longer.
//   TAKE KAISER where a number has been given for the stop band. Its parameter
//   follows from that number through window_kaiser_beta, thus it is the only
//   window here that can be asked for a specification rather than chosen by
//   name.
//
// A LONGER FILTER MAKES THE TURN NARROWER AND CHANGES NOTHING ELSE. The stop
// band of a window is a property of its shape alone, thus doubling the length
// halves the turn and leaves the depth exactly where it was. To go deeper,
// change the window; to turn faster, lengthen the filter.

// How far either side of a frequency the group delay is measured, for a filter
// that is not symmetric.
#ifndef FIR_GROUP_DELAY_STEP
#if defined(SPTK_REAL_64)
#define FIR_GROUP_DELAY_STEP    REAL_C(0.00001)
#else
#define FIR_GROUP_DELAY_STEP    REAL_C(0.0005)
#endif
#endif

// True if a filter of the given length can hold the given cutoff.
//
// The turn from passing to stopping is FIR_TRANSITION/length wide. A cutoff
// nearer to 0 than that, or nearer to 0.5 than that, has no room for the turn.
// The design then gives back a filter whose pass band never reaches 1, and it
// does so quietly.
//
// Measured, for a low pass of 101 coefficients, where the turn is 0.0198 wide,
// at the gain that should be 1.0 in the pass band:
//
//     cutoff    0.0500   0.0200   0.0100   0.0050   0.0020
//     gain      1.0024   1.0039   0.8443   0.5065   0.2140
//
// The gain holds while the cutoff is above the width of the turn, and falls
// away under it. Thus: make the filter longer, or bring the sample rate down.
bool fir_is_valid_cutoff(uint32_t length, real_t cutoff);

// True if a filter of the given length can hold the given band. Both edges
// must be valid, and the band between them must be at least as wide as the
// turn, or the two edges run into each other and no frequency passes fully.
bool fir_is_valid_band(uint32_t length, real_t low_cutoff, real_t high_cutoff);

typedef struct{
    uint32_t length;            // The number of coefficients
    real_t* coefficient;         // The coefficients
    real_t* history;             // The last samples, length of them
    uint32_t position;          // Where the next sample goes in the history
    bool dynamic_alloc;         // True if the memory comes from the heap
}fir_t;

// Give a filter with the given number of coefficients. The memory comes from
// the heap, and every coefficient and every sample of the history holds zero.
// Give the filter to fir_free when you no longer need it.
fir_t fir_alloc(uint32_t length);

// Give a filter that uses the memory that the caller holds. Both lists must
// hold as many float values as the given length. This function takes no
// memory from the heap.
fir_t fir_static_alloc(uint32_t length, real_t* coefficient, real_t* history);

// Build the coefficients of a filter that lets the low frequencies pass. The
// cutoff is a part of the sample rate, and it must lie between 0 and 0.5.
// Give false and leave the filter as it was if fir_is_valid_cutoff is false.
bool fir_design_low_pass(fir_t* fir, real_t cutoff);

// Build the coefficients of a filter that lets the high frequencies pass.
// Give false and leave the filter as it was if fir_is_valid_cutoff is false.
bool fir_design_high_pass(fir_t* fir, real_t cutoff);

// Build the coefficients of a filter that lets a band of frequencies pass. The
// low cutoff must be smaller than the high cutoff, and both must lie between 0
// and 0.5.
// Give false and leave the filter as it was if fir_is_valid_band is false.
bool fir_design_band_pass(fir_t* fir, real_t low_cutoff, real_t high_cutoff);

// Build the coefficients of a filter that stops a band of frequencies and lets
// everything else pass. Reach for this where one narrow thing must go and the
// rest of the band must stay: the hum of a mains supply under a measurement,
// or a carrier that a sensor leaks into its own reading. A low pass or a high
// pass would take half the band with it.
//
// The length must be odd, for the reason fir_design_high_pass gives.
// Give false and leave the filter as it was if fir_is_valid_band is false.
bool fir_design_band_stop(fir_t* fir, real_t low_cutoff, real_t high_cutoff);

// Write one coefficient. Use this function to give the filter a set of
// coefficients that another program calculated.
// Build a low pass with the window of your choosing.
//
// The window decides how wide the turn is and how far down the band that is
// stopped lies; the header above measures both for every window. The parameter
// belongs to the window and is ignored where the window takes none.
//
// Give false if the window is unknown, if it cannot be built at this length,
// or if the length cannot hold this cutoff.
bool fir_design_low_pass_with(fir_t* fir, real_t cutoff, window_kind_t kind,
                              real_t parameter);

// Build a high pass with the window of your choosing. The length must be odd,
// for the reason fir_design_high_pass gives.
bool fir_design_high_pass_with(fir_t* fir, real_t cutoff, window_kind_t kind,
                               real_t parameter);

// Build a band pass with the window of your choosing.
bool fir_design_band_pass_with(fir_t* fir, real_t low_cutoff,
                               real_t high_cutoff, window_kind_t kind,
                               real_t parameter);

// Build a band stop with the window of your choosing. The length must be odd,
// for the reason fir_design_high_pass gives.
//
// THE WINDOW MATTERS MORE HERE THAN ANYWHERE ELSE. What a band stop is asked
// to do is take one thing out and leave what stands beside it, thus how far
// down the stopped band lies is the whole of its worth. The plain window in
// the table reaches 21 dB down, which leaves a tenth of the hum. Choose one
// that reaches further.
bool fir_design_band_stop_with(fir_t* fir, real_t low_cutoff,
                               real_t high_cutoff, window_kind_t kind,
                               real_t parameter);

// Give how wide the turn from passing to stopping is, for this window at this
// length, as a part of the sample rate.
//
// The turn of a window is a fixed number divided by the length, thus this is
// that number divided by the length. The numbers were measured and they are in
// the table in the header.
real_t fir_transition_width(window_kind_t kind, uint32_t length);

// Give how long a filter must be for the turn to be this narrow.
//
// ASK THIS BEFORE ALLOCATING. A turn of a hundredth of the sample rate wants
// 91 coefficients with a Hamming window and 551 with a Blackman-Harris, and
// choosing the window without knowing that is choosing blind.
//
// The answer is always odd, because a high pass and a band stop need a middle
// coefficient. Give 0 where the window is unknown or the width is not above
// nothing.
uint32_t fir_length_for(window_kind_t kind, real_t width);

// Write one coefficient directly, for a filter whose shape comes from
// somewhere other than the designs above.
void fir_set_coefficient(fir_t* fir, uint32_t index, real_t value);

// Give one coefficient.
real_t fir_get_coefficient(fir_t* fir, uint32_t index);

// Give the filtered value of one sample. The filter keeps the sample in its
// history, thus the next call sees it.
real_t fir_process_sample(fir_t* fir, real_t sample);

// Filter a block of samples. The input and the output may be the same list.
void fir_process_block(fir_t* fir, const real_t* input, real_t* output, uint32_t size);

// Set every sample of the history to zero. The filter then behaves as a filter
// that has seen no sample yet.
void fir_reset(fir_t* fir);

// Give the size of the answer of the filter at the given frequency, which is a
// part of the sample rate. A value of 1 says that the frequency passes
// unchanged, and a value of 0 says that the filter stops it.
real_t fir_get_gain(fir_t* fir, real_t frequency);

// Give how far the filter turns the phase at one frequency, in radians.
//
// The frequency is a part of the sample rate, and the answer runs from -pi to
// pi.
real_t fir_phase(fir_t* fir, real_t frequency);

// Give how long the filter holds back the frequencies about this one, in
// samples.
//
// THE ANSWER IS THE SAME AT EVERY FREQUENCY FOR A FILTER BUILT BY THE DESIGNS
// ABOVE, and that is the whole reason to choose a filter of this kind. Every
// design here is symmetric, thus every frequency is held back by exactly half
// the length less one half, and a waveform comes out moved along and not bent.
//
// Measured against the iir module, on filters that meet the same
// specification: an FIR holds every frequency back by the same time, while a
// Butterworth of 10 sections rises from 41 samples to 93 across the band that
// passes and an elliptic of 3 rises from 14 to 87. That is what an FIR costs
// its length for.
//
// A filter whose coefficients were written by hand through
// fir_set_coefficient need not be symmetric, and then this is worked out from
// the phase either side, as the iir module does it.
real_t fir_group_delay(fir_t* fir, real_t frequency);

// True if the coefficients read the same forwards and backwards.
//
// Every design in this module gives a symmetric filter. One built by hand
// through fir_set_coefficient may not be, and only a symmetric filter holds
// every frequency back by the same time.
bool fir_is_symmetric(fir_t* fir);

// Release the memory of a filter that came from fir_alloc. This function does
// nothing for a filter that came from fir_static_alloc.
void fir_free(fir_t* fir);

#endif//FIR_H
