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

#include <sptk/core/real.h>
#include <sptk/transform/fft.h>
#include <sptk/linalg/cnum.h>
#include <math.h>
#include <stdio.h>

// 25 samples in a second is enough for a heart rate, and 256 samples then
// cover about 10 seconds. A longer block gives a finer answer but reacts more
// slowly to a change.
#define SAMPLE_RATE     REAL_C(25.0)
#define SIZE            256u

// The band where a heart rate can lie, in hertz.
#define LOWEST_RATE     REAL_C(0.7)
#define HIGHEST_RATE    REAL_C(3.0)

#define PI              REAL_C(3.14159265358979323846)

static real_t signal[SIZE];
static cnum_t spectrum[SIZE];
static real_t magnitude[SIZE];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own sensor.
//
// It stands for a sensor that watches a person whose heart beats 72 times in a
// minute, which is 1.2 hertz. The reading holds three things that a real
// sensor also holds: the pulse itself, a slow drift, and noise.
// ---------------------------------------------------------------------------
static void read_sensor_block(real_t* block, uint32_t size)
{
    const real_t rate = REAL_C(1.2);

    for(uint32_t index = 0; index < size; index++)
    {
        real_t time = (real_t)index / SAMPLE_RATE;

        // The pulse. The second part gives the shape its sharp rise, which a
        // real pulse also has.
        real_t pulse = REAL_SIN(REAL_C(2.0)*PI*rate*time)
                      + (REAL_C(0.3)*REAL_SIN(REAL_C(2.0)*PI*REAL_C(2.0)*rate*time));

        // The sensor moves against the skin, thus the reading drifts.
        real_t drift = REAL_C(0.8) * REAL_SIN(REAL_C(2.0)*PI*REAL_C(0.05)*time);

        // The light of the room and the sensor itself bring noise.
        real_t noise = REAL_C(0.15) * REAL_SIN((real_t)index * REAL_C(12.9898))
                      * REAL_COS((real_t)index * REAL_C(78.233));

        block[index] = REAL_C(2.5) + pulse + drift + noise;
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
    real_t best_magnitude = REAL_C(0.0);

    for(uint32_t bin = 1; bin < (SIZE/2); bin++)
    {
        real_t frequency = fft_bin_frequency(bin, SIZE, SAMPLE_RATE);

        if((frequency >= LOWEST_RATE) && (frequency <= HIGHEST_RATE))
        {
            if(magnitude[bin] > best_magnitude)
            {
                best_magnitude = magnitude[bin];
                best_bin = bin;
            }
        }
    }

    real_t rate_in_hertz = fft_bin_frequency(best_bin, SIZE, SAMPLE_RATE);
    real_t beats_in_a_minute = rate_in_hertz * REAL_C(60.0);

    printf("A pulse sensor at %.0f samples in a second, %u samples, %.1f seconds\n\n",
           SAMPLE_RATE, SIZE, (real_t)SIZE/SAMPLE_RATE);

    printf("%10s %10s %12s\n", "BIN", "RATE [bpm]", "STRENGTH");
    for(uint32_t bin = 1; bin < (SIZE/2); bin++)
    {
        real_t frequency = fft_bin_frequency(bin, SIZE, SAMPLE_RATE);
        if((frequency >= LOWEST_RATE) && (frequency <= HIGHEST_RATE)
           && (magnitude[bin] > (best_magnitude/REAL_C(10.0))))
        {
            printf("%10u %10.1f %12.1f%s\n", bin, frequency*REAL_C(60.0), magnitude[bin],
                   (bin == best_bin) ? "  <- the strongest" : "");
        }
    }

    printf("\nThe heart rate is %.0f beats in a minute.\n", beats_in_a_minute);

    // The drift lies below the band and the noise spreads over every bin, thus
    // neither of them wins. That is why no filter is needed before this step:
    // the band already leaves both of them out.
    printf("\nThe slow drift sits at %.2f hertz, which lies below the band.\n", REAL_C(0.05));
    printf("One bin holds %.3f hertz, thus the answer steps by %.1f beats.\n",
           SAMPLE_RATE/(real_t)SIZE, (SAMPLE_RATE/(real_t)SIZE)*REAL_C(60.0));
    printf("Use a longer block for a finer answer, or count over several blocks.\n");

    fft_free(&fft);

    return 0;
}

#endif
