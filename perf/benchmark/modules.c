// The benchmarks of the vector, the cubic spline, the Kalman filter, the
// empirical mode decomposition and the moving mean.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/linalg/vector.h>
#include <ffitt/interpolate/cspline.h>
#include <ffitt/estimate/kalman.h>
#include <ffitt/decompose/emd.h>
#include <ffitt/decompose/imf.h>
#include <ffitt/filter/movavg.h>
#include <ffitt/filter/fir.h>

#include <stdlib.h>

static const uint32_t VECTOR_SIZES[] = {2, 16, 128, 1024, 8192};
static const uint32_t VECTOR_SIZE_COUNT =
    sizeof(VECTOR_SIZES)/sizeof(VECTOR_SIZES[0]);

static const uint32_t SPLINE_SIZES[] = {8, 64, 512, 4096};
static const uint32_t SPLINE_SIZE_COUNT =
    sizeof(SPLINE_SIZES)/sizeof(SPLINE_SIZES[0]);

static const uint32_t EMD_SIZES[] = {64, 256, 1024};
static const uint32_t EMD_SIZE_COUNT = sizeof(EMD_SIZES)/sizeof(EMD_SIZES[0]);

static real_t random_value(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

void run_vector_benchmark(void)
{
    for(uint32_t index = 0; index < VECTOR_SIZE_COUNT; index++)
    {
        uint32_t size = VECTOR_SIZES[index];
        uint32_t repeats = 200;
        real_t value;

        vector_t a = vector_alloc(size);
        vector_t b = vector_alloc(size);

        for(uint32_t position = 0; position < size; position++)
        {
            vector_add_point_at_index(&a, position, random_value());
            vector_add_point_at_index(&b, position, random_value());
        }

        BENCHMARK_MEASURE("vector", "dot_product", size, repeats,
                          value = vector_dot_product(&a, &b));

        BENCHMARK_MEASURE("vector", "norm", size, repeats,
                          value = vector_norm(&a));
        (void)value;

        vector_free(&a);
        vector_free(&b);
    }
}

void run_cspline_benchmark(void)
{
    for(uint32_t index = 0; index < SPLINE_SIZE_COUNT; index++)
    {
        uint32_t size = SPLINE_SIZES[index];
        uint32_t repeats = 100;
        real_t value = REAL_C(0.0);

        real_t* x = (real_t*)malloc(sizeof(real_t)*size);
        real_t* y = (real_t*)malloc(sizeof(real_t)*size);

        for(uint32_t position = 0; position < size; position++)
        {
            x[position] = (real_t)position;
            y[position] = random_value();
        }

        cspline_t spline = cspline_alloc(size);
        cspline_mempool_t mempool = cspline_alloc_mempool(size);

        BENCHMARK_MEASURE("cspline", "init", size, repeats,
                          cspline_init(&spline, mempool, x, y));

        cspline_init(&spline, mempool, x, y);

        BENCHMARK_MEASURE("cspline", "interpolate_one_point", size, repeats * 10,
                          value = cspline_get_interpolated_point(&spline,
                                                                 (real_t)(size/2) + REAL_C(0.5)));
        (void)value;

        cspline_free(spline);
        cspline_free_mempool(mempool);
        free(x);
        free(y);
    }
}

void run_kalman_benchmark(void)
{
    static const uint32_t STATE_SIZES[] = {1, 2, 4, 8};
    static const uint32_t STATE_SIZE_COUNT =
        sizeof(STATE_SIZES)/sizeof(STATE_SIZES[0]);

    for(uint32_t index = 0; index < STATE_SIZE_COUNT; index++)
    {
        uint32_t nx = STATE_SIZES[index];
        uint32_t ny = 1;
        uint32_t repeats = 500;

        kalman_t kalman = kalman_alloc(1, nx, ny);

        matrix_t a = matrix_create_unit_matrix(nx);
        matrix_t b = matrix_create_zero_matrix(nx, 1);
        matrix_t u = matrix_create_zero_matrix(1, 1);
        matrix_t p = matrix_create_unit_matrix(nx);
        matrix_t q = matrix_create_zero_matrix(nx, nx);
        matrix_t r = matrix_create_unit_matrix(ny);
        matrix_t c = matrix_create_zero_matrix(ny, nx);
        matrix_t x = matrix_create_zero_matrix(nx, 1);
        matrix_t y = matrix_create_zero_matrix(ny, 1);

        matrix_add_element(&c, 0, 0, REAL_C(1.0));
        matrix_add_element(&y, 0, 0, REAL_C(1.0));

        kalman_set_state_matrix(&kalman, &x);
        kalman_set_state_transition_matrix(&kalman, &a);
        kalman_set_control_matrix(&kalman, &b);
        kalman_set_input_matrix(&kalman, &u);
        kalman_set_covariance_matrix(&kalman, &p);
        kalman_set_process_noise_covariance_matrix(&kalman, &q);
        kalman_set_measurement_covariance_matrix(&kalman, &r);
        kalman_set_observation_matrix(&kalman, &c);

        BENCHMARK_MEASURE("kalman", "predict", nx, repeats,
                          kalman_predict(&kalman));

        BENCHMARK_MEASURE("kalman", "step", nx, repeats,
                          kalman_step(&kalman, NULL, &y));

        matrix_free(&a);
        matrix_free(&b);
        matrix_free(&u);
        matrix_free(&p);
        matrix_free(&q);
        matrix_free(&r);
        matrix_free(&c);
        matrix_free(&x);
        matrix_free(&y);
        kalman_free(&kalman);
    }
}

void run_emd_benchmark(void)
{
    static const uint32_t NUMBER_OF_IMF = 4;

    for(uint32_t index = 0; index < EMD_SIZE_COUNT; index++)
    {
        uint32_t size = EMD_SIZES[index];
        uint32_t repeats = 10;
        uint32_t count = 0;

        real_t* x = (real_t*)malloc(sizeof(real_t)*size);
        real_t* y = (real_t*)malloc(sizeof(real_t)*size);
        real_t* residue = (real_t*)malloc(sizeof(real_t)*size);
        real_t* working = (real_t*)malloc(sizeof(real_t)*size);
        real_t* peak_index = (real_t*)malloc(sizeof(real_t)*size);
        real_t* valley_index = (real_t*)malloc(sizeof(real_t)*size);

        imf_t imf[4];
        for(uint32_t position = 0; position < NUMBER_OF_IMF; position++)
        {
            imf[position] = imf_alloc(size);
        }

        for(uint32_t position = 0; position < size; position++)
        {
            x[position] = (real_t)position;
            // A signal that holds two frequencies and a little noise.
            y[position] = (real_t)((position % 8) - 4) + (REAL_C(0.5)*(real_t)((position % 21) - 10))
                          + (REAL_C(0.1)*random_value());
        }

        emd_t emd = emd_alloc(size);
        emd_initialize(&emd, NUMBER_OF_IMF, imf, x, y, residue, working,
                       peak_index, valley_index);

        BENCHMARK_MEASURE("emd", "sift", size, repeats,
                          count = emd_sift(&emd, 3));
        (void)count;

        emd_free(emd);
        for(uint32_t position = 0; position < NUMBER_OF_IMF; position++)
        {
            imf_free(imf[position]);
        }
        free(x);
        free(y);
        free(residue);
        free(working);
        free(peak_index);
        free(valley_index);
    }
}

void run_movavg_benchmark(void)
{
    // The window sizes run from very short to very long, because the point of
    // the measurement is how the cost follows the size. It follows the size
    // for the fir and it does not for the movavg, and a short window is where
    // the fir still wins.
    static const uint32_t WINDOW_SIZES[] = {4, 16, 64, 512, 4096};
    static const uint32_t WINDOW_SIZE_COUNT =
        sizeof(WINDOW_SIZES)/sizeof(WINDOW_SIZES[0]);

    for(uint32_t index = 0; index < WINDOW_SIZE_COUNT; index++)
    {
        uint32_t size = WINDOW_SIZES[index];
        uint32_t repeats = 20000;
        real_t value = REAL_C(0.0);

        movavg_t movavg = movavg_alloc(size);
        fir_t fir = fir_alloc(size);

        // A fir whose coefficients are all the same gives the mean of its
        // window, thus the two give the same answer.
        for(uint32_t position = 0; position < size; position++)
        {
            fir_set_coefficient(&fir, position, REAL_C(1.0)/(real_t)size);
        }

        BENCHMARK_MEASURE("movavg", "process_sample", size, repeats,
                          value = movavg_process_sample(&movavg, random_value()));

        BENCHMARK_MEASURE("fir", "equal_coefficients", size, repeats,
                          value = fir_process_sample(&fir, random_value()));
        (void)value;

        movavg_free(&movavg);
        fir_free(&fir);
    }
}
