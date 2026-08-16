// Read a heart rate from a pulse sensor.
//
// A pulse sensor, also called a PPG sensor, shines a light into the skin and
// measures how much comes back. Blood absorbs light, thus the reading rises
// and falls with each heartbeat. The reading also drifts slowly, because the
// sensor moves against the skin, and it holds noise from the light of the
// room.
//
// To read the rate, take a block of samples, look at its spectrum, and find
// the strongest frequency inside the band where a heart rate can lie. That
// band is about 0.7 to 3.0 hertz, which is 42 to 180 beats in a minute.
//
// TO PORT THIS: replace read_sensor_block with a read from your own sensor.
// Everything else stays as it is. Set SAMPLE_RATE to the rate of your sensor
// and SIZE to a power of two that covers about ten seconds.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_FFT_EXAMPLE)

#include <sptk/transform/fft.h>
#include <sptk/linalg/cnum.h>
#include <math.h>
#include <stdio.h>

// 25 samples in a second is enough for a heart rate, and 256 samples then
// cover about 10 seconds. A longer block gives a finer answer but reacts more
// slowly to a change.
#define SAMPLE_RATE     25.0f
#define SIZE            256u

// The band where a heart rate can lie, in hertz.
#define LOWEST_RATE     0.7f
#define HIGHEST_RATE    3.0f

#define PI              3.14159265358979323846f

static float signal[SIZE];
static cnum_t spectrum[SIZE];
static float magnitude[SIZE];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own sensor.
//
// It stands for a sensor that watches a person whose heart beats 72 times in a
// minute, which is 1.2 hertz. The reading holds three things that a real
// sensor also holds: the pulse itself, a slow drift, and noise.
// ---------------------------------------------------------------------------
static void read_sensor_block(float* block, uint32_t size)
{
    const float rate = 1.2f;

    for(uint32_t index = 0; index < size; index++)
    {
        float time = (float)index / SAMPLE_RATE;

        // The pulse. The second part gives the shape its sharp rise, which a
        // real pulse also has.
        float pulse = sinf(2.0f*PI*rate*time)
                      + (0.3f*sinf(2.0f*PI*2.0f*rate*time));

        // The sensor moves against the skin, thus the reading drifts.
        float drift = 0.8f * sinf(2.0f*PI*0.05f*time);

        // The light of the room and the sensor itself bring noise.
        float noise = 0.15f * sinf((float)index * 12.9898f)
                      * cosf((float)index * 78.233f);

        block[index] = 2.5f + pulse + drift + noise;
    }
}

int main(void)
{
    read_sensor_block(signal, SIZE);

    fft_t fft = fft_alloc(SIZE);

    fft_forward_real(&fft, signal, spectrum);
    fft_magnitude(spectrum, magnitude, SIZE);

    // Walk the bins that lie inside the band of a heart rate and keep the
    // strongest one. Only the bins below the middle hold new information: the
    // bins above it mirror them.
    uint32_t best_bin = 0;
    float best_magnitude = 0.0f;

    for(uint32_t bin = 1; bin < (SIZE/2); bin++)
    {
        float frequency = fft_bin_frequency(bin, SIZE, SAMPLE_RATE);

        if((frequency >= LOWEST_RATE) && (frequency <= HIGHEST_RATE))
        {
            if(magnitude[bin] > best_magnitude)
            {
                best_magnitude = magnitude[bin];
                best_bin = bin;
            }
        }
    }

    float rate_in_hertz = fft_bin_frequency(best_bin, SIZE, SAMPLE_RATE);
    float beats_in_a_minute = rate_in_hertz * 60.0f;

    printf("A pulse sensor at %.0f samples in a second, %u samples, %.1f seconds\n\n",
           SAMPLE_RATE, SIZE, (float)SIZE/SAMPLE_RATE);

    printf("%10s %10s %12s\n", "BIN", "RATE [bpm]", "STRENGTH");
    for(uint32_t bin = 1; bin < (SIZE/2); bin++)
    {
        float frequency = fft_bin_frequency(bin, SIZE, SAMPLE_RATE);
        if((frequency >= LOWEST_RATE) && (frequency <= HIGHEST_RATE)
           && (magnitude[bin] > (best_magnitude/10.0f)))
        {
            printf("%10u %10.1f %12.1f%s\n", bin, frequency*60.0f, magnitude[bin],
                   (bin == best_bin) ? "  <- the strongest" : "");
        }
    }

    printf("\nThe heart rate is %.0f beats in a minute.\n", beats_in_a_minute);

    // The drift lies below the band and the noise spreads over every bin, thus
    // neither of them wins. That is why no filter is needed before this step:
    // the band already leaves both of them out.
    printf("\nThe slow drift sits at %.2f hertz, which lies below the band.\n", 0.05f);
    printf("One bin holds %.3f hertz, thus the answer steps by %.1f beats.\n",
           SAMPLE_RATE/(float)SIZE, (SAMPLE_RATE/(float)SIZE)*60.0f);
    printf("Use a longer block for a finer answer, or count over several blocks.\n");

    fft_free(&fft);

    return 0;
}

#endif
