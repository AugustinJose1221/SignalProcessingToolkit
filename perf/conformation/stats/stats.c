#include <perf/conformation/support.h>
#include <perf/conformation/stats/stats.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <gsl/gsl_statistics_double.h>
#include <gsl/gsl_sort.h>

// The readings of this library set against those of the GNU Scientific
// Library.
//
// THE TWO DIVIDE A VARIANCE BY DIFFERENT NUMBERS, and that is not a fault in
// either. This library gives the variance of the list AS IT STANDS, divided by
// the number of samples; the header says so, and measured on 1, 2, 3, 4 it
// gives 1.25. The other library divides by one less than the number, which
// estimates the variance of the thing the list was drawn from, and gives
// 1.667 for the same four numbers.
//
// The comparison therefore asks the other library for the form with the mean
// given to it, which divides by the number of samples as this one does. Asking
// for the plain form would fail every time and say nothing.

#define MOST_SAMPLES    256u

static real_t random_between(real_t min, real_t max)
{
    return min + (((real_t)rand() / (real_t)RAND_MAX) * (max - min));
}

static bool readings_check(uint32_t size)
{
    real_t* mine = (real_t*)malloc(sizeof(real_t) * size);
    real_t* work = (real_t*)malloc(sizeof(real_t) * size);
    double* theirs = (double*)malloc(sizeof(double) * size);
    double* sorted = (double*)malloc(sizeof(double) * size);
    bool flag = true;

    for(uint32_t k = 0; k < size; k++)
    {
        real_t value = random_between(REAL_C(-8.0), REAL_C(8.0));

        mine[k] = value;
        theirs[k] = (double)value;
        sorted[k] = (double)value;
    }

    double their_mean = gsl_stats_mean(theirs, 1, size);

    if(!CONFORMATION_IS_NEAR(stats_mean(mine, size), their_mean))
    {
        flag = false;
    }

    if(flag && !CONFORMATION_IS_NEAR(
           stats_variance(mine, size),
           gsl_stats_variance_with_fixed_mean(theirs, 1, size, their_mean)))
    {
        flag = false;
    }

    if(flag && !CONFORMATION_IS_NEAR(
           stats_deviation(mine, size),
           gsl_stats_sd_with_fixed_mean(theirs, 1, size, their_mean)))
    {
        flag = false;
    }

    if(flag && !CONFORMATION_IS_NEAR(stats_min(mine, size),
                                     gsl_stats_min(theirs, 1, size)))
    {
        flag = false;
    }

    if(flag && !CONFORMATION_IS_NEAR(stats_max(mine, size),
                                     gsl_stats_max(theirs, 1, size)))
    {
        flag = false;
    }

    // The median of this library takes the list as it stands and puts it in
    // order itself. The other library wants a list that is already in order,
    // thus a copy is sorted for it. stats_median is given its own copy,
    // because it moves the samples about.
    gsl_sort(sorted, 1, size);

    for(uint32_t k = 0; k < size; k++)
    {
        work[k] = mine[k];
    }

    if(flag && !CONFORMATION_IS_NEAR(
           stats_median(work, size),
           gsl_stats_median_from_sorted_data(sorted, 1, size)))
    {
        flag = false;
    }

    free(mine);
    free(work);
    free(theirs);
    free(sorted);

    return flag;
}

void run_stats_conformation_tests(void)
{
    static const uint32_t sizes[] = {2u, 3u, 8u, 33u, MOST_SAMPLES};

    for(uint32_t k = 0; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
    {
        FLAG_CHECK_TRUE_CASE(readings_check(sizes[k]), "Stats Readings Test");
    }
}
