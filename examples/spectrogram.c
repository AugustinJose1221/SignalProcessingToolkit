// Draw a spectrogram of a tone that rises, and show the trade that fixes it.
//
// One transform of a whole recording says WHICH frequencies it holds and
// nothing at all about WHEN. This example makes a tone that slides from low to
// high across two seconds, which is the plainest possible case of a signal
// whose frequency depends on the time, and draws it.
//
// THE PICTURE IS THE POINT. A rising line across the page is a thing that one
// transform of the whole recording cannot show at all: it would give a broad
// smear covering every frequency the tone ever visited, with no way to tell a
// rising tone from a falling one, or from all those frequencies at once.
//
// THE TRADE CANNOT BE ESCAPED, and the example runs it at two block sizes so
// that it can be seen rather than believed. A short block sees WHEN sharply
// and WHAT frequency coarsely; a long block does the reverse. Their product is
// fixed, thus there is no setting that is good at both and no default that
// suits every question.
//
// TO PORT THIS: replace make_the_chirp with your own samples and the rate with
// your own, then choose the block from the question you are asking. Speech
// changes every 20 ms; a shaft turning slowly wants a block of a second.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_SPECTROGRAM_EXAMPLE)

#include <sptk/core/real.h>
#include <sptk/transform/stft.h>
#include <sptk/transform/spectrogram.h>
#include <sptk/transform/window.h>
#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(2000.0)
#define SAMPLES         4096u
#define LARGEST_BLOCK   256u
#define LARGEST_BINS    ((LARGEST_BLOCK / 2u) + 1u)
#define MOST_FRAMES     (SAMPLES / 8u)

static real_t signal_buffer[SAMPLES];
static cnum_t frames[MOST_FRAMES * LARGEST_BINS];
static real_t values[MOST_FRAMES * LARGEST_BINS];

// A tone that slides from 100 hertz to 800 hertz across the recording.
//
// The angle is carried along rather than worked out afresh at each sample. A
// sliding tone has no single frequency to multiply the sample number by, and
// forming the angle from the sample number directly gives a tone that jumps.
static void make_the_chirp(void)
{
    real_t angle = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        real_t part = (real_t)index / (real_t)SAMPLES;
        real_t frequency = REAL_C(100.0) + (REAL_C(700.0) * part);

        signal_buffer[index] = REAL_SIN(angle);

        angle += (REAL_C(2.0) * REAL_PI * frequency) / RATE;
    }
}

// Turn a reading in decibels into a character, so that the picture can be
// drawn on a terminal that has nothing but characters.
static char mark_for(real_t decibel)
{
    if(decibel > -REAL_C(6.0)) { return '#'; }
    if(decibel > -REAL_C(20.0)) { return '+'; }
    if(decibel > -REAL_C(40.0)) { return '.'; }

    return ' ';
}

static void draw(uint32_t block)
{
    stft_t stft = stft_alloc(block);
    uint32_t hop = block / 2u;
    uint32_t bins = STFT_BIN_COUNT(block);

    stft_design(&stft, hop, WINDOW_HANN, REAL_C(0.0));

    uint32_t count = stft_frame_count(SAMPLES, block, hop);

    stft_forward(&stft, signal_buffer, SAMPLES, frames, count * bins);
    spectrogram_build(&stft, frames, count, SPECTROGRAM_DECIBEL, RATE, values,
                      count * bins);
    spectrogram_against_the_largest(values, count * bins, values);

    printf("\nA block of %u samples.\n", block);
    printf("  It covers %.0f ms and its bins stand %.1f Hz apart.\n",
           (double)((REAL_C(1000.0) * (real_t)block) / RATE),
           (double)(RATE / (real_t)block));

    // The picture is drawn with frequency down the page and time across it,
    // because a terminal is wider than it is tall and a recording is longer
    // than it is deep.
    for(uint32_t bin = bins - 1u; bin >= 1u; bin--)
    {
        real_t frequency = stft_bin_frequency(&stft, bin, RATE);

        // Only the part of the range the tone visits is drawn.
        if((frequency > REAL_C(900.0)) || (frequency < REAL_C(50.0)))
        {
            continue;
        }

        // One bin is drawn for every few, so that the picture fits the page
        // whatever the block.
        if((bin % (bins / 24u + 1u)) != 0u)
        {
            continue;
        }

        printf("  %6.0f Hz |", (double)frequency);

        for(uint32_t frame = 0; frame < count; frame += ((count / 60u) + 1u))
        {
            printf("%c", mark_for(values[(frame * bins) + bin]));
        }

        printf("|\n");
    }

    printf("           +");
    for(uint32_t frame = 0; frame < count; frame += ((count / 60u) + 1u))
    {
        printf("-");
    }
    printf("+ time, %.1f s\n", (double)((real_t)SAMPLES / RATE));

    stft_free(&stft);
}

int main(void)
{
    make_the_chirp();

    printf("A tone sliding from 100 Hz to 800 Hz across %.1f seconds.\n",
           (double)((real_t)SAMPLES / RATE));
    printf("The same recording, drawn at two block sizes.\n");

    // A short block: the rising line is sharp in time and thick in frequency.
    draw(64u);

    // A long block: the line is thin in frequency and smeared in time.
    draw(LARGEST_BLOCK);

    printf("\nThe line rises in both, and neither block draws it thinly in\n"
           "both directions at once. A block of %u samples covers 4 times as\n"
           "long as one of 64 and its bins stand 4 times closer. That product\n"
           "is fixed, and choosing the block is choosing which half of the\n"
           "question to answer well.\n", LARGEST_BLOCK);

    return 0;
}

#endif
