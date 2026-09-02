// The benchmarks of the transform modules.
//
// ONE SIZE FOR EACH, AND THE SAME SIZE WHEREVER TWO CAN BE COMPARED. A block
// of 1024 samples runs through the transform, the cosine transform, the
// wavelet, the window and the analytic signal, thus the times of those five
// stand beside each other and say something. Where a module has a size of its
// own, such as the 4096 of the correlation, the header of the module named it.

#include <perf/benchmark/benchmark.h>

#include <ffitt/core/real.h>
#include <ffitt/decompose/imf.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/bluestein.h>
#include <ffitt/transform/cepstrum.h>
#include <ffitt/transform/convolve.h>
#include <ffitt/transform/correlate.h>
#include <ffitt/transform/csd.h>
#include <ffitt/transform/dct.h>
#include <ffitt/transform/dwt.h>
#include <ffitt/transform/fft.h>
#include <ffitt/transform/goertzel.h>
#include <ffitt/transform/hht.h>
#include <ffitt/transform/hilbert.h>
#include <ffitt/transform/psd.h>
#include <ffitt/transform/spectrogram.h>
#include <ffitt/transform/stft.h>
#include <ffitt/transform/window.h>

#include <stdlib.h>
#include <string.h>

#define TRANSFORM_BLOCK         1024u
#define TRANSFORM_SIGNAL        4096u
#define TRANSFORM_SAMPLE_RATE   REAL_C(8000.0)

static real_t transform_signal[TRANSFORM_SIGNAL];
static real_t transform_other[TRANSFORM_SIGNAL];
static real_t transform_answer[TRANSFORM_SIGNAL * 2u];
static cnum_t transform_work[TRANSFORM_SIGNAL * 2u];

static real_t transform_random(void)
{
    return ((real_t)rand() / (real_t)RAND_MAX) - REAL_C(0.5);
}

static void transform_fill(void)
{
    for(uint32_t index = 0u; index < TRANSFORM_SIGNAL; index++)
    {
        transform_signal[index] = transform_random();
        transform_other[index] = transform_random();
    }
}

static void run_fft_benchmark(void)
{
    fft_t fft = fft_alloc(TRANSFORM_BLOCK);
    cnum_t* data = (cnum_t*)malloc(sizeof(cnum_t) * TRANSFORM_BLOCK);
    real_t* magnitude = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);

    BENCHMARK_MEASURE("fft", "forward_real",
                      "a 1024 point transform of a real signal",
                      TRANSFORM_BLOCK, 2000,
                      fft_forward_real(&fft, transform_signal, data));

    BENCHMARK_MEASURE("fft", "forward",
                      "a 1024 point transform of a complex signal",
                      TRANSFORM_BLOCK, 2000,
                      fft_forward(&fft, data));

    BENCHMARK_MEASURE("fft", "inverse",
                      "a 1024 point inverse transform",
                      TRANSFORM_BLOCK, 2000,
                      fft_inverse(&fft, data));

    BENCHMARK_MEASURE("fft", "magnitude",
                      "the size of every bin of a 1024 point spectrum",
                      TRANSFORM_BLOCK, 2000,
                      fft_magnitude(data, magnitude, TRANSFORM_BLOCK));

    free(data);
    free(magnitude);
    fft_free(&fft);
}

static void run_bluestein_benchmark(void)
{
    // 1000 is not a power of two, which is the whole reason this module is
    // here. The transform beside it does 1024 and cannot do this size at all.
    static const uint32_t SIZE = 1000u;

    bluestein_t bluestein = bluestein_alloc(SIZE);
    cnum_t* data = (cnum_t*)malloc(sizeof(cnum_t) * SIZE);

    for(uint32_t index = 0u; index < SIZE; index++)
    {
        data[index] = cnum_make(transform_signal[index], REAL_C(0.0));
    }

    BENCHMARK_MEASURE("bluestein", "forward",
                      "a 1000 point transform, a size no power of two",
                      SIZE, 500,
                      bluestein_forward(&bluestein, data));

    free(data);
    bluestein_free(&bluestein);
}

static void run_dct_benchmark(void)
{
    BENCHMARK_MEASURE("dct", "forward",
                      "a 1024 point cosine transform",
                      TRANSFORM_BLOCK, 200,
                      (void)dct_forward(transform_signal, transform_answer,
                                        TRANSFORM_BLOCK));

    BENCHMARK_MEASURE("dct", "inverse",
                      "a 1024 point inverse cosine transform",
                      TRANSFORM_BLOCK, 200,
                      (void)dct_inverse(transform_answer, transform_other,
                                        TRANSFORM_BLOCK));
}

static void run_dwt_benchmark(void)
{
    dwt_t dwt = dwt_init(DWT_DAUBECHIES4);
    real_t* approximation = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t* detail = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t* whole = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t* work = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);

    BENCHMARK_MEASURE("dwt", "forward",
                      "one level of a wavelet over 1024 samples",
                      TRANSFORM_BLOCK, 2000,
                      dwt_forward(&dwt, transform_signal, TRANSFORM_BLOCK,
                                  approximation, detail));

    BENCHMARK_MEASURE("dwt", "inverse",
                      "rebuild 1024 samples from one wavelet level",
                      TRANSFORM_BLOCK, 2000,
                      dwt_inverse(&dwt, approximation, detail,
                                  TRANSFORM_BLOCK / 2u, whole));

    memcpy(whole, transform_signal, sizeof(real_t) * TRANSFORM_BLOCK);

    BENCHMARK_MEASURE("dwt", "forward_multi",
                      "four levels of a wavelet over 1024 samples",
                      TRANSFORM_BLOCK, 1000,
                      dwt_forward_multi(&dwt, whole, TRANSFORM_BLOCK, 4u, work));

    BENCHMARK_MEASURE("dwt", "threshold",
                      "take the small wavelet values out of 1024",
                      TRANSFORM_BLOCK, 2000,
                      dwt_threshold(whole, TRANSFORM_BLOCK, REAL_C(0.01)));

    free(approximation);
    free(detail);
    free(whole);
    free(work);
}

static void run_window_benchmark(void)
{
    real_t* shape = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("window", "build",
                      "build a window of Blackman and Harris over 1024",
                      TRANSFORM_BLOCK, 2000,
                      window_build(shape, TRANSFORM_BLOCK,
                                   WINDOW_BLACKMAN_HARRIS));

    BENCHMARK_MEASURE("window", "apply",
                      "put a window over 1024 samples",
                      TRANSFORM_BLOCK, 5000,
                      window_apply(shape, transform_signal, transform_answer,
                                   TRANSFORM_BLOCK));

    BENCHMARK_MEASURE("window", "noise_bandwidth",
                      "the noise bandwidth of a window of 1024",
                      TRANSFORM_BLOCK, 5000,
                      value = window_noise_bandwidth(shape, TRANSFORM_BLOCK));
    (void)value;

    free(shape);
}

static void run_hilbert_benchmark(void)
{
    fft_t fft = fft_alloc(TRANSFORM_BLOCK);
    cnum_t* analytic = (cnum_t*)malloc(sizeof(cnum_t) * TRANSFORM_BLOCK);
    real_t* amplitude = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);

    BENCHMARK_MEASURE("hilbert", "analytic_signal",
                      "the analytic signal of 1024 samples",
                      TRANSFORM_BLOCK, 1000,
                      hilbert_analytic_signal(&fft, transform_signal,
                                              analytic));

    BENCHMARK_MEASURE("hilbert", "amplitude",
                      "the envelope of 1024 samples",
                      TRANSFORM_BLOCK, 5000,
                      hilbert_amplitude(analytic, amplitude, TRANSFORM_BLOCK));

    BENCHMARK_MEASURE("hilbert", "frequency",
                      "the frequency at each of 1024 samples",
                      TRANSFORM_BLOCK, 5000,
                      hilbert_frequency(analytic, amplitude, TRANSFORM_BLOCK,
                                        TRANSFORM_SAMPLE_RATE));

    free(analytic);
    free(amplitude);
    fft_free(&fft);
}

static void run_cepstrum_benchmark(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(TRANSFORM_BLOCK);
    real_t* answer = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    uint32_t where = 0u;

    BENCHMARK_MEASURE("cepstrum", "real",
                      "the cepstrum of 1024 samples",
                      TRANSFORM_BLOCK, 500,
                      (void)cepstrum_real(&cepstrum, transform_signal, answer));

    BENCHMARK_MEASURE("cepstrum", "best_quefrency",
                      "find the pitch in a cepstrum of 1024",
                      TRANSFORM_BLOCK, 5000,
                      where = cepstrum_best_quefrency(answer, TRANSFORM_BLOCK,
                                                      20u, 400u, NULL));
    (void)where;

    free(answer);
    cepstrum_free(&cepstrum);
}

static void run_goertzel_benchmark(void)
{
    goertzel_t watcher = goertzel_init(REAL_C(1000.0), TRANSFORM_SAMPLE_RATE,
                                       TRANSFORM_BLOCK);
    real_t value = REAL_C(0.0);

    BENCHMARK_MEASURE("goertzel", "process_block",
                      "watch one frequency over a block of 1024",
                      TRANSFORM_BLOCK, 2000,
                      {
                          goertzel_reset(&watcher);
                          goertzel_process_block(&watcher, transform_signal,
                                                 TRANSFORM_BLOCK);
                      });

    BENCHMARK_MEASURE("goertzel", "magnitude",
                      "read how much of that frequency was there",
                      TRANSFORM_BLOCK, 20000,
                      value = goertzel_magnitude(&watcher));
    (void)value;
}

static void run_correlate_benchmark(void)
{
    uint32_t transform_size = correlate_transform_size(TRANSFORM_SIGNAL);
    fft_t fft = fft_alloc(transform_size);
    real_t* window = (real_t*)malloc(sizeof(real_t) * transform_size);
    real_t strength = REAL_C(0.0);
    uint32_t lag = 0u;

    BENCHMARK_MEASURE("correlate", "auto",
                      "4096 samples against themselves at every lag",
                      TRANSFORM_SIGNAL, 5,
                      (void)correlate_auto(transform_signal, TRANSFORM_SIGNAL,
                                           transform_answer,
                                           TRANSFORM_SIGNAL - 1u,
                                           CORRELATE_RAW));

    BENCHMARK_MEASURE("correlate", "auto_by_transform",
                      "the same by the transform",
                      TRANSFORM_SIGNAL, 200,
                      (void)correlate_auto_by_transform(transform_signal,
                                                        TRANSFORM_SIGNAL,
                                                        transform_answer,
                                                        TRANSFORM_SIGNAL - 1u,
                                                        CORRELATE_RAW, &fft,
                                                        transform_work,
                                                        window));

    BENCHMARK_MEASURE("correlate", "cross",
                      "4096 samples against another signal at 512 lags",
                      TRANSFORM_SIGNAL, 20,
                      (void)correlate_cross(transform_signal, transform_other,
                                            TRANSFORM_SIGNAL,
                                            transform_answer, 512u,
                                            CORRELATE_RAW));

    BENCHMARK_MEASURE("correlate", "best_lag",
                      "find the period of 4096 samples",
                      TRANSFORM_SIGNAL, 20,
                      lag = correlate_best_lag(transform_signal,
                                               TRANSFORM_SIGNAL,
                                               transform_answer, 20u, 512u,
                                               &strength));
    (void)lag;

    free(window);
    fft_free(&fft);
}

static void run_convolve_benchmark(void)
{
    static const uint32_t SHAPE = 512u;

    uint32_t transform_size = convolve_transform_size(TRANSFORM_SIGNAL, SHAPE);
    fft_t fft = fft_alloc(transform_size);
    cnum_t* second = (cnum_t*)malloc(sizeof(cnum_t) * transform_size);
    real_t* work = (real_t*)malloc(sizeof(real_t) * transform_size);

    BENCHMARK_MEASURE("convolve", "direct",
                      "slide a shape of 512 along 4096 samples",
                      TRANSFORM_SIGNAL, 50,
                      (void)convolve_direct(transform_signal, TRANSFORM_SIGNAL,
                                            transform_other, SHAPE,
                                            transform_answer, CONVOLVE_FULL));

    BENCHMARK_MEASURE("convolve", "by_transform",
                      "the same by the transform",
                      TRANSFORM_SIGNAL, 200,
                      (void)convolve_by_transform(transform_signal,
                                                  TRANSFORM_SIGNAL,
                                                  transform_other, SHAPE,
                                                  transform_answer,
                                                  CONVOLVE_FULL, &fft,
                                                  transform_work, second,
                                                  work));

    free(second);
    free(work);
    fft_free(&fft);
}

static void run_psd_benchmark(void)
{
    psd_t psd = psd_alloc(256u);
    real_t* density = NULL;

    (void)psd_design(&psd, 128u, WINDOW_HANN, REAL_C(0.0));
    density = (real_t*)malloc(sizeof(real_t) * psd_bin_count(&psd));

    BENCHMARK_MEASURE("psd", "estimate",
                      "the spectrum of 4096 samples by the way of Welch",
                      TRANSFORM_SIGNAL, 200,
                      (void)psd_estimate(&psd, transform_signal,
                                         TRANSFORM_SIGNAL,
                                         TRANSFORM_SAMPLE_RATE, density));

    BENCHMARK_MEASURE("psd", "band_power",
                      "the power between two frequencies of that spectrum",
                      TRANSFORM_SIGNAL, 5000,
                      (void)psd_band_power(&psd, density,
                                           TRANSFORM_SAMPLE_RATE,
                                           REAL_C(100.0), REAL_C(1000.0)));

    free(density);
    psd_free(&psd);
}

static void run_csd_benchmark(void)
{
    csd_t csd = csd_alloc(256u);
    cnum_t* cross = (cnum_t*)malloc(sizeof(cnum_t) * 256u);
    real_t* answer = (real_t*)malloc(sizeof(real_t) * 256u);

    (void)csd_design(&csd, 128u, WINDOW_HANN, REAL_C(0.0));

    BENCHMARK_MEASURE("csd", "estimate",
                      "the cross spectrum of two signals of 4096",
                      TRANSFORM_SIGNAL, 200,
                      (void)csd_estimate(&csd, transform_signal,
                                         transform_other, TRANSFORM_SIGNAL,
                                         TRANSFORM_SAMPLE_RATE, cross));

    BENCHMARK_MEASURE("csd", "coherence",
                      "how much two signals of 4096 agree, bin by bin",
                      TRANSFORM_SIGNAL, 200,
                      (void)csd_coherence(&csd, transform_signal,
                                          transform_other, TRANSFORM_SIGNAL,
                                          answer));

    BENCHMARK_MEASURE("csd", "transfer",
                      "the transfer between two signals of 4096",
                      TRANSFORM_SIGNAL, 200,
                      (void)csd_transfer(&csd, transform_signal,
                                         transform_other, TRANSFORM_SIGNAL,
                                         cross));

    free(cross);
    free(answer);
    csd_free(&csd);
}

static void run_stft_benchmark(void)
{
    static const uint32_t BLOCK = 256u;
    static const uint32_t HOP = 64u;

    stft_t stft = stft_alloc(BLOCK);
    uint32_t frames = stft_frame_count(TRANSFORM_SIGNAL, BLOCK, HOP);
    uint32_t room = frames * BLOCK;
    cnum_t* spectra = (cnum_t*)malloc(sizeof(cnum_t) * room);
    real_t* rebuilt = (real_t*)malloc(sizeof(real_t) * TRANSFORM_SIGNAL);
    real_t* weight = (real_t*)malloc(sizeof(real_t) * TRANSFORM_SIGNAL);
    real_t* picture = NULL;
    uint32_t values = 0u;

    (void)stft_design(&stft, HOP, WINDOW_HANN, REAL_C(0.0));

    BENCHMARK_MEASURE("stft", "forward",
                      "4096 samples into frames of 256, hopping 64",
                      TRANSFORM_SIGNAL, 200,
                      (void)stft_forward(&stft, transform_signal,
                                         TRANSFORM_SIGNAL, spectra, room));

    BENCHMARK_MEASURE("stft", "inverse",
                      "rebuild the 4096 samples from those frames",
                      TRANSFORM_SIGNAL, 200,
                      (void)stft_inverse(&stft, spectra, frames, rebuilt,
                                         TRANSFORM_SIGNAL, weight));

    values = spectrogram_value_count(&stft, frames);
    picture = (real_t*)malloc(sizeof(real_t) * values);

    BENCHMARK_MEASURE("spectrogram", "build",
                      "a picture in decibel from those frames",
                      TRANSFORM_SIGNAL, 200,
                      (void)spectrogram_build(&stft, spectra, frames,
                                              SPECTROGRAM_AMPLITUDE,
                                              TRANSFORM_SAMPLE_RATE, picture,
                                              values));

    BENCHMARK_MEASURE("spectrogram", "against_the_largest",
                      "hold that picture against its own loudest point",
                      TRANSFORM_SIGNAL, 500,
                      (void)spectrogram_against_the_largest(picture, values,
                                                            picture));

    free(spectra);
    free(rebuilt);
    free(weight);
    free(picture);
    stft_free(&stft);
}

static void run_hht_benchmark(void)
{
    fft_t fft = fft_alloc(TRANSFORM_BLOCK);
    imf_t mode = imf_alloc(TRANSFORM_BLOCK);
    real_t* amplitude = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t* frequency = (real_t*)malloc(sizeof(real_t) * TRANSFORM_BLOCK);
    real_t value = REAL_C(0.0);

    for(uint32_t index = 0u; index < TRANSFORM_BLOCK; index++)
    {
        mode.x[index] = (real_t)index;
        mode.y[index] = transform_signal[index];
    }
    mode.size = TRANSFORM_BLOCK;

    BENCHMARK_MEASURE("hht", "transform_imf",
                      "amplitude and frequency of one mode of 1024",
                      TRANSFORM_BLOCK, 500,
                      hht_transform_imf(&fft, &mode, transform_work, amplitude,
                                        frequency, TRANSFORM_SAMPLE_RATE));

    BENCHMARK_MEASURE("hht", "mean_frequency",
                      "the mean frequency of that mode",
                      TRANSFORM_BLOCK, 20000,
                      value = hht_mean_frequency(amplitude, frequency,
                                                 TRANSFORM_BLOCK));
    (void)value;

    free(amplitude);
    free(frequency);
    imf_free(mode);
    fft_free(&fft);
}

void run_transform_benchmark(void)
{
    transform_fill();

    run_fft_benchmark();
    run_bluestein_benchmark();
    run_dct_benchmark();
    run_dwt_benchmark();
    run_window_benchmark();
    run_hilbert_benchmark();
    run_cepstrum_benchmark();
    run_goertzel_benchmark();
    run_correlate_benchmark();
    run_convolve_benchmark();
    run_psd_benchmark();
    run_csd_benchmark();
    run_stft_benchmark();
    run_hht_benchmark();
}
