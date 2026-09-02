cmake_minimum_required(VERSION 3.17)

# Keep this version the same as the newest entry of CHANGELOG.md and the newest
# git tag. The section "Making a release" of README.md gives the steps.
project(Ffitt VERSION 0.18.2 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})

# THE WIDTH THAT EVERY NUMBER IS HELD IN.
#
# Off gives a float, which keeps about 7 digits. On gives a double, which keeps
# about 16. Nothing else in the build changes.
#
#     cmake -S . -B build -DFFITT_REAL_64=ON
#
# THIS WAS ASKED FOR AND NOT READ. The workflow passed -DFFITT_REAL_64=ON to
# four builds and nothing here ever looked at it, thus every one of them was
# built at 32 bits and the job named "64 bit" was the 32 bit job over again.
# Passing a variable that no file reads fails silently, which is the whole of
# why it stood for so long.
option(FFITT_REAL_64 "Hold every number in a double rather than a float." OFF)

# The library sources. These sources use only the C standard library.
set(SIGNALPROC_SOURCES
    ffitt/core/real.c
    ffitt/core/ringbuf.c
    ffitt/linalg/cmatrix.c
    ffitt/linalg/cnum.c
    ffitt/linalg/matrix.c
    ffitt/linalg/eigen.c
    ffitt/linalg/lstsq.c
    ffitt/linalg/pmatrix.c
    ffitt/linalg/poly.c
    ffitt/linalg/quaternion.c
    ffitt/linalg/vector.c
    ffitt/linalg/vector2d.c
    ffitt/interpolate/cspline.c
    ffitt/interpolate/interp.c
    ffitt/transform/bluestein.c
    ffitt/transform/cepstrum.c
    ffitt/transform/dct.c
    ffitt/transform/dwt.c
    ffitt/transform/fft.c
    ffitt/transform/goertzel.c
    ffitt/transform/hht.c
    ffitt/transform/hilbert.c
    ffitt/transform/window.c
    ffitt/transform/correlate.c
    ffitt/transform/convolve.c
    ffitt/transform/csd.c
    ffitt/transform/psd.c
    ffitt/transform/spectrogram.c
    ffitt/transform/stft.c
    ffitt/filter/fir.c
    ffitt/filter/iir.c
    ffitt/filter/savgol.c
    ffitt/filter/movavg.c
    ffitt/filter/medfilt.c
    ffitt/filter/dcblock.c
    ffitt/filter/detrend.c
    ffitt/filter/hampel.c
    ffitt/filter/adaptive.c
    ffitt/filter/resample.c
    ffitt/filter/lattice.c
    ffitt/filter/rls.c
    ffitt/filter/filtfilt.c
    ffitt/filter/farrow.c
    ffitt/estimate/ekf.c
    ffitt/estimate/kalman.c
    ffitt/estimate/propagate.c
    ffitt/estimate/pll.c
    ffitt/estimate/ukf.c
    ffitt/decompose/emd.c
    ffitt/decompose/imf.c
    ffitt/util/binarysearch.c
    ffitt/util/curve.c
    ffitt/util/generate.c
    ffitt/util/quantise.c
    ffitt/util/stats.c
    ffitt/util/peakdetect.c
    ffitt/util/valleydetect.c
    ffitt/detect/matched.c
    ffitt/detect/delay.c
    ffitt/detect/changepoint.c)

# The repository gives a library, thus the first target is a library. A library
# needs no main function, and it always builds.
add_library(signalproc STATIC ${SIGNALPROC_SOURCES})
target_link_libraries(signalproc m)

# PUBLIC and not PRIVATE, because the width is part of what a caller sees. Every
# structure of the library holds real_t, thus a program built at one width
# cannot use a library built at the other. The example, the benchmark and the
# conformation tests all link this target and take the width with it.
if(FFITT_REAL_64)
    target_compile_definitions(signalproc PUBLIC FFITT_REAL_64)
endif()

# Say which width was built. The reading stands in the log of every build, so
# that a build at the wrong width is seen rather than assumed.
if(FFITT_REAL_64)
    message(STATUS "ffitt: every number is held in a double, 64 bits")
else()
    message(STATUS "ffitt: every number is held in a float, 32 bits")
endif()

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
        perf/conformation/fft/fft.c
        perf/conformation/eigen/eigen.c
        perf/conformation/poly/poly.c
        perf/conformation/stats/stats.c
        perf/conformation/cspline/cspline.c
        perf/conformation/lstsq/lstsq.c
        perf/conformation/dwt/dwt.c
        perf/conformation/run.c)

    target_link_libraries(conformation signalproc GSL::gsl GSL::gslcblas m)
endif()
