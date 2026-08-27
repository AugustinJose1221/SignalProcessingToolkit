#ifndef RUN_EXAMPLE_H
#define RUN_EXAMPLE_H

// Which example the program runs.
//
// Each example holds its own main function, and every main function stands
// inside a condition on RUN_EXAMPLE. Thus one build gives one example. Change
// the value below, and then build with:
//
//     cmake -S . -B build -DBUILD_EXAMPLE=ON && cmake --build build
//     ./build/signalproc_example
//
// The value RUN_NONE gives no main function at all, thus the example program
// cannot link. That value is the default, because the library itself is the
// work of this repository and the examples only show how to use it.
//
// Every name below must stand in this file. A name that is not there counts as
// zero inside an #if, and zero is the value of RUN_NONE. Every example would
// then give a main function, and the program would hold several of them.

#define RUN_NONE                    0
#define RUN_EMD_EXAMPLE             1
#define RUN_MATRIX_EXAMPLE          2
#define RUN_FFT_EXAMPLE             3
#define RUN_FILTER_EXAMPLE          4
#define RUN_HHT_EXAMPLE             5
#define RUN_EKF_EXAMPLE             6
#define RUN_GOERTZEL_EXAMPLE        7
#define RUN_DWT_EXAMPLE             8
#define RUN_SAVGOL_EXAMPLE          9
#define RUN_RESAMPLE_EXAMPLE        10
#define RUN_PSD_EXAMPLE             11
#define RUN_CORRELATE_EXAMPLE       12
#define RUN_ADAPTIVE_EXAMPLE        13
#define RUN_CLEAN_EXAMPLE           14
#define RUN_FILTFILT_EXAMPLE        15
#define RUN_KALMAN_EXAMPLE          16
#define RUN_STREAM_EXAMPLE          17
#define RUN_CALIBRATE_EXAMPLE       18
#define RUN_LINALG_EXAMPLE          19
#define RUN_ATTITUDE_EXAMPLE        20
#define RUN_FITCURVE_EXAMPLE        21
#define RUN_SPECTROGRAM_EXAMPLE     22
#define RUN_COHERENCE_EXAMPLE       23
#define RUN_SHAPES_EXAMPLE          24
#define RUN_CONTINUOUS_EXAMPLE      25

#define RUN_EXAMPLE     RUN_NONE

#endif//RUN_EXAMPLE_H
