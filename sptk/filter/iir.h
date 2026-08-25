#ifndef IIR_H
#define IIR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

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
    real_t* coefficient;         // Five coefficients for each section
    real_t* state;               // Two values for each section
    bool dynamic_alloc;         // True if the memory comes from the heap
}iir_t;

// WHICH SHAPE OF FILTER TO ASK FOR
//
// A filter trades three things against each other: how flat the band that
// passes is, how sharply it falls, and how much of the band that is stopped
// gets through. NO FILTER IS BEST AT ALL THREE, and the shapes here sit at
// different corners of that trade.
//
// Measured, on a low pass of order 8 at a cutoff of a tenth of the sample
// rate, asked for 1 dB of ripple and a stop band 60 dB down:
//
//   shape           at nothing   ripple in the band   falls to 60 dB below
//                                that passes
//   Butterworth        1.000     none                  0.209
//   Chebyshev I        0.891     1.000 dB              0.151
//   Chebyshev II       1.000     none                  0.100
//   Elliptic           0.891     1.000 dB              0.110
//
// And the same trade seen the other way round. To pass everything below 0.1
// and stop everything above 0.15, 60 dB down, with 1 dB of ripple allowed:
//
//   shape           sections needed   order
//   Butterworth            9           18
//   Chebyshev I            5           10
//   Chebyshev II           5           10
//   Elliptic               3            6
//
// A THIRD OF THE SECTIONS FOR THE SAME WORK. That is what ripple in both bands
// buys, and iir_sections_for is how to ask before choosing. Every filter in
// that table was built and measured, and every one really meets what was
// asked.
//
//   TAKE BUTTERWORTH where the band that passes must be flat and there is room
//   for the fall. It is the safe answer and the one to start from.
//   TAKE CHEBYSHEV I where the fall must be sharper and a known ripple in the
//   band that passes can be borne.
//   TAKE CHEBYSHEV II where the band that passes must stay flat but the fall
//   must still be sharp. The ripple goes into the band that is stopped, where
//   it usually matters less.
//   TAKE ELLIPTIC where the two bands stand close together and nothing else
//   will fit. It ripples in both bands and it has the worst phase of the four,
//   and in exchange it needs a third of the sections.
//
// ONE THING TO KNOW ABOUT AN ELLIPTIC FILTER AT 32 BITS. It holds its band
// that is stopped down with a set of notches, and a notch must be placed
// exactly to reach all the way down. At 32 bits the coefficients cannot always
// place them exactly, and the floor between them then sits a little higher
// than was asked. Measured, at a cutoff of 0.05 with 70 dB asked for:
//
//     ripple asked   0.5    1.0    2.0    3.0    5.0  dB
//     32 bits       -70.0  -70.0  -66.9  -69.2  -69.6  dB delivered
//     64 bits       -70.0  -70.0  -70.0  -70.0  -70.0  dB delivered
//
// The shortfall is at most about 3 dB and MORE SECTIONS DO NOT MEND IT,
// because the fault is in placing the notches and not in having too few. Ask
// for a few dB more than is needed at 32 bits, or build at 64. No other shape
// here shows this: a Chebyshev II delivers exactly what is asked at either
// width, all the way to 110 dB.
//
// A WORD ON PHASE, WHICH IS THE PART THAT IS FORGOTTEN. Every shape here moves
// the different frequencies by different times, and the sharper the fall the
// worse that gets. Where the shape of a waveform matters, and not only which
// frequencies it holds, use iir_group_delay to see what the filter will do to
// it, or use filtfilt, which runs the filter both ways and leaves no phase
// shift at all.
typedef enum{
    // The band that passes is as flat as it can be. No ripple anywhere.
    IIR_BUTTERWORTH = 0,

    // Ripples in the band that passes by the amount asked for, and falls
    // faster than Butterworth for the same order.
    IIR_CHEBYSHEV_I,

    // Flat in the band that passes, and ripples in the band that is stopped.
    IIR_CHEBYSHEV_II,

    // Ripples in both bands and falls fastest of all for the order.
    IIR_ELLIPTIC
}iir_shape_t;

// The smallest ripple worth asking for, in decibels.
//
// Below this the shape is Butterworth in all but name, and the arithmetic that
// works out the poles loses its footing.
#ifndef IIR_SMALLEST_RIPPLE
#define IIR_SMALLEST_RIPPLE     REAL_C(0.001)
#endif

// The largest ripple worth asking for in the band that PASSES, in decibels.
//
// A band that passes with 20 dB of ripple in it is not a band that passes.
#ifndef IIR_LARGEST_RIPPLE
#define IIR_LARGEST_RIPPLE      REAL_C(20.0)
#endif

// How far down the band that is STOPPED may be asked to lie, in decibels.
//
// These are a different quantity from the ripple above and belong to a
// different range, which is worth saying because giving one where the other
// belongs is the easy mistake. A stop band 60 dB down is an ordinary ask; a
// band that passes with 60 dB of ripple is nonsense.

//
// WHAT LIMITS THIS IS THE ORDER AND NOT THE WIDTH, which is worth saying
// because the opposite is what one expects. Measured, on a filter of 4
// sections, both widths deliver exactly what is asked from 40 dB to 110 dB.
// Above that the answer falls short at 64 bits exactly as it does at 32,
// because 4 sections cannot fall that far however the arithmetic is done.
//
// Thus: ask iir_sections_for how many sections a deep stop band needs, rather
// than asking a short filter for one and being quietly given less.
// The most sections that iir_sections_for will ask for.
//
// A chain longer than this holds no more precision than a shorter one, because
// each section adds its own rounding to what the one before it gave. Where a
// specification needs more, it wants a different shape or a different sample
// rate, not more sections.
#ifndef IIR_LARGEST_SECTIONS
#define IIR_LARGEST_SECTIONS        32u
#endif

#ifndef IIR_SMALLEST_ATTENUATION
#define IIR_SMALLEST_ATTENUATION    REAL_C(3.0)
#endif

#ifndef IIR_LARGEST_ATTENUATION
#define IIR_LARGEST_ATTENUATION     REAL_C(120.0)
#endif

// How far either side of a frequency the group delay is measured.
//
// Small enough that the phase between the two places is nearly straight, and
// large enough that the difference is not lost in the rounding of two phases.
// The wider number can afford a smaller step and gets a finer answer for it.
#ifndef IIR_GROUP_DELAY_STEP
#if defined(SPTK_REAL_64)
#define IIR_GROUP_DELAY_STEP    REAL_C(0.00001)
#else
#define IIR_GROUP_DELAY_STEP    REAL_C(0.0005)
#endif
#endif

// The smallest cutoff that a design can hold, which follows the width of the
// build.
//
// A section keeps its poles near the circle when the cutoff is low, and how
// near decides how much precision the coefficients need. When the digits run
// out the coefficients round to values that no longer describe the filter that
// was asked for, and the filter gives a wrong answer WITHOUT SAYING SO.
//
// Measured, at the gain that should be 1.0 at zero frequency, with the check
// taken out so that every cutoff could be tried:
//
//     cutoff     0.001   0.0001    1e-5    1e-6    1e-7
//     32 bits    0.996    0.685   0.000   0.000   0.000
//     64 bits    1.000    1.000   1.000   1.000   1.001
//
// Thus the limit is a thousand times lower at 64 bits, and the same filter
// that no 32 bit build can hold is exact there. A high pass at 0.5 Hz against
// 32 kHz is a cutoff of 0.000016: out of reach at 32 bits, and nothing at all
// at 64.
//
// A cutoff below the limit of the build is usually a sign that the sample rate
// is too high for the work. Bring the rate down first, as the guide of this
// area says, or build at 64 bits.
#if defined(SPTK_REAL_64)
#define IIR_MIN_CUTOFF      REAL_C(0.000001)
#else
#define IIR_MIN_CUTOFF      REAL_C(0.001)
#endif

// True if a design can hold the given cutoff, which is a part of the sample
// rate. Ask this before a design when the cutoff comes from a measurement or
// from a setting, because a design that cannot hold its cutoff gives back a
// filter that looks right and is not.
bool iir_is_valid_cutoff(real_t cutoff);

// Give a filter with the given number of sections. The memory comes from the
// heap. The filter lets everything pass until a design function or
// iir_set_section gives it coefficients. Give the filter to iir_free when you
// no longer need it.
iir_t iir_alloc(uint32_t sections);

// Give a filter that uses the memory that the caller holds. The list
// coefficient must hold IIR_COEFFICIENT_SIZE(sections) float values, and the
// list state must hold IIR_STATE_SIZE(sections) of them. This function takes
// no memory from the heap.
iir_t iir_static_alloc(uint32_t sections, real_t* coefficient, real_t* state);

// Build the coefficients of a filter of Butterworth that lets the low
// frequencies pass. The order of the filter is two times the number of
// sections.
// Give false and leave the filter as it was if the cutoff is outside
// IIR_MIN_CUTOFF to 0.5.
bool iir_design_low_pass(iir_t* iir, real_t cutoff);

// Build the coefficients of a filter of Butterworth that lets the high
// frequencies pass.
// Give false and leave the filter as it was if the cutoff is outside
// IIR_MIN_CUTOFF to 0.5.
bool iir_design_high_pass(iir_t* iir, real_t cutoff);

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
bool iir_design_band_pass(iir_t* iir, real_t low_cutoff, real_t high_cutoff);

// Build a filter that stops the band between the two cutoffs and passes
// everything else.
//
// Each section is a second order stop, standing at the middle of the band with
// a width that the two cutoffs give. Sections beyond the first make the stop
// deeper and narrower, thus ONE SECTION IS USUALLY WHAT IS WANTED.
//
// Give false if either cutoff cannot be held, or if the high cutoff is not
// above the low one.
bool iir_design_band_stop(iir_t* iir, real_t low_cutoff, real_t high_cutoff);

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
bool iir_design_notch(iir_t* iir, real_t centre, real_t quality);

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
bool iir_design_peak(iir_t* iir, real_t centre, real_t quality);

// True if the shape is one this module knows.
bool iir_is_valid_shape(iir_shape_t shape);

// True if this much ripple can be asked for in the band that passes, in
// decibels.
bool iir_is_valid_ripple(real_t ripple);

// True if the band that is stopped can be asked to lie this far down, in
// decibels.
//
// Asking for a depth is not the same as getting it: a filter of too few
// sections falls short whatever it was asked. Use iir_sections_for.
bool iir_is_valid_attenuation(real_t attenuation);

// Build a low pass of the given shape.
//
// THE CUTOFF MEANS A DIFFERENT THING FOR DIFFERENT SHAPES, and giving it the
// same number for each will not give three filters that can be compared.
//
//   Butterworth   where the answer has fallen to 0.707, which is 3 dB down
//   Chebyshev I   where the answer leaves the ripple, thus the end of the band
//                 that passes
//   Chebyshev II  where the band that is STOPPED begins, thus the answer is
//                 already all the way down there
//   Elliptic      where the answer leaves the ripple, as with Chebyshev I
//
// It is a part of the sample rate, from IIR_MIN_CUTOFF to 0.5.
//
// pass_ripple is how much the band that passes may ripple, in decibels, and
// Chebyshev I and elliptic read it. stop_ripple is how far down the band that
// is stopped must lie, in decibels, and Chebyshev II and elliptic read it.
// Butterworth reads neither, and gives the same filter as
// iir_design_low_pass.
//
// Give false if the shape is unknown, the cutoff cannot be held, or a ripple
// that the shape reads lies outside IIR_SMALLEST_RIPPLE to IIR_LARGEST_RIPPLE.
bool iir_design_low_pass_with(iir_t* iir, real_t cutoff, iir_shape_t shape,
                              real_t pass_ripple, real_t stop_ripple);

// Build a high pass of the given shape. The arguments read as they do for
// iir_design_low_pass_with.
bool iir_design_high_pass_with(iir_t* iir, real_t cutoff, iir_shape_t shape,
                               real_t pass_ripple, real_t stop_ripple);

// Give how many sections a filter needs to meet a specification.
//
// The two edges are parts of the sample rate. For a low pass the band that
// passes ends at pass_edge and the band that is stopped begins at stop_edge,
// thus stop_edge must be the larger. Measured, to pass below 0.1 and stop
// above 0.15 by 60 dB with 1 dB of ripple, this gives 9 sections for a
// Butterworth, 5 for either Chebyshev and 3 for an elliptic, and a filter
// built to any of those numbers really does meet the specification. pass_ripple is how much ripple the band
// that passes may hold and stop_ripple is how far down the band that is
// stopped must lie, both in decibels.
//
// ASK THIS BEFORE CHOOSING A SHAPE. The same specification wants far fewer
// sections from an elliptic filter than from a Butterworth, and this says how
// many fewer, which is the whole of the trade the header describes.
//
// The answer is rounded up to whole sections, since a section holds two poles.
// Give 0 where the specification cannot be met: when the edges are out of
// order, when a ripple cannot be held, or when it would need more than
// IIR_LARGEST_SECTIONS sections.
uint32_t iir_sections_for(iir_shape_t shape, real_t pass_edge,
                          real_t stop_edge, real_t pass_ripple,
                          real_t stop_ripple);

// Give how far the filter turns the phase at one frequency, in radians.
//
// The frequency is a part of the sample rate. The answer runs from -pi to pi
// and does not carry how many whole turns have gone before it; use
// iir_group_delay to see what the filter does to the shape of a waveform.
real_t iir_phase(iir_t* iir, real_t frequency);

// Give how long the filter holds back the frequencies about this one, in
// samples.
//
// THIS IS THE NUMBER THAT SAYS WHAT A FILTER DOES TO A WAVEFORM. A filter that
// holds every frequency back by the same time moves the waveform along and
// leaves its shape alone. One that holds some frequencies back longer than
// others changes the shape, and a sharp filter does that most of all near its
// cutoff.
//
// The answer is worked out from the phase a little either side of the
// frequency, thus it is an estimate and not an exact derivative. Near the
// cutoff of a sharp filter it changes quickly, and there the estimate is
// coarsest.
real_t iir_group_delay(iir_t* iir, real_t frequency);

// Write the five coefficients of one section. The three coefficients b belong
// to the input, and the two coefficients a belong to the feedback. The
// function divides every coefficient by a0, thus the caller may give the
// coefficients as another program calculated them.
void iir_set_section(iir_t* iir, uint32_t section, real_t b0, real_t b1, real_t b2,
                     real_t a0, real_t a1, real_t a2);

// Give the filtered value of one sample.
real_t iir_process_sample(iir_t* iir, real_t sample);

// Filter a block of samples. The input and the output may be the same list.
void iir_process_block(iir_t* iir, const real_t* input, real_t* output, uint32_t size);

// Set the state of every section to zero. The filter then behaves as a filter
// that has seen no sample yet.
void iir_reset(iir_t* iir);

// Give the size of the answer of the filter at the given frequency, which is a
// part of the sample rate. A value of 1 says that the frequency passes
// unchanged, and a value of 0 says that the filter stops it.
real_t iir_get_gain(iir_t* iir, real_t frequency);

// Release the memory of a filter that came from iir_alloc. This function does
// nothing for a filter that came from iir_static_alloc.
void iir_free(iir_t* iir);

#endif//IIR_H
