// Send a day of readings over a radio that costs battery by the byte.
//
// A logger in a field measures temperature every minute and sends the day home
// over a low power radio. The radio is by far the largest drain on the board:
// the processor sleeps between readings, but every byte sent is current out of
// the cell. Halving the bytes very nearly doubles the months the logger runs.
//
// WHY A COSINE TRANSFORM AND NOT THE FOURIER ONE. A day of temperature is
// smooth: it rises, it falls, it does not jump. A smooth signal packs almost
// all of its energy into a handful of low cosines, thus a few numbers describe
// the whole day. The Fourier transform would do the same work and hand back
// COMPLEX numbers, twice the values to send for a signal that has no phase
// worth sending. The cosine transform gives real numbers only.
//
// HOW MANY TO SEND IS A MEASURED CHOICE AND NOT A GUESS. dct_count_for_share
// answers it: give it the share of the energy to keep and it says how many
// coefficients hold that much. The logger sends that many and the receiver
// rebuilds the rest as zero.
//
// WHAT IS LOST IS REAL AND THE EXAMPLE MEASURES IT. Throwing coefficients away
// is not free. The rebuilt day is not the day that was measured, and the
// example prints how far each is off in the unit the sensor reads in, so that
// the choice is made against a number and not a feeling.
//
// TO PORT THIS: replace fill_day with a read from your own logger. The size
// must be one dct_is_valid_size accepts.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_DCT_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/transform/dct.h>
#include <ffitt/util/stats.h>

#include <math.h>
#include <stdio.h>

#define SIZE            256u
#define PI              REAL_C(3.14159265358979323846)
#define SHARE_COUNT     4u

static const real_t SHARE[SHARE_COUNT] = {
    REAL_C(0.90), REAL_C(0.99), REAL_C(0.999), REAL_C(0.99999)
};

static real_t day[SIZE];
static real_t cosines[SIZE];
static real_t kept[SIZE];
static real_t rebuilt[SIZE];

// ---------------------------------------------------------------------------
// Replace this function with a read from your own logger.
//
// It stands for a day of temperature: cold before dawn, a peak in the middle
// of the afternoon, a warm evening, and the small movement of cloud and wind
// on top of it.
// ---------------------------------------------------------------------------
static void fill_day(real_t* block)
{
    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t part = (real_t)index / (real_t)SIZE;

        block[index] = REAL_C(12.0)
                       - (REAL_C(8.0) * REAL_COS(REAL_C(2.0) * PI * part))
                       - (REAL_C(2.0) * REAL_COS(REAL_C(4.0) * PI * part))
                       + (REAL_C(0.8) * REAL_COS(REAL_C(6.0) * PI * part))
                       + (REAL_C(0.04) * REAL_SIN((real_t)index * REAL_C(2.399)));
    }
}

// How far the rebuilt day stands from the measured one, at its worst point.
static real_t worst_error(const real_t* truth, const real_t* got)
{
    real_t worst = REAL_C(0.0);

    for(uint32_t index = 0; index < SIZE; index++)
    {
        real_t error = REAL_ABS(truth[index] - got[index]);

        if(error > worst)
        {
            worst = error;
        }
    }

    return worst;
}

int main(void)
{
    if(!dct_is_valid_size(SIZE))
    {
        printf("The transform cannot take a block of %u.\n", SIZE);
        return 1;
    }

    fill_day(day);

    if(!dct_forward(day, cosines, SIZE))
    {
        printf("The transform refused the block.\n");
        return 1;
    }

    printf("A day of temperature, %u readings, one every few minutes.\n", SIZE);
    printf("Sent as they stand that is %u values.\n\n", SIZE);

    printf("%11s %8s %10s %14s %14s\n",
           "KEEP", "VALUES", "SAVED", "WORST ERROR", "MEAN ERROR");
    printf("------------------------------------------------------------\n");

    for(uint32_t which = 0; which < SHARE_COUNT; which++)
    {
        uint32_t count = dct_count_for_share(cosines, SIZE, SHARE[which]);

        // The logger sends `count` values. The receiver has the rest as zero.
        for(uint32_t index = 0; index < SIZE; index++)
        {
            kept[index] = (index < count) ? cosines[index] : REAL_C(0.0);
        }

        (void)dct_inverse(kept, rebuilt, SIZE);

        real_t worst = worst_error(day, rebuilt);
        real_t mean = REAL_C(0.0);

        for(uint32_t index = 0; index < SIZE; index++)
        {
            mean += REAL_ABS(day[index] - rebuilt[index]);
        }
        mean /= (real_t)SIZE;

        printf("%9.3f%% %8u %9.1fx %13.3f %14.3f\n",
               (double)(SHARE[which] * REAL_C(100.0)), count,
               (double)((real_t)SIZE / (real_t)count),
               (double)worst, (double)mean);
    }

    printf("\nREAD THE WORST COLUMN AND NOT THE MEAN ONE. A mean error hides the\n");
    printf("one hour that went wrong, and that hour is usually the interesting\n");
    printf("one.\n\n");
    printf("The sensor reads to about a tenth of a degree, thus a worst error\n");
    printf("below that is a day the receiver cannot tell from the one measured.\n");
    printf("A handful of values reaches it, and the radio sends a fraction of\n");
    printf("what it did.\n\n");
    printf("THE ERROR THEN STOPS FALLING, at about the size of the sensor's own\n");
    printf("noise. That is not the transform giving up: noise is not smooth and\n");
    printf("no small number of cosines describes it. Sending more values past\n");
    printf("that point spends battery to carry noise home.\n");

    return 0;
}

#endif//RUN_EXAMPLE
