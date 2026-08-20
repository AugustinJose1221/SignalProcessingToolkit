#ifndef MOVAVG_H
#define MOVAVG_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/core/ringbuf.h>
#else
#include "real.h"
#include "ringbuf.h"
#endif

// The mean of the last samples, worked out in a fixed time.
//
// A mean over a window that slides is the most common smoothing there is. It
// is also the one most often written badly.
//
// A filter with a finite impulse response whose coefficients are all the same
// gives the right answer, and many callers reach for that. It costs one
// multiplication and one addition for EACH coefficient, for EACH sample. A
// window of 500 samples then costs 500 operations a sample, and at 32 kHz that
// is 16 million a second for a mean.
//
// It need not cost that. The mean of the new window differs from the mean of
// the old one by exactly two samples: the one that arrived and the one that
// fell off the end. Add the first and take away the second, and the work is
// the same whether the window holds ten samples or ten thousand.
//
// Measured, in nanoseconds for one sample:
//
//   window          4      8     16     64    500   4096
//   this module  13.4   13.4   12.5   12.4   14.1   17.6
//   equal fir     6.0    8.2   14.1   58.9  438.7 3588.4
//
// The cost of this module does not follow the window. The cost of the other
// one does, and at a window of 4096 it is two hundred times as much.
//
// BELOW A WINDOW OF 16 THE PLAIN FILTER IS FASTER, and the table shows it. The
// bookkeeping of this module costs more than four multiplications do. Take the
// fir module for a very short window; take this one from about 16 upwards.
//
// WHAT IT COSTS TO SMOOTH THIS WAY
//
// This filter is not a good low pass. Its answer to a single frequency falls
// to nothing at the rate that fits the window and then rises again, thus a
// tone at the wrong frequency comes through nearly untouched. A filter from
// the fir or the iir module is better where the frequencies matter.
//
// Take this one where the window itself is the point: an energy over the last
// 200 ms, a level over the last second, the moving mean in the middle of a
// detector. Take savgol where the shape of a peak must be kept.
//
// THE THREE MEASURES
//
//   movavg_get_mean       the mean of the window          fixed time
//   movavg_get_rms        the root of the mean of squares fixed time
//   movavg_get_deviation  how far the samples spread      one pass
//
// The mean and the root mean square are held as running totals, thus they cost
// nothing to read. The deviation cannot be held that way without losing its
// accuracy, and the header of that function says why.

typedef struct{
    ringbuf_t window;           // The samples of the window
    real_t total;               // The running sum of the samples
    real_t square_total;        // The running sum of the squares
    uint32_t since_refresh;     // Samples since the totals were built again
}movavg_t;

// How many samples pass before the totals are worked out again from the whole
// window. The reason stands at movavg_process_sample.
#define MOVAVG_REFRESH      4096u

// Give a filter with a window of the given size. The memory comes from the
// heap. Give the filter to movavg_free when you no longer need it.
movavg_t movavg_alloc(uint32_t size);

// Give a filter that uses the memory at data, which must hold as many float
// values as the given size. This function takes no memory from the heap.
movavg_t movavg_static_alloc(uint32_t size, real_t* data);

// Forget every sample and every total.
void movavg_reset(movavg_t* movavg);

// Put one sample in and give the mean of the window as it now stands.
//
// While the window is still filling, the mean is taken over the samples that
// have arrived and not over the whole size. Thus the answer is right from the
// first sample and does not start low.
real_t movavg_process_sample(movavg_t* movavg, real_t sample);

// Filter a whole block. The input and the output may be the same list.
void movavg_process_block(movavg_t* movavg, const real_t* input, real_t* output,
                          uint32_t size);

// Give the mean of the window without putting a sample in.
real_t movavg_get_mean(const movavg_t* movavg);

// Give the root of the mean of the squares of the window.
//
// This follows the level of the signal and not how much it moves. For a signal
// that sits at 100 and wanders by 1 it gives about 100, where the deviation
// gives 1. Take this one for the energy or the power of a signal.
real_t movavg_get_rms(const movavg_t* movavg);

// Give the standard deviation of the window.
//
// THIS ONE READS THE WHOLE WINDOW, thus it costs one pass and not a fixed
// time. The other two are held as running totals and this cannot be, for a
// reason worth knowing:
//
// A deviation from running totals would be the mean of the squares less the
// square of the mean. Those two numbers are nearly equal whenever the signal
// sits far from zero, and their difference is the answer. A reading that sits
// at 8 000 000 and moves by 1 gives two numbers near 64 000 000 000 000 whose
// difference is 1, and that difference is lost.
//
// Reading the window and taking the mean away from each sample first has no
// such trouble. It costs a pass over the window, thus call it when the answer
// is wanted and not for every sample.
real_t movavg_get_deviation(const movavg_t* movavg);

// Give how many samples the window holds now.
uint32_t movavg_count(const movavg_t* movavg);

// True when the window holds as many samples as its size.
bool movavg_is_full(const movavg_t* movavg);

// Release the memory of a filter that came from movavg_alloc. This function
// does nothing for a filter that came from movavg_static_alloc.
void movavg_free(movavg_t* movavg);

#endif//MOVAVG_H
