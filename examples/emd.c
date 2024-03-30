#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_EMD_EXAMPLE)

#include <cspline/cspline.h>
#include <emd/emd.h>
#include <imf/imf.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define SAMPLE_SIZE 1000u
#define PI 3.14159265f
#define RADS_TO_DEG(x)  ((PI/180)*x)
#define NUMBER_OF_IMF   6u

int main()
{
    float x[SAMPLE_SIZE];
    float y[SAMPLE_SIZE];

    float residue[SAMPLE_SIZE];
    float working_buffer[SAMPLE_SIZE];
    float peak_index_buffer[SAMPLE_SIZE];
    float valley_index_buffer[SAMPLE_SIZE];

    uint32_t imf_count;

    emd_t emd;
    imf_t imf[NUMBER_OF_IMF];

    srand(time(0));
    for(int i = 0; i < SAMPLE_SIZE; i++)
    {
        x[i] = i;
        y[i] = sin(RADS_TO_DEG(10*i)) + sin(RADS_TO_DEG(20*i)) /*+ log(rand())*/;
    }

    for(int i = 0; i < NUMBER_OF_IMF; i++)
    {
        imf[i] = imf_alloc(SAMPLE_SIZE);
    }
    emd = emd_alloc(SAMPLE_SIZE);
    emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working_buffer, peak_index_buffer, valley_index_buffer);
    imf_count = emd_sift(&emd, 3);
    
    imf_print_all(imf, SAMPLE_SIZE, imf_count, NULL);

    return 0;
}

#endif

