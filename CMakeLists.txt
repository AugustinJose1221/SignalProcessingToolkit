cmake_minimum_required(VERSION 3.17)

project(SignalProcessing)   

set(CMAKE_C_COMPILER gcc)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(signalproc
    utils/binarysearch/binarysearch.c
    utils/peakdetect/peakdetect.c
    utils/valleydetect/valleydetect.c
    cspline/cspline.c
    vector/vector.c
    vector2d/vector2d.c
    emd/emd.c
    imf/imf.c
    matrix/matrix.c
    examples/emd.c
    examples/matrix.c)
