cmake_minimum_required(VERSION 3.17)

# Keep this version the same as the newest entry of CHANGELOG.md and the newest
# git tag. The section "Making a release" of README.md gives the steps.
project(Ffitt VERSION 0.19.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})

# The places a system puts a library, its headers and its description of
# itself. GNUInstallDirs answers that for the system in hand rather than this
# file guessing at it.
include(GNUInstallDirs)

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

# THE ARITHMETIC OF THE LIBRARY INSTEAD OF THE SYSTEM'S.
#
# Every call into the mathematics of the system passes through the REAL_
# macros of ffitt/core/real.h. This switch puts ffitt/core/nolibm.c behind
# them, and the library then links with no mathematics library at all. That
# file holds the measured error of every function it stands in for.
option(FFITT_NO_LIBM "Use the library's own arithmetic and link no mathematics library." OFF)

# THE AREAS, AND LEAVING ONE OUT OF THE BUILD.
#
# The linker already drops what nothing calls. These switches go one step
# further and never compile the area at all, which is what a build on a very
# small flash wants to reason about, and what a shop that must account for
# every line in the image needs.
#
# core cannot be left out: everything stands on it. What each of the others
# needs is written below and the build refuses a combination that cannot
# stand, rather than failing later with a name the caller has never heard of.
# The list came from the symbols themselves and not from the includes: hht
# names a type of decompose and calls nothing of it, thus transform does NOT
# need decompose, which reading the includes would have said it did.

option(FFITT_NO_LINALG "Leave the linalg area out of the build. It needs nothing else." OFF)
option(FFITT_NO_UTIL "Leave the util area out of the build. It needs nothing else." OFF)
option(FFITT_NO_INTERPOLATE "Leave the interpolate area out of the build. It needs util." OFF)
option(FFITT_NO_DECOMPOSE "Leave the decompose area out of the build. It needs interpolate, util." OFF)
option(FFITT_NO_ESTIMATE "Leave the estimate area out of the build. It needs linalg." OFF)
option(FFITT_NO_TRANSFORM "Leave the transform area out of the build. It needs linalg." OFF)
option(FFITT_NO_FILTER "Leave the filter area out of the build. It needs linalg, transform, util." OFF)
option(FFITT_NO_DETECT "Leave the detect area out of the build. It needs transform, util." OFF)

# A combination that cannot stand is named here and not left to the linker.
function(ffitt_area_needs area needed)
    if((NOT FFITT_NO_${area}) AND FFITT_NO_${needed})
        string(TOLOWER "${area}" lower_area)
        string(TOLOWER "${needed}" lower_needed)
        message(FATAL_ERROR
            "FFITT_NO_${needed} was asked for while the ${lower_area} area is "
            "still in the build, and ${lower_area} calls into ${lower_needed}. "
            "Leave ${lower_area} out as well, or keep ${lower_needed} in.")
    endif()
endfunction()

ffitt_area_needs(INTERPOLATE UTIL)
ffitt_area_needs(DECOMPOSE INTERPOLATE)
ffitt_area_needs(DECOMPOSE UTIL)
ffitt_area_needs(ESTIMATE LINALG)
ffitt_area_needs(TRANSFORM LINALG)
ffitt_area_needs(FILTER LINALG)
ffitt_area_needs(FILTER TRANSFORM)
ffitt_area_needs(FILTER UTIL)
ffitt_area_needs(DETECT TRANSFORM)
ffitt_area_needs(DETECT UTIL)

# The sources, area by area, so that one can be left out whole.
set(FFITT_SOURCES)

list(APPEND FFITT_SOURCES
    ffitt/core/nolibm.c
    ffitt/core/real.c
    ffitt/core/ringbuf.c
)

if(NOT FFITT_NO_LINALG)
    list(APPEND FFITT_SOURCES
        ffitt/linalg/cmatrix.c
        ffitt/linalg/cnum.c
        ffitt/linalg/eigen.c
        ffitt/linalg/lstsq.c
        ffitt/linalg/matrix.c
        ffitt/linalg/pmatrix.c
        ffitt/linalg/poly.c
        ffitt/linalg/quaternion.c
        ffitt/linalg/vector.c
        ffitt/linalg/vector2d.c
    )
else()
    add_compile_definitions(FFITT_NO_LINALG)
endif()

if(NOT FFITT_NO_UTIL)
    list(APPEND FFITT_SOURCES
        ffitt/util/binarysearch.c
        ffitt/util/curve.c
        ffitt/util/generate.c
        ffitt/util/peakdetect.c
        ffitt/util/quantise.c
        ffitt/util/stats.c
        ffitt/util/valleydetect.c
    )
else()
    add_compile_definitions(FFITT_NO_UTIL)
endif()

if(NOT FFITT_NO_INTERPOLATE)
    list(APPEND FFITT_SOURCES
        ffitt/interpolate/cspline.c
        ffitt/interpolate/interp.c
    )
else()
    add_compile_definitions(FFITT_NO_INTERPOLATE)
endif()

if(NOT FFITT_NO_DECOMPOSE)
    list(APPEND FFITT_SOURCES
        ffitt/decompose/emd.c
        ffitt/decompose/imf.c
    )
else()
    add_compile_definitions(FFITT_NO_DECOMPOSE)
endif()

if(NOT FFITT_NO_ESTIMATE)
    list(APPEND FFITT_SOURCES
        ffitt/estimate/ekf.c
        ffitt/estimate/kalman.c
        ffitt/estimate/pll.c
        ffitt/estimate/propagate.c
        ffitt/estimate/ukf.c
    )
else()
    add_compile_definitions(FFITT_NO_ESTIMATE)
endif()

if(NOT FFITT_NO_TRANSFORM)
    list(APPEND FFITT_SOURCES
        ffitt/transform/bluestein.c
        ffitt/transform/cepstrum.c
        ffitt/transform/convolve.c
        ffitt/transform/correlate.c
        ffitt/transform/csd.c
        ffitt/transform/dct.c
        ffitt/transform/dwt.c
        ffitt/transform/fft.c
        ffitt/transform/goertzel.c
        ffitt/transform/hht.c
        ffitt/transform/hilbert.c
        ffitt/transform/psd.c
        ffitt/transform/slide.c
        ffitt/transform/spectrogram.c
        ffitt/transform/stft.c
        ffitt/transform/window.c
    )
else()
    add_compile_definitions(FFITT_NO_TRANSFORM)
endif()

if(NOT FFITT_NO_FILTER)
    list(APPEND FFITT_SOURCES
        ffitt/filter/adaptive.c
        ffitt/filter/dcblock.c
        ffitt/filter/detrend.c
        ffitt/filter/farrow.c
        ffitt/filter/filtfilt.c
        ffitt/filter/fir.c
        ffitt/filter/hampel.c
        ffitt/filter/iir.c
        ffitt/filter/lattice.c
        ffitt/filter/medfilt.c
        ffitt/filter/movavg.c
        ffitt/filter/resample.c
        ffitt/filter/rls.c
        ffitt/filter/savgol.c
    )
else()
    add_compile_definitions(FFITT_NO_FILTER)
endif()

if(NOT FFITT_NO_DETECT)
    list(APPEND FFITT_SOURCES
        ffitt/detect/changepoint.c
        ffitt/detect/delay.c
        ffitt/detect/matched.c
    )
else()
    add_compile_definitions(FFITT_NO_DETECT)
endif()

# The repository gives a library, thus the first target is a library. A library
# needs no main function, and it always builds.
add_library(ffitt STATIC ${FFITT_SOURCES})

# WHERE A CALLER FINDS THE HEADERS, IN THE TREE AND ONCE INSTALLED.
#
# The directory level include above serves this build. It says nothing to a
# caller who installs the library and builds against it somewhere else, thus
# the target carries its own answer for both cases.
target_include_directories(ffitt PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# THE MATHS LIBRARY IS LINKED ONLY WHERE IT IS USED.
#
# It was linked always, which made a liar of the switch beside it: a build that
# asked for no maths library still handed -lm to everything that linked this
# one. Nothing needs it when the library holds its own arithmetic.
if(NOT FFITT_NO_LIBM)
    target_link_libraries(ffitt PUBLIC m)
endif()

# PUBLIC and not PRIVATE, because the width is part of what a caller sees. Every
# structure of the library holds real_t, thus a program built at one width
# cannot use a library built at the other. The example, the benchmark and the
# conformation tests all link this target and take the width with it.
if(FFITT_REAL_64)
    target_compile_definitions(ffitt PUBLIC FFITT_REAL_64)
endif()

if(FFITT_NO_LIBM)
    # PUBLIC, because a caller that included real.h without it would reach for
    # the system's functions while the library reached for its own.
    target_compile_definitions(ffitt PUBLIC FFITT_NO_LIBM)
endif()

# Say which width was built. The reading stands in the log of every build, so
# that a build at the wrong width is seen rather than assumed.
if(FFITT_REAL_64)
    message(STATUS "ffitt: every number is held in a double, 64 bits")
else()
    message(STATUS "ffitt: every number is held in a float, 32 bits")
endif()

# ---------------------------------------------------------------------------
# INSTALLING THE LIBRARY, SO THAT SOMETHING ELSE CAN USE IT.
#
# Until now this repository could only be vendored: copy the sources in, or
# point a build at the tree. That serves the caller who wants the sources, and
# it is still the shortest road for a small target. It does not serve the
# caller who wants to install the library once and build many programs against
# it, and it does not serve a package manager at all.
#
# Two roads are laid, because callers use both and neither can be assumed:
#
#   find_package(ffitt)      for a build that uses CMake
#   pkg-config --libs ffitt  for one that does not
#
# THE WIDTH AND THE ARITHMETIC TRAVEL WITH THE LIBRARY. Every structure holds
# real_t, thus a program built at one width cannot use a library built at the
# other, and a program that reached for the system's arithmetic while the
# library held its own would disagree with it about what a sine is. Both are
# PUBLIC on the target above, thus the export carries them and a caller cannot
# get them wrong by forgetting a flag.
# ---------------------------------------------------------------------------

install(TARGETS ffitt
    EXPORT ffittTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# The headers keep the layout they have in the tree, thus an include reads the
# same whether the library was installed or vendored.
install(DIRECTORY ffitt
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")

install(EXPORT ffittTargets
    FILE ffittTargets.cmake
    NAMESPACE ffitt::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffitt)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/ffittConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ffittConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffitt)

# SameMajorVersion, because that is what the freeze means from 1.0.0 onward: a
# caller who asked for 1.2 is served by 1.7 and never by 2.0.
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ffittConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ffittConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ffittConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffitt)

# The description for a build that does not use CMake. The flags that must
# travel with the library are worked out here rather than written twice.
set(FFITT_PC_CFLAGS "")

if(FFITT_REAL_64)
    set(FFITT_PC_CFLAGS "${FFITT_PC_CFLAGS} -DFFITT_REAL_64")
endif()

if(FFITT_NO_LIBM)
    set(FFITT_PC_CFLAGS "${FFITT_PC_CFLAGS} -DFFITT_NO_LIBM")
    set(FFITT_PC_LIBS "")
else()
    set(FFITT_PC_LIBS " -lm")
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/ffitt.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/ffitt.pc
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ffitt.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# The example program. Choose the example in examples/run_example.h. The value
# RUN_NONE gives no main function, thus the program cannot link. For that
# reason this target is off by default.
option(BUILD_EXAMPLE "Build the example program. First choose an example in examples/run_example.h." OFF)

if(BUILD_EXAMPLE)
    add_executable(ffitt_example
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
        examples/detect.c
        examples/pll.c
        examples/detrend.c
        examples/interp.c
        examples/quantise.c
        examples/dct.c
        examples/convolve.c
        examples/cepstrum.c
        examples/bluestein.c
        examples/farrow.c
        examples/lattice.c
        examples/noheap.c
        examples/listen.c)

    target_link_libraries(ffitt_example ffitt m)
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
        perf/benchmark/core_and_util.c
        perf/benchmark/linalg.c
        perf/benchmark/transform.c
        perf/benchmark/filter.c
        perf/benchmark/estimate.c
        perf/benchmark/run.c)

    target_link_libraries(benchmark ffitt m)
endif()

# The cost tests hold the headers to what they claim an operation costs. The
# headers say that a moving mean does not follow its window, that decimating
# costs the output rate and not the input rate, and that a transform beats the
# plain way above a size that they name. Those sentences decide which function
# a caller picks, and nothing held them until now.
#
# The tests measure a time, thus they need a machine that is not busy with
# something else, and they take time to run. Thus this target is off by
# default. It needs no external library.
option(BUILD_COST_TESTS "Build the tests that hold the headers to what they claim an operation costs." OFF)

if(BUILD_COST_TESTS)
    # THESE TESTS MUST MEASURE AN OPTIMISED BUILD, AND WITHOUT THIS THEY DO
    # NOT. A build with no build type is a build with no optimisation, and what
    # that measures is the compiler and not the module. Measured on the same
    # machine, convolve at a shape of 512 beat the plain way by 2.2 times built
    # for release and by 1.16 times built with nothing, which is below what the
    # header claims and would have reported the header as wrong.
    #
    # A caller who names a build type keeps it. This only fills in the gap.
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release)
        message(STATUS "The cost tests measure a time, thus the build type is set to Release.")
    endif()

    add_executable(cost
        perf/cost/cost.c
        perf/cost/ringbuf.c
        perf/cost/resample.c
        perf/cost/movavg.c
        perf/cost/correlate.c
        perf/cost/goertzel.c
        perf/cost/convolve.c
        perf/cost/run.c)

    target_link_libraries(cost ffitt m)
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

    target_link_libraries(conformation ffitt GSL::gsl GSL::gslcblas m)
endif()
