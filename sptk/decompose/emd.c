#ifndef TEST
#include <sptk/decompose/emd.h>
#include <sptk/core/defs.h>
#include <string.h>
#include <sptk/util/peakdetect.h>
#include <sptk/util/valleydetect.h>
#else
#include "emd.h"
#include "defs.h"
#include <string.h>
#include "peakdetect.h"
#include "valleydetect.h"
#endif

static void emd_update_runtime_params(emd_t* emd, real_t* x, real_t* y);
static real_t emd_get_largest(real_t* data, uint32_t size);
static real_t emd_get_smallest(real_t* data, uint32_t size);
static uint32_t emd_gather_peaks(emd_t* emd, real_t* signal, uint32_t* found);
static uint32_t emd_gather_valleys(emd_t* emd, real_t* signal, uint32_t* found);

emd_t emd_alloc(uint32_t size)
{
    ASSERT(size != 0);

    emd_t emd;

    emd.size = size;
    emd.cspline = cspline_alloc(size);
    emd.cspline_mempool = cspline_alloc_mempool(size);
    emd.peak_buffer = (real_t*)malloc(sizeof(real_t)*size);
    emd.valley_buffer = (real_t*)malloc(sizeof(real_t)*size);
    emd.dynamic_alloc = true;

    return emd;
}

emd_t emd_static_alloc(uint32_t size, real_t** membank, real_t** mempool, real_t* peak_buffer, real_t* valley_buffer)
{
    ASSERT(size != 0);
    ASSERT(membank != NULL);

    emd_t emd;
    emd.size = size;
    emd.cspline = cspline_static_alloc(size, membank);
    emd.cspline_mempool = cspline_static_alloc_mempool(mempool);
    emd.peak_buffer = peak_buffer;
    emd.valley_buffer = valley_buffer;
    emd.dynamic_alloc = false;

    return emd;
}

void emd_initialize(emd_t* emd, uint32_t num_of_imf, imf_t* imf, real_t* x, real_t* y, real_t* residue, real_t* working_buffer, real_t* peak_index_buffer, real_t* valley_index_buffer)
{
    ASSERT(emd != NULL);
    ASSERT(num_of_imf > 0);
    ASSERT(imf != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    emd->imf_count = num_of_imf;
    emd->imf = imf;
    emd->x = x;
    emd->y = y;
    emd->residue = residue;
    emd->working_buffer = working_buffer;
    emd->peak_index_buffer = peak_index_buffer;
    emd->valley_index_buffer = valley_index_buffer;
}

imf_t* emd_get_imf(emd_t* emd, uint32_t imf_index, uint32_t stopping_threshold, uint32_t* status)
{
    ASSERT(emd != NULL);

    uint32_t peakcount = 0;
    uint32_t valleycount = 0;
    uint32_t real_peaks = 0;
    uint32_t real_valleys = 0;
    uint32_t start_index;
    uint32_t shift;
    uint32_t iteration_count = 0;

    real_t interpolation_index = 0;

    *status = 0;

    // A signal with fewer than three samples holds no peak and no valley, thus
    // the decomposition cannot take anything out of it. The buffers hold one
    // element for each sample, and the code below needs room for two more
    // points than the number of peaks. Without this guard the function reads
    // and writes after the end of the buffers.
    if(emd->size < EMD_MINIMUM_SIZE)
    {
        for(uint32_t index = 0; index < emd->size; index++)
        {
            emd->imf[imf_index].x[index] = (real_t)index;
            emd->imf[imf_index].y[index] = REAL_C(0.0);
        }
        return &emd->imf[imf_index];
    }

    start_index = (uint32_t)emd->x[0];
    shift = (uint32_t)(emd->x[1]-emd->x[0]);

    memcpy(emd->working_buffer, emd->residue, sizeof(real_t)*emd->size);

    peakcount = emd_gather_peaks(emd, emd->working_buffer, &real_peaks);
    valleycount = emd_gather_valleys(emd, emd->working_buffer, &real_valleys);

    // A RESIDUE THAT ONLY RISES OR ONLY FALLS HOLDS NO MODE.
    //
    // A mode is an oscillation, and an oscillation must turn. A signal that
    // never turns has nothing left in it to take out, thus the method is
    // finished and says so with a status of 0. What is left belongs to the
    // residue and to nothing else.
    //
    // The two counts above are what the DETECTION found, before the points at
    // the two ends were added. Those end points are there so that the spline
    // covers the whole signal; counting them as turns of the signal would say
    // that every signal turns, and the method would then never finish. It
    // would go on giving modes of zero until it had filled every place the
    // caller offered.
    if((real_peaks == 0u) && (real_valleys == 0u))
    {
        for(uint32_t index = 0; index < emd->size; index++)
        {
            emd->imf[imf_index].x[index] = (real_t)index;
            emd->imf[imf_index].y[index] = REAL_C(0.0);
        }
        return &emd->imf[imf_index];
    }

    while(peakcount > 1 && valleycount > 1 && iteration_count < stopping_threshold)
    {
        cspline_update_size(&emd->cspline, peakcount);
        cspline_init(&emd->cspline, emd->cspline_mempool, emd->peak_index_buffer, emd->peak_buffer);

        for(uint32_t index = 0; index < emd->size; index++)
        {
            interpolation_index = start_index + (index*shift);
            emd->imf[imf_index].x[index] = index;
            emd->imf[imf_index].y[index] = cspline_get_interpolated_point(&emd->cspline, interpolation_index);
        }

        cspline_update_size(&emd->cspline, valleycount);
        cspline_init(&emd->cspline, emd->cspline_mempool, emd->valley_index_buffer, emd->valley_buffer);
        for(uint32_t index = 0; index < emd->size; index++)
        {
            interpolation_index = start_index + (index*shift);
            emd->imf[imf_index].x[index] = index;
            emd->imf[imf_index].y[index] += cspline_get_interpolated_point(&emd->cspline, interpolation_index);
            emd->imf[imf_index].y[index] /=2;
        }

        for(uint32_t index = 0; index < emd->size; index++)
        {
            emd->imf[imf_index].y[index] = emd->working_buffer[index]-emd->imf[imf_index].y[index];
            emd->working_buffer[index] = emd->imf[imf_index].y[index];
        }

        peakcount = emd_gather_peaks(emd, emd->working_buffer, &real_peaks);
        valleycount = emd_gather_valleys(emd, emd->working_buffer,
                                         &real_valleys);

        iteration_count++;
        *status = 1;
    }

    return &emd->imf[imf_index];
}

uint32_t emd_sift(emd_t* emd, uint32_t stopping_threshold)
{
    ASSERT(emd != NULL);
    ASSERT(stopping_threshold > 0);

    imf_t* imf;
    uint32_t imf_count = 0;
    uint32_t status;

    memcpy(emd->residue, emd->y, sizeof(real_t)*emd->size);

    do{
        imf = emd_get_imf(emd, imf_count, stopping_threshold, &status);

        // A status of 0 says the residue held no more mode. The function that
        // came back is then all zeros, thus taking it away would change
        // nothing and counting it would say that a mode was found where none
        // was. Stop here and leave the rest in the residue.
        if(status == 0u)
        {
            break;
        }

        for(uint32_t index = 0; index < emd->size; index++)
        {
            emd->residue[index] = emd->residue[index] - imf->y[index];
        }
        emd_update_runtime_params(emd, emd->x, emd->residue);
        imf_count++;
    }while(imf_count < emd->imf_count);

    return imf_count;
}

void emd_free(emd_t emd)
{
    if(emd.dynamic_alloc)
    {
        cspline_free(emd.cspline);
        cspline_free_mempool(emd.cspline_mempool);
        free(emd.peak_buffer);
        free(emd.valley_buffer);
    }
}

// Find the peaks of the signal and put a point at each end, so that a spline
// through them covers the whole signal and not only the part between the first
// peak and the last.
//
// A SIGNAL THAT ONLY RISES OR ONLY FALLS HOLDS NO PEAK AT ALL. The detection
// then writes nothing into the buffer, and the buffer still holds whatever was
// put there for an earlier signal. Taking the largest sample gives the
// envelope a value that belongs to THIS signal.
//
// That must be done every time the peaks are found and not only the first
// time. The sifting takes the mean of the two envelopes away from the signal
// again and again, and an envelope left standing at the value of an earlier
// round is a constant amount taken away at every step. The mode then grows
// without bound instead of settling: measured, a signal whose largest sample
// was 3 gave a residue of 1.5 million after six modes.
//
// Give how many points the spline is to be drawn through.
static uint32_t emd_gather_peaks(emd_t* emd, real_t* signal, uint32_t* found)
{
    uint32_t count = peakdetect_get_peaks(signal, &emd->peak_index_buffer[1],
                                          &emd->peak_buffer[1], emd->size);

    *found = count;

    if(count == 0)
    {
        emd->peak_buffer[1] = emd_get_largest(signal, emd->size);
    }

    emd->peak_buffer[0] = emd->peak_buffer[1];
    emd->peak_index_buffer[0] = 0;
    count++;
    emd->peak_buffer[count] = emd->peak_buffer[count-1];
    emd->peak_index_buffer[count++] = emd->size-1;

    return count;
}

// The same for the valleys, with the smallest sample where the peaks take the
// largest.
static uint32_t emd_gather_valleys(emd_t* emd, real_t* signal,
                                   uint32_t* found)
{
    uint32_t count = valleydetect_get_valley(signal,
                                             &emd->valley_index_buffer[1],
                                             &emd->valley_buffer[1],
                                             emd->size);

    *found = count;

    if(count == 0)
    {
        emd->valley_buffer[1] = emd_get_smallest(signal, emd->size);
    }

    emd->valley_buffer[0] = emd->valley_buffer[1];
    emd->valley_index_buffer[0] = 0;
    count++;
    emd->valley_buffer[count] = emd->valley_buffer[count-1];
    emd->valley_index_buffer[count++] = emd->size-1;

    return count;
}

static void emd_update_runtime_params(emd_t* emd, real_t* x, real_t* y)
{
    ASSERT(emd != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    emd->x = x;
    emd->y = y;
}

static real_t emd_get_largest(real_t* data, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);

    real_t largest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] > largest)
        {
            largest = data[index];
        }
    }

    return largest;
}

static real_t emd_get_smallest(real_t* data, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);

    real_t smallest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] < smallest)
        {
            smallest = data[index];
        }
    }

    return smallest;
}
