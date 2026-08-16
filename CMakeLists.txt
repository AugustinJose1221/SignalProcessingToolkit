cmake_minimum_required(VERSION 3.17)

project(SignalProcessing)

set(CMAKE_C_COMPILER gcc)

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
    kalman/kalman.c)

add_executable(signalproc
    ${SIGNALPROC_SOURCES}
    examples/emd.c
    examples/matrix.c)

# The conformation tests compare the results of the library with the results of
# the GNU Scientific Library. GSL is a test dependency only. The library itself
# does not use GSL. Thus this target is off by default.
option(BUILD_CONFORMATION_TESTS "Build the conformation tests. These tests need GSL." OFF)

if(BUILD_CONFORMATION_TESTS)
    find_package(GSL REQUIRED)

    add_executable(conformation
        ${SIGNALPROC_SOURCES}
        perf/conformation/support.c
        perf/conformation/matrix/matrix.c
        perf/conformation/matrix/support.c
        perf/conformation/vector/vector.c
        perf/conformation/vector/support.c
        perf/conformation/run.c)

    target_link_libraries(conformation GSL::gsl GSL::gslcblas m)
endif()
