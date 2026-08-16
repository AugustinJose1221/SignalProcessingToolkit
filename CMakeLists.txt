cmake_minimum_required(VERSION 3.17)

# Keep this version the same as the newest entry of CHANGELOG.md and the newest
# git tag. The section "Making a release" of README.md gives the steps.
project(SignalProcessing VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})

# The library sources. These sources use only the C standard library.
set(SIGNALPROC_SOURCES
    utils/binarysearch/binarysearch.c
    utils/peakdetect/peakdetect.c
    utils/valleydetect/valleydetect.c
    cspline/cspline.c
    vector/vector.c
    vector2d/vector2d.c
    emd/emd.c
    imf/imf.c
    matrix/matrix.c
    cnum/cnum.c
    cmatrix/cmatrix.c
    pmatrix/pmatrix.c
    fft/fft.c
    hilbert/hilbert.c
    hht/hht.c
    fir/fir.c
    iir/iir.c
    ekf/ekf.c
    goertzel/goertzel.c
    dwt/dwt.c
    savgol/savgol.c
    kalman/kalman.c)

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
        examples/matrix.c)

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
