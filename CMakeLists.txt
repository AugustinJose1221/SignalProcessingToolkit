cmake_minimum_required(VERSION 3.17)

# Keep this version the same as the newest entry of CHANGELOG.md and the newest
# git tag. The section "Making a release" of README.md gives the steps.
project(SignalProcessing VERSION 0.17.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})

# The library sources. These sources use only the C standard library.
set(SIGNALPROC_SOURCES
    sptk/core/real.c
    sptk/core/ringbuf.c
    sptk/linalg/cmatrix.c
    sptk/linalg/cnum.c
    sptk/linalg/matrix.c
    sptk/linalg/eigen.c
    sptk/linalg/lstsq.c
    sptk/linalg/pmatrix.c
    sptk/linalg/poly.c
    sptk/linalg/quaternion.c
    sptk/linalg/vector.c
    sptk/linalg/vector2d.c
    sptk/interpolate/cspline.c
    sptk/interpolate/interp.c
    sptk/transform/bluestein.c
    sptk/transform/cepstrum.c
    sptk/transform/dct.c
    sptk/transform/dwt.c
    sptk/transform/fft.c
    sptk/transform/goertzel.c
    sptk/transform/hht.c
    sptk/transform/hilbert.c
    sptk/transform/window.c
    sptk/transform/correlate.c
    sptk/transform/convolve.c
    sptk/transform/csd.c
    sptk/transform/psd.c
    sptk/transform/spectrogram.c
    sptk/transform/stft.c
    sptk/filter/fir.c
    sptk/filter/iir.c
    sptk/filter/savgol.c
    sptk/filter/movavg.c
    sptk/filter/medfilt.c
    sptk/filter/dcblock.c
    sptk/filter/detrend.c
    sptk/filter/hampel.c
    sptk/filter/adaptive.c
    sptk/filter/resample.c
    sptk/filter/lattice.c
    sptk/filter/rls.c
    sptk/filter/filtfilt.c
    sptk/filter/farrow.c
    sptk/estimate/ekf.c
    sptk/estimate/kalman.c
    sptk/estimate/propagate.c
    sptk/estimate/pll.c
    sptk/estimate/ukf.c
    sptk/decompose/emd.c
    sptk/decompose/imf.c
    sptk/util/binarysearch.c
    sptk/util/curve.c
    sptk/util/generate.c
    sptk/util/quantise.c
    sptk/util/stats.c
    sptk/util/peakdetect.c
    sptk/util/valleydetect.c
    sptk/detect/matched.c
    sptk/detect/delay.c
    sptk/detect/changepoint.c)

# The repository gives a library, thus the first target is a library. A library
# needs no main function, and it always builds.
add_library(signalproc STATIC ${SIGNALPROC_SOURCES})
target_link_libraries(signalproc m)

# The example program. Choose the example in examples/run_example.h. The value
# RUN_NONE gives no main function, thus the program cannot link. For that
# reason this target is off by default.
option(BUILD_EXAMPLE "Build the example program. First choose an example in examples/run_example.h." OFF)

if(BUILD_EXAMPLE)
    add_executable(signalproc_example
        examples/emd.c
        examples/matrix.c
        examples/fft.c
        examples/filter.c
        examples/hht.c
        examples/ekf.c
        examples/goertzel.c
        examples/dwt.c
        examples/savgol.c
        examples/resample.c
        examples/psd.c
        examples/correlate.c
        examples/adaptive.c
        examples/clean.c
        examples/filtfilt.c
        examples/kalman.c
        examples/stream.c
        examples/calibrate.c
        examples/linalg.c
        examples/attitude.c
        examples/fitcurve.c
        examples/spectrogram.c
        examples/coherence.c
        examples/shapes.c
        examples/continuous.c
        examples/survey.c
        examples/detect.c)

    target_link_libraries(signalproc_example signalproc m)
endif()

# The benchmark measures how long each operation of the library takes. It needs
# no external library, but it takes time to run. Thus this target is off by
# default.
option(BUILD_BENCHMARK "Build the benchmark program." OFF)

if(BUILD_BENCHMARK)
    add_executable(benchmark
        perf/benchmark/benchmark.c
        perf/benchmark/matrix.c
        perf/benchmark/modules.c
        perf/benchmark/run.c)

    target_link_libraries(benchmark signalproc m)
endif()

# The conformation tests compare the results of the library with the results of
# the GNU Scientific Library. GSL is a test dependency only. The library itself
# does not use GSL. Thus this target is off by default.
option(BUILD_CONFORMATION_TESTS "Build the conformation tests. These tests need GSL." OFF)

if(BUILD_CONFORMATION_TESTS)
    find_package(GSL REQUIRED)

    add_executable(conformation
        perf/conformation/support.c
        perf/conformation/matrix/matrix.c
        perf/conformation/matrix/support.c
        perf/conformation/vector/vector.c
        perf/conformation/vector/support.c
        perf/conformation/run.c)

    target_link_libraries(conformation signalproc GSL::gsl GSL::gslcblas m)
endif()
