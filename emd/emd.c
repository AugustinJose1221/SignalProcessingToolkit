#ifndef TEST
#include <emd/emd.h>
#include <common/defs.h>
#include <string.h>
#include <utils/peakdetect/peakdetect.h>
#include <utils/valleydetect/valleydetect.h>
#else
#include "emd.h"
#include "defs.h"
#include <string.h>
#include "peakdetect.h"
#include "valleydetect.h"
#endif

static void emd_update_runtime_params(emd_t* emd, float* x, float* y);
static float emd_get_largest(float* data, uint32_t size);
static float emd_get_smallest(float* data, uint32_t size);

emd_t emd_alloc(uint32_t size)
{
    ASSERT(size != 0);

    emd_t emd;

    emd.size = size;
    emd.cspline = cspline_alloc(size);
    emd.cspline_mempool = cspline_alloc_mempool(size);
    emd.peak_buffer = (float*)malloc(sizeof(float)*size);
    emd.valley_buffer = (float*)malloc(sizeof(float)*size);
    emd.dynamic_alloc = true;

    return emd;
}

emd_t emd_static_alloc(uint32_t size, float** membank, float** mempool, float* peak_buffer, float* valley_buffer)
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

void emd_initialize(emd_t* emd, uint32_t num_of_imf, imf_t* imf, float* x, float* y, float* residue, float* working_buffer, float* peak_index_buffer, float* valley_index_buffer)
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
    uint32_t start_index;
    uint32_t shift;
    uint32_t iteration_count = 0;

    float interpolation_index = 0;

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
            emd->imf[imf_index].x[index] = (float)index;
            emd->imf[imf_index].y[index] = 0.0f;
        }
        return &emd->imf[imf_index];
    }

    start_index = (uint32_t)emd->x[0];
    shift = (uint32_t)(emd->x[1]-emd->x[0]);

    memcpy(emd->working_buffer, emd->residue, sizeof(float)*emd->size);

    peakcount = peakdetect_get_peaks(emd->working_buffer, &emd->peak_index_buffer[1], &emd->peak_buffer[1], emd->size);
    valleycount = valleydetect_get_valley(emd->working_buffer, &emd->valley_index_buffer[1], &emd->valley_buffer[1], emd->size);

    // A signal that only rises or only falls holds no peak and no valley. The
    // detection then writes nothing, and the two buffers hold no value. Take
    // the largest sample for the upper envelope and the smallest sample for
    // the lower envelope, so that both envelopes hold a value.
    if(peakcount == 0)
    {
        emd->peak_buffer[1] = emd_get_largest(emd->working_buffer, emd->size);
    }
    if(valleycount == 0)
    {
        emd->valley_buffer[1] = emd_get_smallest(emd->working_buffer, emd->size);
    }

    emd->peak_buffer[0] = emd->peak_buffer[1];
    emd->peak_index_buffer[0] = 0;
    peakcount++;
    emd->peak_buffer[peakcount] = emd->peak_buffer[peakcount-1];
    emd->peak_index_buffer[peakcount++] = emd->size-1;

    emd->valley_buffer[0] = emd->valley_buffer[1];
    emd->valley_index_buffer[0] = 0;
    valleycount++;
    emd->valley_buffer[valleycount] = emd->valley_buffer[valleycount-1];
    emd->valley_index_buffer[valleycount++] = emd->size-1;

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

        peakcount = peakdetect_get_peaks(emd->working_buffer, &emd->peak_index_buffer[1], &emd->peak_buffer[1], emd->size);
        valleycount = valleydetect_get_valley(emd->working_buffer, &emd->valley_index_buffer[1], &emd->valley_buffer[1], emd->size);
        emd->peak_buffer[0] = emd->peak_buffer[1];
        emd->peak_index_buffer[0] = 0;
        peakcount++;
        emd->peak_buffer[peakcount] = emd->peak_buffer[peakcount-1];
        emd->peak_index_buffer[peakcount++] = emd->size-1;

        emd->valley_buffer[0] = emd->valley_buffer[1];
        emd->valley_index_buffer[0] = 0;
        valleycount++;
        emd->valley_buffer[valleycount] = emd->valley_buffer[valleycount-1];
        emd->valley_index_buffer[valleycount++] = emd->size-1;

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

    memcpy(emd->residue, emd->y, sizeof(float)*emd->size);

    do{
        imf = emd_get_imf(emd, imf_count, stopping_threshold, &status);
        for(uint32_t index = 0; index < emd->size; index++)
        {
            emd->residue[index] = emd->residue[index] - imf->y[index];
        }   
        emd_update_runtime_params(emd, emd->x, emd->residue);
        imf_count++;
    }while(imf_count < emd->imf_count && status == 1);

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

static void emd_update_runtime_params(emd_t* emd, float* x, float* y)
{
    ASSERT(emd != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    emd->x = x;
    emd->y = y;
}

static float emd_get_largest(float* data, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);

    float largest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] > largest)
        {
            largest = data[index];
        }
    }

    return largest;
}

static float emd_get_smallest(float* data, uint32_t size)
{
    ASSERT(data != NULL);
    ASSERT(size > 0);

    float smallest = data[0];

    for(uint32_t index = 1; index < size; index++)
    {
        if(data[index] < smallest)
        {
            smallest = data[index];
        }
    }

    return smallest;
}
