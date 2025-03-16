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
    uint32_t start_index = (uint32_t)emd->x[0];
    uint32_t end_index = (uint32_t)emd->x[emd->size-1];
    uint32_t shift = (uint32_t)(emd->x[1]-emd->x[0]);
    uint32_t iteration_count = 0;

    float interpolation_index = 0;

    memcpy(emd->working_buffer, emd->residue, sizeof(float)*emd->size);
    
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
    
    *status = 0;

    while(peakcount > 1 && valleycount > 1 && iteration_count < stopping_threshold)
    {
        cspline_update_size(&emd->cspline, peakcount);
        cspline_init(&emd->cspline, emd->cspline_mempool, emd->peak_index_buffer, emd->peak_buffer);

        for(int index = 0; index < emd->size; index++)
        {
            interpolation_index = start_index + (index*shift);
            emd->imf[imf_index].x[index] = index;
            emd->imf[imf_index].y[index] = cspline_get_interpolated_point(&emd->cspline, interpolation_index);
        }

        cspline_update_size(&emd->cspline, valleycount);
        cspline_init(&emd->cspline, emd->cspline_mempool, emd->valley_index_buffer, emd->valley_buffer);
        for(int index = 0; index < emd->size; index++)
        {
            interpolation_index = start_index + (index*shift);
            emd->imf[imf_index].x[index] = index;
            emd->imf[imf_index].y[index] += cspline_get_interpolated_point(&emd->cspline, interpolation_index);
            emd->imf[imf_index].y[index] /=2;
        }

        for(int index = 0; index < emd->size; index++)
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
        for(int index = 0; index < emd->size; index++)
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