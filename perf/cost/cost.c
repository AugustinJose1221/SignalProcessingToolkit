#define _POSIX_C_SOURCE 199309L

#include <perf/cost/cost.h>

#include <math.h>
#include <stdlib.h>
#include <time.h>

volatile real_t cost_sink = REAL_C(0.0);

int claims_tested = 0;
int claims_held = 0;
int claims_broken = 0;

double cost_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (double)now.tv_sec + ((double)now.tv_nsec / 1000000000.0);
}

real_t cost_random_value(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

// Write one line and count what it says. The line carries the number that was
// measured and the number the claim asks for, so that a claim that breaks says
// by how much and not only that it broke.
static void cost_report(const char* module, const char* claim, bool held,
                        const char* relation, double measured, double allowed)
{
    claims_tested++;

    // A RATIO THAT IS NOT A NUMBER IS A BROKEN MEASUREMENT AND NEVER A HELD
    // CLAIM. A measured time of zero makes the ratio either not-a-number or
    // endless, and endless is larger than anything a claim asks for. Thus a
    // claim that measured nothing at all reported itself as held. It happened:
    // the clock was read into a real_t, every time came out as zero, and one
    // of the twelve claims below said HELD on the strength of it.
    if(!isfinite(measured))
    {
        held = false;
    }

    if(held)
    {
        claims_held++;
    }
    else
    {
        claims_broken++;
    }

    printf("%-10s %-46s %8.2f %s %-8.2f  %s\n",
           module, claim, measured, relation, allowed,
           held ? "HELD" : "BROKEN");
}

void cost_claim_at_least(const char* module, const char* claim,
                         double measured, double least_allowed)
{
    cost_report(module, claim, measured >= least_allowed, ">=", measured,
                least_allowed);
}

void cost_claim_at_most(const char* module, const char* claim,
                        double measured, double largest_allowed)
{
    cost_report(module, claim, measured <= largest_allowed, "<=", measured,
                largest_allowed);
}

void cost_report_header(void)
{
    printf("%-10s %-46s %8s %2s %-8s  %s\n",
           "MODULE", "WHAT THE HEADER CLAIMS", "MEASURED", "", "ASKED", "");
    printf("--------------------------------------------------------"
           "-------------------------------------\n");
}

void cost_report_summary(void)
{
    printf("--------------------------------------------------------"
           "-------------------------------------\n");
    printf("Total Claims Tested: %d\n", claims_tested);
    printf("Total Claims Held: %d\n", claims_held);
    printf("Total Claims Broken: %d\n", claims_broken);
    printf("--------------------------------------------------------"
           "-------------------------------------\n");
}
