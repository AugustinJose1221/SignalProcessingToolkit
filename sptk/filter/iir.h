#ifndef IIR_H
#define IIR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>

// A filter with an infinite impulse response, as a chain of biquad sections.
//
// Such a filter feeds its own output back into itself. Thus it gives a sharp
// edge with very few operations for each sample: a section of two poles needs
// five multiplications, where an FIR filter of the same sharpness needs
// dozens. The cost is that the filter moves the different frequencies by
// different times, and that a filter with bad coefficients can run away.
//
// One section holds two poles. The order of the whole filter is two times the
// number of sections, thus a filter of the order 4 needs two sections. The
// design functions build the coefficients of a filter of Butterworth, whose
// band that passes is as flat as it can be.
//
// Give the cutoff as a part of the sample rate, thus 0.25 means one quarter of
// the sample rate. The value must lie between 0 and 0.5.
//
// Each section keeps its state in the form of Direct Form II transposed. That
// form needs two values for each section, and it holds the error of a float
// better than the plain form does.

// The number of coefficients of one section: b0, b1, b2, a1 and a2.
#define IIR_COEFFICIENT_COUNT       5u

// The number of values of the state of one section.
#define IIR_STATE_COUNT             2u

// The number of float values that a filter with the given number of sections
// needs for its coefficients.
#define IIR_COEFFICIENT_SIZE(sections)  ((sections) * IIR_COEFFICIENT_COUNT)

// The number of float values that a filter with the given number of sections
// needs for its state.
#define IIR_STATE_SIZE(sections)        ((sections) * IIR_STATE_COUNT)

typedef struct{
    uint32_t sections;          // The number of biquad sections
    float* coefficient;         // Five coefficients for each section
    float* state;               // Two values for each section
    bool dynamic_alloc;         // True if the memory comes from the heap
}iir_t;

// The smallest cutoff that a design in single precision can hold.
//
// A section keeps its poles near the circle when the cutoff is low, and how
// near decides how much precision the coefficients need. A float holds about
// seven digits. Below this cutoff those digits run out: the coefficients round
// to values that no longer describe the filter that was asked for, and the
// filter gives a wrong answer WITHOUT SAYING SO.
//
// Measured, at the gain that should be 1.0 at zero frequency:
//
//     cutoff    0.0100   0.0020   0.0010   0.0005   0.0001
//     gain      1.0000   1.0014   0.9959   0.9909   0.6849
//
// Thus 0.002 and above is safe, 0.001 costs about one percent, and below
// 0.0005 the answer means nothing. The limit stands at 0.001.
//
// A cutoff below this is nearly always a sign that the sample rate is too high
// for the work. A cutoff of 0.5 Hz against 32 kHz is 0.000016 and cannot be
// held; the same cutoff against 500 Hz is 0.001 and can. Bring the rate down
// first, as the guide of this area says.
#define IIR_MIN_CUTOFF      0.001f

// True if a design can hold the given cutoff, which is a part of the sample
// rate. Ask this before a design when the cutoff comes from a measurement or
// from a setting, because a design that cannot hold its cutoff gives back a
// filter that looks right and is not.
bool iir_is_valid_cutoff(float cutoff);

// Give a filter with the given number of sections. The memory comes from the
// heap. The filter lets everything pass until a design function or
// iir_set_section gives it coefficients. Give the filter to iir_free when you
// no longer need it.
iir_t iir_alloc(uint32_t sections);

// Give a filter that uses the memory that the caller holds. The list
// coefficient must hold IIR_COEFFICIENT_SIZE(sections) float values, and the
// list state must hold IIR_STATE_SIZE(sections) of them. This function takes
// no memory from the heap.
iir_t iir_static_alloc(uint32_t sections, float* coefficient, float* state);

// Build the coefficients of a filter of Butterworth that lets the low
// frequencies pass. The order of the filter is two times the number of
// sections.
// Give false and leave the filter as it was if the cutoff is outside
// IIR_MIN_CUTOFF to 0.5.
bool iir_design_low_pass(iir_t* iir, float cutoff);

// Build the coefficients of a filter of Butterworth that lets the high
// frequencies pass.
// Give false and leave the filter as it was if the cutoff is outside
// IIR_MIN_CUTOFF to 0.5.
bool iir_design_high_pass(iir_t* iir, float cutoff);

// Build a filter that passes the band between the two cutoffs.
//
// The design is a high pass at the low edge followed by a low pass at the high
// edge, sharing the sections of the filter between them. Thus the number of
// sections MUST BE EVEN, and half of them go to each edge.
//
// This suits a band that is wide. For a band that is narrow, say where the
// high edge is under about one and a half times the low edge, the two edges
// reach into each other and the gain in the middle of the band falls below
// one. Take iir_design_peak for a narrow band: it holds its gain at 1 at the
// middle however narrow the band is.
//
// Give false if the number of sections is odd, if either cutoff cannot be
// held, or if the high cutoff is not above the low one.
bool iir_design_band_pass(iir_t* iir, float low_cutoff, float high_cutoff);

// Build a filter that stops the band between the two cutoffs and passes
// everything else.
//
// Each section is a second order stop, standing at the middle of the band with
// a width that the two cutoffs give. Sections beyond the first make the stop
// deeper and narrower, thus ONE SECTION IS USUALLY WHAT IS WANTED.
//
// Give false if either cutoff cannot be held, or if the high cutoff is not
// above the low one.
bool iir_design_band_stop(iir_t* iir, float low_cutoff, float high_cutoff);

// Build a filter that stops one frequency and passes everything else.
//
// This is the answer to the hum of the mains, which is the most common single
// unwanted frequency there is. Give the frequency of the hum as a part of the
// sample rate, and give how narrow the stop must be as the quality.
//
// The quality is the frequency divided by the width of the stop. A quality of
// 30 at a hum of 50 Hz stops a band about 1.7 Hz wide, which takes the hum out
// and leaves the signal on both sides of it. A higher quality is narrower.
//
// A NARROW STOP IS NOT ALWAYS BETTER. A stop that is very narrow rings: it
// answers a step with a tone at its own frequency that dies away slowly, and
// that tone can look like a signal. It also needs the hum to stand still,
// which the mains does not always do. A quality between 10 and 50 suits most
// work.
//
// Sections beyond the first make the stop deeper and narrower. One is usually
// what is wanted.
//
// Give false if the frequency cannot be held or if the quality is not above
// zero.
bool iir_design_notch(iir_t* iir, float centre, float quality);

// Build a filter that passes one frequency and stops everything else.
//
// This is the other side of iir_design_notch, and it takes the same two
// numbers. Its gain is 1 at the middle of the band however narrow the band is,
// thus it suits a band that iir_design_band_pass cannot hold.
//
// Take it to follow one tone: the carrier of a signal, one note, the beat of a
// heart inside a band. Where only the SIZE of one frequency is wanted and not
// the signal itself, the goertzel module costs far less.
//
// Give false if the frequency cannot be held or if the quality is not above
// zero.
bool iir_design_peak(iir_t* iir, float centre, float quality);

// Write the five coefficients of one section. The three coefficients b belong
// to the input, and the two coefficients a belong to the feedback. The
// function divides every coefficient by a0, thus the caller may give the
// coefficients as another program calculated them.
void iir_set_section(iir_t* iir, uint32_t section, float b0, float b1, float b2,
                     float a0, float a1, float a2);

// Give the filtered value of one sample.
float iir_process_sample(iir_t* iir, float sample);

// Filter a block of samples. The input and the output may be the same list.
void iir_process_block(iir_t* iir, const float* input, float* output, uint32_t size);

// Set the state of every section to zero. The filter then behaves as a filter
// that has seen no sample yet.
void iir_reset(iir_t* iir);

// Give the size of the answer of the filter at the given frequency, which is a
// part of the sample rate. A value of 1 says that the frequency passes
// unchanged, and a value of 0 says that the filter stops it.
float iir_get_gain(iir_t* iir, float frequency);

// Release the memory of a filter that came from iir_alloc. This function does
// nothing for a filter that came from iir_static_alloc.
void iir_free(iir_t* iir);

#endif//IIR_H
