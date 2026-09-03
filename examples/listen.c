// Listen for a smoke alarm through a wall, always, on almost no memory.
//
// A device that must hear an alarm cannot sleep through it, thus it listens at
// every sample of every second of every day. That is the hardest kind of
// listening to pay for, and the usual way of paying is wrong for it.
//
// WHY NOT A TRANSFORM. A transform of 1024 points needs 1024 complex numbers
// to work in, which is eight kilobytes at this width, and it answers once per
// block: a beep that begins just after a block ends waits a whole block to be
// noticed. The device does not want the spectrum. It wants one question
// answered continuously: how much of 3.2 kilohertz is in the last little while?
//
// WHY NOT goertzel EITHER, WHICH IS THE NEAR MISS. It answers that exact
// question and holds three values to do it, which is right. But it answers
// ONCE PER BLOCK and must be reset between blocks, thus it has the same
// waiting as the transform. Between the two sits the sliding transform, which
// answers at EVERY sample and holds one running total per frequency.
//
// WHAT IT WATCHES AND WHY THREE BINS AND NOT ONE. An alarm is not a pure tone
// and no crystal is exact: the beep drifts a little, and a wall moves it about
// as well. Watching the bin either side as well means the beep is heard
// wherever in that little range it really lands. The three are added together,
// which also steadies the answer.
//
// THE PATTERN IS THE OTHER HALF OF THE ANSWER. A tone alone is not an alarm:
// a kettle, a motor and a phone all make tones. The standard for these alarms
// asks for three beeps and then a pause, and it is the PATTERN that tells an
// alarm from a kettle. The example follows the tone in and out and counts the
// beeps.
//
// TO PORT THIS: replace read_sample with a read from your own microphone. Set
// RATE to its rate and ALARM_HZ to the tone you are listening for.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_LISTEN_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/slide.h>

#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(16000.0)
#define WINDOW          256u
#define ALARM_HZ        REAL_C(3200.0)
#define WATCHERS        3u
#define SECONDS         4u
#define SAMPLES         (((uint32_t)RATE) * SECONDS)

// The alarm sounds three beeps of a fifth of a second, a fifth apart, and then
// waits. What is read below plays that pattern twice with a kettle whistling
// through the middle of it.
#define BEEP_SAMPLES    3200u
#define GAP_SAMPLES     3200u
#define PI              REAL_C(3.14159265358979323846)

// TWO THRESHOLDS AND NOT ONE, AND THIS IS NOT A DETAIL.
//
// The tone is called present above the first and absent below the SECOND,
// which stands lower. With one threshold the count came out at twice the
// truth: the level wobbles as the beep sounds, dips below the line once in the
// middle of each beep, and the crossing back up counts as a second beep. A
// wobble of a fraction of a decibel is enough for that, and no amount of
// choosing the one number better mends it.
//
// The gap between the two is the memory that stops the chatter. Read both
// against the loudest and quietest the example prints, and not in the
// abstract.
#define PRESENT_ABOVE   REAL_C(25.0)
#define ABSENT_BELOW    REAL_C(12.0)

static real_t window_memory[SLIDE_WINDOW_COUNT(WINDOW)];
static cnum_t total_memory[SLIDE_BIN_COUNT(WATCHERS)];
static cnum_t turn_memory[SLIDE_TURN_COUNT(WATCHERS)];

// True while the alarm is really sounding, so that the example can say whether
// it was heard. A device has no such thing.
static bool alarm_is_sounding(uint32_t index)
{
    uint32_t within = index % (uint32_t)(RATE * REAL_C(2.0));
    uint32_t beep = within / (BEEP_SAMPLES + GAP_SAMPLES);

    if(beep >= 3u)
    {
        return false;
    }

    return (within % (BEEP_SAMPLES + GAP_SAMPLES)) < BEEP_SAMPLES;
}

// ---------------------------------------------------------------------------
// Replace this function with a read from your own microphone.
//
// It stands for a room with a kettle whistling at 2100 hertz throughout, room
// noise, and an alarm through a wall that sounds its three beeps twice.
// ---------------------------------------------------------------------------
static real_t read_sample(uint32_t index)
{
    real_t time = (real_t)index / RATE;

    real_t room = REAL_C(0.30) * REAL_SIN((real_t)index * REAL_C(12.9898))
                  * REAL_COS((real_t)index * REAL_C(78.233));

    real_t kettle = REAL_C(0.45)
                    * REAL_SIN(REAL_C(2.0) * PI * REAL_C(2100.0) * time);

    real_t alarm = REAL_C(0.0);

    if(alarm_is_sounding(index))
    {
        // Through a wall it arrives quieter than the kettle in the room.
        alarm = REAL_C(0.25)
                * REAL_SIN(REAL_C(2.0) * PI * ALARM_HZ * time);
    }

    return room + kettle + alarm;
}

int main(void)
{
    slide_t listener = slide_static_alloc(WINDOW, WATCHERS, window_memory,
                                          total_memory, turn_memory);

    // The bin the alarm falls on, and the one either side of it.
    uint32_t middle = (uint32_t)((ALARM_HZ * (real_t)WINDOW) / RATE);

    for(uint32_t which = 0; which < WATCHERS; which++)
    {
        (void)slide_watch(&listener, which, (middle - 1u) + which);
    }

    printf("Listening for a %.0f Hz alarm at %.0f samples a second.\n",
           (double)ALARM_HZ, (double)RATE);
    printf("Watching bins %u, %u and %u, which stand at %.0f, %.0f and %.0f Hz.\n",
           middle - 1u, middle, middle + 1u,
           (double)slide_bin_frequency(&listener, middle - 1u, RATE),
           (double)slide_bin_frequency(&listener, middle, RATE),
           (double)slide_bin_frequency(&listener, middle + 1u, RATE));

    printf("\nWHAT IT HOLDS: %u values for the window and %u complex numbers\n",
           (unsigned)SLIDE_WINDOW_COUNT(WINDOW),
           (unsigned)(SLIDE_BIN_COUNT(WATCHERS) + SLIDE_TURN_COUNT(WATCHERS)));
    printf("for the watchers, which is %u bytes in all.\n",
           (unsigned)((SLIDE_WINDOW_COUNT(WINDOW) * sizeof(real_t))
                      + ((SLIDE_BIN_COUNT(WATCHERS)
                          + SLIDE_TURN_COUNT(WATCHERS)) * sizeof(cnum_t))));
    printf("A transform of %u points would need %u bytes to work in, and\n",
           WINDOW * 4u, (unsigned)(WINDOW * 4u * sizeof(cnum_t)));
    printf("would answer once every %u samples instead of at every one.\n\n",
           WINDOW * 4u);

    bool was_present = false;
    uint32_t beeps = 0;
    uint32_t missed = 0;
    uint32_t false_alarms = 0;
    real_t quietest = REAL_C(1e30);
    real_t loudest = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        slide_process_sample(&listener, read_sample(index));

        if(!slide_is_full(&listener))
        {
            continue;
        }

        real_t heard = REAL_C(0.0);

        for(uint32_t which = 0; which < WATCHERS; which++)
        {
            heard += slide_magnitude(&listener, which);
        }

        bool sounding = alarm_is_sounding(index);

        // The tone must climb past the upper line to be called present, and
        // fall past the lower one to be called absent. Between the two it
        // stays as it was.
        bool present = was_present;

        if(heard > PRESENT_ABOVE)
        {
            present = true;
        }
        else if(heard < ABSENT_BELOW)
        {
            present = false;
        }

        if(sounding && (heard > loudest))
        {
            loudest = heard;
        }
        if(!sounding && (heard < quietest))
        {
            quietest = heard;
        }

        // A beep is counted where the tone arrives, not while it lasts.
        if(present && !was_present)
        {
            beeps++;
        }

        if(sounding && !present)
        {
            missed++;
        }
        if(!sounding && present)
        {
            false_alarms++;
        }

        was_present = present;
    }

    printf("Over %u seconds the alarm sounded 6 beeps.\n", SECONDS);
    printf("  the watcher counted             %u\n", beeps);
    printf("  loudest while it sounded        %.1f\n", (double)loudest);
    printf("  quietest while it did not       %.1f\n", (double)quietest);
    printf("  present above                   %.1f\n", (double)PRESENT_ABOVE);
    printf("  absent below                    %.1f\n", (double)ABSENT_BELOW);
    printf("  samples heard wrong             %u of %u\n",
           missed + false_alarms, SAMPLES);

    printf("\nTHOSE WRONG SAMPLES ARE THE EDGES AND NOTHING ELSE. The window\n");
    printf("holds %u samples, thus when a beep begins the watcher is still\n",
           WINDOW);
    printf("looking mostly at the silence before it, and it takes a window to\n");
    printf("catch up. There are twelve edges in all, six beginnings and six\n");
    printf("endings, and %u samples across them is about %.1f of a window\n",
           missed + false_alarms,
           (double)(missed + false_alarms) / (double)(WINDOW * 12u));
    printf("each. A shorter window would answer faster and would tell the\n");
    printf("alarm from the kettle less well. That is the one trade here.\n");

    printf("\nThe kettle whistles at 2100 Hz throughout and never once trips\n");
    printf("the watcher, because the watcher was never asked about 2100 Hz.\n");
    printf("That is the whole economy of the thing: a device that wants three\n");
    printf("frequencies pays for three and not for the other 125.\n");

    return 0;
}

#endif//RUN_EXAMPLE
