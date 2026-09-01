#ifndef TEST
#include <ffitt/transform/stft.h>
#include <ffitt/core/defs.h>
#else
#include "stft.h"
#include "defs.h"
#endif

#include <math.h>
#include <stdlib.h>

bool stft_is_valid_block(uint32_t block)
{
    return fft_is_valid_size(block);
}

bool stft_is_valid_hop(uint32_t block, uint32_t hop)
{
    return stft_is_valid_block(block) && (hop >= 1u) && (hop <= block);
}

uint32_t stft_frame_count(uint32_t size, uint32_t block, uint32_t hop)
{
    if(!stft_is_valid_hop(block, hop) || (size < block))
    {
        return 0u;
    }

    // Only blocks that are filled whole are taken.
    return ((size - block) / hop) + 1u;
}

uint32_t stft_fewest_frames(uint32_t block, uint32_t hop)
{
    if(!stft_is_valid_hop(block, hop))
    {
        return 0u;
    }

    // The block divided by the hop, rounded up. Written as a division that
    // rounds up rather than with any arithmetic that might overflow.
    return ((block + hop) - 1u) / hop;
}

uint32_t stft_signal_size(uint32_t frames, uint32_t block, uint32_t hop)
{
    if(!stft_is_valid_hop(block, hop) || (frames == 0u))
    {
        return 0u;
    }

    return ((frames - 1u) * hop) + block;
}

stft_t stft_alloc(uint32_t block)
{
    stft_t stft;

    stft.block = 0u;
    stft.hop = 0u;
    stft.kind = WINDOW_HANN;
    stft.parameter = REAL_C(0.0);
    stft.window = NULL;
    stft.windowed = NULL;
    stft.spectrum = NULL;
    stft.designed = false;
    stft.dynamic_alloc = true;

    if(!stft_is_valid_block(block))
    {
        stft.fft = fft_alloc(2);
        fft_free(&stft.fft);
        return stft;
    }

    stft.fft = fft_alloc(block);
    stft.window = (real_t*)malloc(sizeof(real_t) * (size_t)block);
    stft.windowed = (real_t*)malloc(sizeof(real_t) * (size_t)block);
    stft.spectrum = (cnum_t*)malloc(sizeof(cnum_t) * (size_t)block);

    if((stft.fft.size == 0u) || (stft.window == NULL)
       || (stft.windowed == NULL) || (stft.spectrum == NULL))
    {
        stft_free(&stft);
        return stft;
    }

    stft.block = block;

    return stft;
}

stft_t stft_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                         cnum_t* spectrum, fft_t fft)
{
    ASSERT(window != NULL);
    ASSERT(windowed != NULL);
    ASSERT(spectrum != NULL);

    stft_t stft;

    stft.block = stft_is_valid_block(block) ? block : 0u;
    stft.hop = 0u;
    stft.kind = WINDOW_HANN;
    stft.parameter = REAL_C(0.0);
    stft.window = window;
    stft.windowed = windowed;
    stft.spectrum = spectrum;
    stft.fft = fft;
    stft.designed = false;
    stft.dynamic_alloc = false;

    return stft;
}

bool stft_design(stft_t* stft, uint32_t hop, window_kind_t kind,
                 real_t parameter)
{
    ASSERT(stft != NULL);

    if((stft->block == 0u) || !stft_is_valid_hop(stft->block, hop)
       || !window_is_valid_kind(kind))
    {
        return false;
    }

    stft->hop = hop;
    stft->kind = kind;
    stft->parameter = parameter;

    window_build_with(stft->window, stft->block, kind, parameter);

    stft->designed = true;

    return true;
}

// Give the smallest and the largest weight that the windows lay on a sample,
// looking at one hop of the signal, which is where the pattern repeats.
//
// Every sample of a long signal falls at some place inside a hop, and the
// weight it carries depends on that place only. Thus one hop holds every case
// there is and there is no need to walk a whole signal.
static void stft_weights_of_one_hop(const stft_t* stft, real_t* smallest,
                                    real_t* largest)
{
    uint32_t block = stft->block;
    uint32_t hop = stft->hop;

    *smallest = REAL_LARGEST;
    *largest = REAL_C(0.0);

    for(uint32_t place = 0; place < hop; place++)
    {
        real_t total = REAL_C(0.0);

        // The blocks that reach this place are the ones starting a whole
        // number of hops before it.
        for(uint32_t start = place; start < block; start += hop)
        {
            // The synthesis lays the window a second time, thus the weight is
            // the square.
            total += stft->window[start] * stft->window[start];
        }

        if(total < *smallest) { *smallest = total; }
        if(total > *largest) { *largest = total; }
    }
}

bool stft_can_rebuild(const stft_t* stft)
{
    ASSERT(stft != NULL);

    if(!stft->designed)
    {
        return false;
    }

    real_t smallest;
    real_t largest;

    stft_weights_of_one_hop(stft, &smallest, &largest);

    // A sample that carries almost no weight is almost not there. Dividing it
    // back up lifts whatever rounding it holds by the same amount, thus a
    // weight far below the largest is refused rather than answered.
    return smallest > (largest * STFT_SMALLEST_WEIGHT_PART);
}

bool stft_solid_range(const stft_t* stft, uint32_t frame_count,
                      uint32_t* first, uint32_t* count)
{
    ASSERT(stft != NULL);
    ASSERT(first != NULL);
    ASSERT(count != NULL);

    *first = 0u;
    *count = 0u;

    if(!stft->designed || (frame_count == 0u))
    {
        return false;
    }

    uint32_t block = stft->block;
    uint32_t hop = stft->hop;

    // A sample is covered fully when every block that would reach it in a
    // signal without ends really exists. The blocks that reach it start at the
    // multiples of the hop within one block below it, thus the first such
    // sample is one block less one hop from the start, and the last is one hop
    // short of the end of the last block.
    uint32_t start = block - hop;
    uint32_t past = frame_count * hop;

    if(past <= start)
    {
        return false;
    }

    *first = start;
    *count = past - start;

    return true;
}

bool stft_forward(stft_t* stft, const real_t* signal, uint32_t size,
                  cnum_t* output, uint32_t room)
{
    ASSERT(stft != NULL);
    ASSERT(signal != NULL);
    ASSERT(output != NULL);

    if(!stft->designed)
    {
        return false;
    }

    uint32_t block = stft->block;
    uint32_t bins = STFT_BIN_COUNT(block);
    uint32_t frames = stft_frame_count(size, block, stft->hop);

    if((frames == 0u) || (room < (frames * bins)))
    {
        return false;
    }

    for(uint32_t frame = 0; frame < frames; frame++)
    {
        const real_t* start = &signal[frame * stft->hop];

        window_apply(stft->window, start, stft->windowed, block);
        fft_forward_real(&stft->fft, stft->windowed, stft->spectrum);

        // Only the bins up to half the block and one more are kept. The rest
        // are their mirror and say nothing new.
        for(uint32_t bin = 0; bin < bins; bin++)
        {
            output[(frame * bins) + bin] = stft->spectrum[bin];
        }
    }

    return true;
}

bool stft_inverse(stft_t* stft, const cnum_t* frames, uint32_t frame_count,
                  real_t* output, uint32_t room, real_t* weight)
{
    ASSERT(stft != NULL);
    ASSERT(frames != NULL);
    ASSERT(output != NULL);
    ASSERT(weight != NULL);

    if(!stft->designed || !stft_can_rebuild(stft) || (frame_count == 0u))
    {
        return false;
    }

    uint32_t block = stft->block;
    uint32_t bins = STFT_BIN_COUNT(block);
    uint32_t size = stft_signal_size(frame_count, block, stft->hop);

    if(room < size)
    {
        return false;
    }

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = REAL_C(0.0);
        weight[index] = REAL_C(0.0);
    }

    for(uint32_t frame = 0; frame < frame_count; frame++)
    {
        fft_inverse_real(&stft->fft, &frames[frame * bins], stft->windowed,
                         stft->spectrum);

        uint32_t start = frame * stft->hop;

        // The window is laid a second time here. That is what keeps the joins
        // from showing where the frames have been changed in between, and it
        // is why the weight below is the SQUARE of the window.
        for(uint32_t index = 0; index < block; index++)
        {
            output[start + index] += stft->windowed[index]
                                     * stft->window[index];
            weight[start + index] += stft->window[index] * stft->window[index];
        }
    }

    // Inside the solid stretch every sample was covered by as many blocks as
    // fit across it, thus dividing by the weight they laid gives the sample
    // back exactly.
    uint32_t first;
    uint32_t count;

    if(!stft_solid_range(stft, frame_count, &first, &count))
    {
        return false;
    }

    for(uint32_t index = first; index < (first + count); index++)
    {
        output[index] /= weight[index];
    }

    // Outside it the cover is short: the sample at the very start is under the
    // first block only, and a window that is zero at its first sample has
    // taken it away for good. Such a sample is set to nothing rather than left
    // as a number that looks like an answer.
    for(uint32_t index = 0; index < first; index++)
    {
        output[index] = REAL_C(0.0);
    }

    for(uint32_t index = first + count; index < size; index++)
    {
        output[index] = REAL_C(0.0);
    }

    return true;
}

real_t stft_bin_frequency(const stft_t* stft, uint32_t bin,
                          real_t sample_rate)
{
    ASSERT(stft != NULL);
    ASSERT(stft->block != 0u);

    return ((real_t)bin * sample_rate) / (real_t)stft->block;
}

real_t stft_frame_time(const stft_t* stft, uint32_t frame, real_t sample_rate)
{
    ASSERT(stft != NULL);
    ASSERT(stft->block != 0u);
    ASSERT(sample_rate > REAL_C(0.0));

    // The middle of the block, because a window weighs the middle most heavily
    // and that is where the answer of the frame really sits.
    real_t middle = (real_t)(frame * stft->hop)
                    + (((real_t)stft->block - REAL_C(1.0)) / REAL_C(2.0));

    return middle / sample_rate;
}

void stft_free(stft_t* stft)
{
    ASSERT(stft != NULL);

    fft_free(&stft->fft);

    if(stft->dynamic_alloc)
    {
        free(stft->window);
        free(stft->windowed);
        free(stft->spectrum);
    }

    stft->window = NULL;
    stft->windowed = NULL;
    stft->spectrum = NULL;
    stft->block = 0u;
    stft->designed = false;
}
