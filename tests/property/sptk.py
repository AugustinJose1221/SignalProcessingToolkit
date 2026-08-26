"""Bindings that let Python call the library.

The library keeps its rule that it needs no external library. Python is a test
tool only. This module builds the C sources into a shared object and reads the
functions from it with ctypes. It changes no source file of the library.
"""

import ctypes
import os
import subprocess
import sys

REPOSITORY = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# The library holds every number in real_t, which is a float or a double
# depending on how it is built. The tests must agree with the build, thus the
# same variable that chooses the width for the C build chooses it here.
#
# Set SPTK_REAL_64 in the environment to test the 64 bit build.
REAL_64 = os.environ.get("SPTK_REAL_64", "") not in ("", "0", "false", "False")
REAL_T = ctypes.c_double if REAL_64 else ctypes.c_float
WIDTH = "64" if REAL_64 else "32"

BUILD_DIRECTORY = os.path.join(REPOSITORY, "build", "property")
LIBRARY_PATH = os.path.join(BUILD_DIRECTORY, "libsptk%s.so" % WIDTH)

# The same list of sources as SIGNALPROC_SOURCES in CMakeLists.txt.
SOURCES = [
    "sptk/core/real.c",
    "sptk/core/ringbuf.c",
    "sptk/linalg/cmatrix.c",
    "sptk/linalg/cnum.c",
    "sptk/linalg/matrix.c",
    "sptk/linalg/eigen.c",
    "sptk/linalg/lstsq.c",
    "sptk/linalg/pmatrix.c",
    "sptk/linalg/quaternion.c",
    "sptk/linalg/vector.c",
    "sptk/linalg/vector2d.c",
    "sptk/interpolate/cspline.c",
    "sptk/interpolate/interp.c",
    "sptk/transform/bluestein.c",
    "sptk/transform/dwt.c",
    "sptk/transform/fft.c",
    "sptk/transform/goertzel.c",
    "sptk/transform/hht.c",
    "sptk/transform/hilbert.c",
    "sptk/transform/window.c",
    "sptk/transform/correlate.c",
    "sptk/transform/convolve.c",
    "sptk/transform/csd.c",
    "sptk/transform/psd.c",
    "sptk/transform/spectrogram.c",
    "sptk/transform/stft.c",
    "sptk/filter/fir.c",
    "sptk/filter/iir.c",
    "sptk/filter/savgol.c",
    "sptk/filter/movavg.c",
    "sptk/filter/medfilt.c",
    "sptk/filter/dcblock.c",
    "sptk/filter/detrend.c",
    "sptk/filter/hampel.c",
    "sptk/filter/adaptive.c",
    "sptk/filter/resample.c",
    "sptk/filter/rls.c",
    "sptk/filter/filtfilt.c",
    "sptk/estimate/ekf.c",
    "sptk/estimate/kalman.c",
    "sptk/estimate/ukf.c",
    "sptk/decompose/emd.c",
    "sptk/decompose/imf.c",
    "sptk/util/binarysearch.c",
    "sptk/util/stats.c",
    "sptk/util/peakdetect.c",
    "sptk/util/valleydetect.c",
]


def build_library():
    """Build the shared object. Give the path to it."""
    os.makedirs(BUILD_DIRECTORY, exist_ok=True)
    command = [
        "gcc", "-shared", "-fPIC", "-std=c99", "-g", "-O1",
        "-I", REPOSITORY,
    ]
    if REAL_64:
        command.append("-DSPTK_REAL_64")
    command += ["-o", LIBRARY_PATH]
    command += [os.path.join(REPOSITORY, source) for source in SOURCES] + ["-lm"]

    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("the build of the shared object failed:\n" + result.stderr)
    return LIBRARY_PATH


class Ringbuf(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.POINTER(REAL_T)),
        ("size", ctypes.c_uint32),
        ("head", ctypes.c_uint32),
        ("count", ctypes.c_uint32),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Medfilt(ctypes.Structure):
    _fields_ = [
        ("window", Ringbuf),
        ("sorted", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Matrix(ctypes.Structure):
    _fields_ = [
        ("m", ctypes.c_uint32),
        ("n", ctypes.c_uint32),
        ("elem", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Vector(ctypes.Structure):
    _fields_ = [
        ("size", ctypes.c_uint32),
        ("data", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class CSpline(ctypes.Structure):
    _fields_ = [
        ("size", ctypes.c_uint32),
        ("x", ctypes.POINTER(REAL_T)),
        ("y", ctypes.POINTER(REAL_T)),
        ("b", ctypes.POINTER(REAL_T)),
        ("c", ctypes.POINTER(REAL_T)),
        ("d", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class CSplineMempool(ctypes.Structure):
    _fields_ = [
        ("dx", ctypes.POINTER(REAL_T)),
        ("dp", ctypes.POINTER(REAL_T)),
        ("d", ctypes.POINTER(REAL_T)),
        ("b", ctypes.POINTER(REAL_T)),
        ("q", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class KalmanScratch(ctypes.Structure):
    _fields_ = [(name, Matrix) for name in (
        "nxnx_a", "nxnx_b", "nxnx_c",
        "nxny_a", "nxny_b",
        "nynx_a",
        "nyny_a", "nyny_b", "nyny_c",
        "augmented",
        "nx1_a", "nx1_b",
        "ny1_a", "ny1_b",
    )]


class Kalman(ctypes.Structure):
    _fields_ = [
        ("ni", ctypes.c_uint32),
        ("nx", ctypes.c_uint32),
        ("ny", ctypes.c_uint32),
    ] + [(name, Matrix) for name in
         ("_x", "x", "y", "u", "a", "b", "p", "q", "r", "c", "k")] + [
        ("scratch", KalmanScratch),
        ("mempool", ctypes.POINTER(REAL_T)),
        ("singular", ctypes.c_bool),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Cnum(ctypes.Structure):
    _fields_ = [
        ("re", REAL_T),
        ("im", REAL_T),
    ]


class Fft(ctypes.Structure):
    _fields_ = [
        ("size", ctypes.c_uint32),
        ("twiddle", ctypes.POINTER(Cnum)),
        ("reverse", ctypes.POINTER(ctypes.c_uint32)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Bluestein(ctypes.Structure):
    _fields_ = [
        ("size", ctypes.c_uint32),
        ("fft", Fft),
        ("chirp", ctypes.POINTER(Cnum)),
        ("kernel", ctypes.POINTER(Cnum)),
        ("first", ctypes.POINTER(Cnum)),
        ("second", ctypes.POINTER(Cnum)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Stft(ctypes.Structure):
    _fields_ = [
        ("block", ctypes.c_uint32),
        ("hop", ctypes.c_uint32),
        ("kind", ctypes.c_int),
        ("parameter", REAL_T),
        ("window", ctypes.POINTER(REAL_T)),
        ("windowed", ctypes.POINTER(REAL_T)),
        ("spectrum", ctypes.POINTER(Cnum)),
        ("fft", Fft),
        ("designed", ctypes.c_bool),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Iir(ctypes.Structure):
    _fields_ = [
        ("sections", ctypes.c_uint32),
        ("coefficient", ctypes.POINTER(REAL_T)),
        ("state", ctypes.POINTER(REAL_T)),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Fir(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint32),
        ("coefficient", ctypes.POINTER(REAL_T)),
        ("history", ctypes.POINTER(REAL_T)),
        ("position", ctypes.c_uint32),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Quaternion(ctypes.Structure):
    _fields_ = [
        ("w", REAL_T),
        ("x", REAL_T),
        ("y", REAL_T),
        ("z", REAL_T),
    ]


class PeakdetectOptions(ctypes.Structure):
    _fields_ = [
        ("minimum_height", REAL_T),
        ("minimum_prominence", REAL_T),
        ("minimum_width", REAL_T),
        ("minimum_distance", ctypes.c_uint32),
    ]


FLOAT_POINTER = ctypes.POINTER(REAL_T)

# The enumerations of the library. A test that writes 0 and 1 says nothing
# about what it is asking for; these names say it.
INTERP_LINEAR = 0
INTERP_PCHIP = 1

DETREND_CONSTANT = 0
DETREND_LINEAR = 1

CONVOLVE_FULL = 0
CONVOLVE_SAME = 1
CONVOLVE_VALID = 2

WINDOW_RECTANGULAR = 0
WINDOW_HANN = 1
WINDOW_HAMMING = 2
WINDOW_BLACKMAN = 3
WINDOW_BLACKMAN_HARRIS = 4
WINDOW_TUKEY = 5
WINDOW_KAISER = 6

# The windows that take no parameter, which is what most tests want to walk
# through.
WINDOWS_WITHOUT_A_PARAMETER = (WINDOW_RECTANGULAR, WINDOW_HANN, WINDOW_HAMMING,
                               WINDOW_BLACKMAN, WINDOW_BLACKMAN_HARRIS)

IIR_BUTTERWORTH = 0
IIR_CHEBYSHEV_I = 1
IIR_CHEBYSHEV_II = 2
IIR_ELLIPTIC = 3

IIR_SHAPES = (IIR_BUTTERWORTH, IIR_CHEBYSHEV_I, IIR_CHEBYSHEV_II,
              IIR_ELLIPTIC)

# The shapes that ripple in the band that passes, in the order of how sharply
# they fall.
IIR_SHAPES_BY_SHARPNESS = (IIR_BUTTERWORTH, IIR_CHEBYSHEV_I, IIR_ELLIPTIC)


def load_library():
    """Load the shared object and give the types of every function."""
    library = ctypes.CDLL(build_library())

    # matrix
    library.matrix_alloc.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    library.matrix_alloc.restype = Matrix
    library.matrix_add_element.argtypes = [ctypes.POINTER(Matrix), ctypes.c_uint32,
                                           ctypes.c_uint32, REAL_T]
    library.matrix_add_element.restype = None
    library.matrix_get_element.argtypes = [ctypes.POINTER(Matrix), ctypes.c_uint32,
                                           ctypes.c_uint32]
    library.matrix_get_element.restype = REAL_T
    for name in ("matrix_add", "matrix_subtract", "matrix_multiply"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Matrix), ctypes.POINTER(Matrix)]
        function.restype = Matrix
    library.matrix_multiply_scalar.argtypes = [ctypes.POINTER(Matrix), REAL_T]
    library.matrix_multiply_scalar.restype = Matrix
    for name in ("matrix_transpose", "matrix_inverse"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Matrix)]
        function.restype = Matrix
    library.matrix_determinant.argtypes = [ctypes.POINTER(Matrix)]
    library.matrix_determinant.restype = REAL_T
    library.matrix_trace.argtypes = [ctypes.POINTER(Matrix)]
    library.matrix_trace.restype = REAL_T
    for name in ("matrix_is_equal", "matrix_is_multipliable"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Matrix), ctypes.POINTER(Matrix)]
        function.restype = ctypes.c_bool
    for name in ("matrix_is_square", "matrix_is_zero", "matrix_is_unit"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Matrix)]
        function.restype = ctypes.c_bool
    library.matrix_create_unit_matrix.argtypes = [ctypes.c_uint32]
    library.matrix_create_unit_matrix.restype = Matrix
    library.matrix_create_zero_matrix.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    library.matrix_create_zero_matrix.restype = Matrix
    library.matrix_get_nth_row.argtypes = [ctypes.POINTER(Matrix), ctypes.c_uint32]
    library.matrix_get_nth_row.restype = Matrix
    library.matrix_get_nth_col.argtypes = [ctypes.POINTER(Matrix), ctypes.c_uint32]
    library.matrix_get_nth_col.restype = Matrix
    library.matrix_free.argtypes = [ctypes.POINTER(Matrix)]
    library.matrix_free.restype = None

    # vector
    library.vector_alloc.argtypes = [ctypes.c_uint32]
    library.vector_alloc.restype = Vector
    library.vector_add_point_at_index.argtypes = [ctypes.POINTER(Vector),
                                                  ctypes.c_uint32, REAL_T]
    library.vector_add_point_at_index.restype = None
    library.vector_get.argtypes = [ctypes.POINTER(Vector), ctypes.c_uint32]
    library.vector_get.restype = REAL_T
    library.vector_dot_product.argtypes = [ctypes.POINTER(Vector), ctypes.POINTER(Vector)]
    library.vector_dot_product.restype = REAL_T
    library.vector_norm.argtypes = [ctypes.POINTER(Vector)]
    library.vector_norm.restype = REAL_T
    library.vector_free.argtypes = [ctypes.POINTER(Vector)]
    library.vector_free.restype = None

    # cspline
    library.cspline_alloc.argtypes = [ctypes.c_uint32]
    library.cspline_alloc.restype = CSpline
    library.cspline_alloc_mempool.argtypes = [ctypes.c_uint32]
    library.cspline_alloc_mempool.restype = CSplineMempool
    library.cspline_init.argtypes = [ctypes.POINTER(CSpline), CSplineMempool,
                                     FLOAT_POINTER, FLOAT_POINTER]
    library.cspline_init.restype = None
    library.cspline_get_interpolated_point.argtypes = [ctypes.POINTER(CSpline),
                                                       REAL_T]
    library.cspline_get_interpolated_point.restype = REAL_T
    library.cspline_free.argtypes = [CSpline]
    library.cspline_free.restype = None
    library.cspline_free_mempool.argtypes = [CSplineMempool]
    library.cspline_free_mempool.restype = None

    # kalman
    library.kalman_alloc.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
    library.kalman_alloc.restype = Kalman
    for name in ("kalman_set_state_matrix", "kalman_set_state_transition_matrix",
                 "kalman_set_control_matrix", "kalman_set_covariance_matrix",
                 "kalman_set_process_noise_covariance_matrix",
                 "kalman_set_measurement_covariance_matrix",
                 "kalman_set_observation_matrix", "kalman_set_input_matrix",
                 "kalman_set_measurement_matrix"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Kalman), ctypes.POINTER(Matrix)]
        function.restype = None
    library.kalman_predict.argtypes = [ctypes.POINTER(Kalman)]
    library.kalman_predict.restype = None
    library.kalman_update.argtypes = [ctypes.POINTER(Kalman)]
    library.kalman_update.restype = ctypes.c_bool
    library.kalman_step.argtypes = [ctypes.POINTER(Kalman), ctypes.POINTER(Matrix),
                                    ctypes.POINTER(Matrix)]
    library.kalman_step.restype = ctypes.c_bool
    library.kalman_free.argtypes = [ctypes.POINTER(Kalman)]
    library.kalman_free.restype = None

    # utils
    library.binarysearch_get_index.argtypes = [FLOAT_POINTER, REAL_T,
                                               ctypes.c_uint32]
    library.binarysearch_get_index.restype = ctypes.c_uint32
    for name in ("peakdetect_get_peaks", "valleydetect_get_valley"):
        function = getattr(library, name)
        function.argtypes = [FLOAT_POINTER, FLOAT_POINTER, FLOAT_POINTER,
                             ctypes.c_uint32]
        function.restype = ctypes.c_uint32

    # medfilt
    library.medfilt_alloc.argtypes = [ctypes.c_uint32]
    library.medfilt_alloc.restype = Medfilt
    library.medfilt_process_sample.argtypes = [ctypes.POINTER(Medfilt), REAL_T]
    library.medfilt_process_sample.restype = REAL_T
    library.medfilt_get_median.argtypes = [ctypes.POINTER(Medfilt)]
    library.medfilt_get_median.restype = REAL_T
    library.medfilt_count.argtypes = [ctypes.POINTER(Medfilt)]
    library.medfilt_count.restype = ctypes.c_uint32
    library.medfilt_free.argtypes = [ctypes.POINTER(Medfilt)]
    library.medfilt_free.restype = None

    # stats
    for name in ("stats_sum", "stats_mean", "stats_variance", "stats_deviation",
                 "stats_rms", "stats_min", "stats_max", "stats_median"):
        function = getattr(library, name)
        function.argtypes = [FLOAT_POINTER, ctypes.c_uint32]
        function.restype = REAL_T
    library.stats_percentile.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                         REAL_T]
    library.stats_percentile.restype = REAL_T
    library.stats_mad.argtypes = [FLOAT_POINTER, ctypes.c_uint32, FLOAT_POINTER]
    library.stats_mad.restype = REAL_T

    # interp
    library.interp_is_valid_kind.argtypes = [ctypes.c_int]
    library.interp_is_valid_kind.restype = ctypes.c_bool
    library.interp_is_valid_table.argtypes = [FLOAT_POINTER, ctypes.c_uint32]
    library.interp_is_valid_table.restype = ctypes.c_bool
    library.interp_linear.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                      ctypes.c_uint32, REAL_T]
    library.interp_linear.restype = REAL_T
    library.interp_pchip_slopes.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                            ctypes.c_uint32, FLOAT_POINTER]
    library.interp_pchip_slopes.restype = ctypes.c_bool
    library.interp_pchip.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32, REAL_T]
    library.interp_pchip.restype = REAL_T
    library.interp_block.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32,
                                     ctypes.c_int, FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32]
    library.interp_block.restype = ctypes.c_bool

    # detrend
    library.detrend_is_valid_kind.argtypes = [ctypes.c_int]
    library.detrend_is_valid_kind.restype = ctypes.c_bool
    library.detrend_trend.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                      ctypes.c_int, FLOAT_POINTER,
                                      FLOAT_POINTER]
    library.detrend_trend.restype = ctypes.c_bool
    library.detrend_trend_at.argtypes = [REAL_T, REAL_T, ctypes.c_uint32,
                                         ctypes.c_uint32]
    library.detrend_trend_at.restype = REAL_T
    library.detrend_block.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                      ctypes.c_uint32, ctypes.c_int]
    library.detrend_block.restype = ctypes.c_bool
    library.detrend_remove.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                       ctypes.c_uint32, REAL_T, REAL_T]
    library.detrend_remove.restype = ctypes.c_bool

    # lstsq
    library.lstsq_is_valid_fit.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    library.lstsq_is_valid_fit.restype = ctypes.c_bool
    library.lstsq_polyfit.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                      ctypes.c_uint32, ctypes.c_uint32,
                                      FLOAT_POINTER]
    library.lstsq_polyfit.restype = ctypes.c_bool
    library.lstsq_polyfit_scaled.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                             ctypes.c_uint32, ctypes.c_uint32,
                                             FLOAT_POINTER, FLOAT_POINTER,
                                             FLOAT_POINTER]
    library.lstsq_polyfit_scaled.restype = ctypes.c_bool
    library.lstsq_scaling.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                      FLOAT_POINTER, FLOAT_POINTER]
    library.lstsq_scaling.restype = None
    library.lstsq_evaluate.argtypes = [FLOAT_POINTER, ctypes.c_uint32, REAL_T]
    library.lstsq_evaluate.restype = REAL_T
    library.lstsq_evaluate_scaled.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                              REAL_T, REAL_T, REAL_T]
    library.lstsq_evaluate_scaled.restype = REAL_T
    library.lstsq_fit_quality.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                          ctypes.c_uint32, FLOAT_POINTER,
                                          ctypes.c_uint32]
    library.lstsq_fit_quality.restype = REAL_T
    library.lstsq_fit_quality_scaled.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                                 ctypes.c_uint32,
                                                 FLOAT_POINTER,
                                                 ctypes.c_uint32, REAL_T,
                                                 REAL_T]
    library.lstsq_fit_quality_scaled.restype = REAL_T

    # window
    library.window_is_valid_kind.argtypes = [ctypes.c_int]
    library.window_is_valid_kind.restype = ctypes.c_bool
    library.window_takes_a_parameter.argtypes = [ctypes.c_int]
    library.window_takes_a_parameter.restype = ctypes.c_bool
    library.window_is_valid_size.argtypes = [ctypes.c_uint32, ctypes.c_int]
    library.window_is_valid_size.restype = ctypes.c_bool
    library.window_build.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                     ctypes.c_int]
    library.window_build.restype = None
    library.window_build_with.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                          ctypes.c_int, REAL_T]
    library.window_build_with.restype = None
    library.window_value.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_int, REAL_T]
    library.window_value.restype = REAL_T
    for name in ("window_coherent_gain", "window_noise_gain",
                 "window_noise_bandwidth"):
        function = getattr(library, name)
        function.argtypes = [FLOAT_POINTER, ctypes.c_uint32]
        function.restype = REAL_T
    library.window_apply.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32]
    library.window_apply.restype = None

    # convolve
    library.convolve_is_valid_mode.argtypes = [ctypes.c_int]
    library.convolve_is_valid_mode.restype = ctypes.c_bool
    library.convolve_output_size.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                             ctypes.c_int]
    library.convolve_output_size.restype = ctypes.c_uint32
    library.convolve_direct.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                        FLOAT_POINTER, ctypes.c_uint32,
                                        FLOAT_POINTER, ctypes.c_int]
    library.convolve_direct.restype = ctypes.c_bool
    library.convolve_transform_size.argtypes = [ctypes.c_uint32,
                                                ctypes.c_uint32]
    library.convolve_transform_size.restype = ctypes.c_uint32
    library.convolve_by_transform.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                              FLOAT_POINTER, ctypes.c_uint32,
                                              FLOAT_POINTER, ctypes.c_int,
                                              ctypes.POINTER(Fft),
                                              ctypes.POINTER(Cnum),
                                              ctypes.POINTER(Cnum),
                                              FLOAT_POINTER]
    library.convolve_by_transform.restype = ctypes.c_bool

    # cnum
    library.cnum_make.argtypes = [REAL_T, REAL_T]
    library.cnum_make.restype = Cnum
    library.cnum_magnitude.argtypes = [Cnum]
    library.cnum_magnitude.restype = REAL_T

    # fft
    library.fft_is_valid_size.argtypes = [ctypes.c_uint32]
    library.fft_is_valid_size.restype = ctypes.c_bool
    library.fft_alloc.argtypes = [ctypes.c_uint32]
    library.fft_alloc.restype = Fft
    for name in ("fft_forward", "fft_inverse"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Fft), ctypes.POINTER(Cnum)]
        function.restype = None
    library.fft_forward_real.argtypes = [ctypes.POINTER(Fft), FLOAT_POINTER,
                                         ctypes.POINTER(Cnum)]
    library.fft_forward_real.restype = None
    library.fft_inverse_real.argtypes = [ctypes.POINTER(Fft),
                                         ctypes.POINTER(Cnum), FLOAT_POINTER,
                                         ctypes.POINTER(Cnum)]
    library.fft_inverse_real.restype = None
    library.fft_magnitude.argtypes = [ctypes.POINTER(Cnum), FLOAT_POINTER,
                                      ctypes.c_uint32]
    library.fft_magnitude.restype = None
    library.fft_bin_frequency.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                          REAL_T]
    library.fft_bin_frequency.restype = REAL_T
    library.fft_free.argtypes = [ctypes.POINTER(Fft)]
    library.fft_free.restype = None

    # bluestein
    library.bluestein_is_valid_size.argtypes = [ctypes.c_uint32]
    library.bluestein_is_valid_size.restype = ctypes.c_bool
    library.bluestein_transform_size.argtypes = [ctypes.c_uint32]
    library.bluestein_transform_size.restype = ctypes.c_uint32
    library.bluestein_alloc.argtypes = [ctypes.c_uint32]
    library.bluestein_alloc.restype = Bluestein
    for name in ("bluestein_forward", "bluestein_inverse"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Bluestein), ctypes.POINTER(Cnum)]
        function.restype = None
    library.bluestein_bin_frequency.argtypes = [ctypes.c_uint32,
                                                ctypes.c_uint32, REAL_T]
    library.bluestein_bin_frequency.restype = REAL_T
    library.bluestein_free.argtypes = [ctypes.POINTER(Bluestein)]
    library.bluestein_free.restype = None

    # stft
    library.stft_is_valid_block.argtypes = [ctypes.c_uint32]
    library.stft_is_valid_block.restype = ctypes.c_bool
    library.stft_is_valid_hop.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    library.stft_is_valid_hop.restype = ctypes.c_bool
    library.stft_frame_count.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32]
    library.stft_frame_count.restype = ctypes.c_uint32
    library.stft_signal_size.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32]
    library.stft_signal_size.restype = ctypes.c_uint32
    library.stft_fewest_frames.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    library.stft_fewest_frames.restype = ctypes.c_uint32
    library.stft_alloc.argtypes = [ctypes.c_uint32]
    library.stft_alloc.restype = Stft
    library.stft_design.argtypes = [ctypes.POINTER(Stft), ctypes.c_uint32,
                                    ctypes.c_int, REAL_T]
    library.stft_design.restype = ctypes.c_bool
    library.stft_can_rebuild.argtypes = [ctypes.POINTER(Stft)]
    library.stft_can_rebuild.restype = ctypes.c_bool
    library.stft_solid_range.argtypes = [ctypes.POINTER(Stft),
                                         ctypes.c_uint32,
                                         ctypes.POINTER(ctypes.c_uint32),
                                         ctypes.POINTER(ctypes.c_uint32)]
    library.stft_solid_range.restype = ctypes.c_bool
    library.stft_forward.argtypes = [ctypes.POINTER(Stft), FLOAT_POINTER,
                                     ctypes.c_uint32, ctypes.POINTER(Cnum),
                                     ctypes.c_uint32]
    library.stft_forward.restype = ctypes.c_bool
    library.stft_inverse.argtypes = [ctypes.POINTER(Stft), ctypes.POINTER(Cnum),
                                     ctypes.c_uint32, FLOAT_POINTER,
                                     ctypes.c_uint32, FLOAT_POINTER]
    library.stft_inverse.restype = ctypes.c_bool
    library.stft_bin_frequency.argtypes = [ctypes.POINTER(Stft),
                                           ctypes.c_uint32, REAL_T]
    library.stft_bin_frequency.restype = REAL_T
    library.stft_frame_time.argtypes = [ctypes.POINTER(Stft), ctypes.c_uint32,
                                        REAL_T]
    library.stft_frame_time.restype = REAL_T
    library.stft_free.argtypes = [ctypes.POINTER(Stft)]
    library.stft_free.restype = None

    # iir
    library.iir_is_valid_shape.argtypes = [ctypes.c_int]
    library.iir_is_valid_shape.restype = ctypes.c_bool
    library.iir_is_valid_ripple.argtypes = [REAL_T]
    library.iir_is_valid_ripple.restype = ctypes.c_bool
    library.iir_is_valid_attenuation.argtypes = [REAL_T]
    library.iir_is_valid_attenuation.restype = ctypes.c_bool
    library.iir_alloc.argtypes = [ctypes.c_uint32]
    library.iir_alloc.restype = Iir
    for name in ("iir_design_low_pass_with", "iir_design_high_pass_with"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Iir), REAL_T, ctypes.c_int,
                             REAL_T, REAL_T]
        function.restype = ctypes.c_bool
    library.iir_sections_for.argtypes = [ctypes.c_int, REAL_T, REAL_T, REAL_T,
                                         REAL_T]
    library.iir_sections_for.restype = ctypes.c_uint32
    for name in ("iir_get_gain", "iir_phase", "iir_group_delay"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Iir), REAL_T]
        function.restype = REAL_T
    library.iir_free.argtypes = [ctypes.POINTER(Iir)]
    library.iir_free.restype = None

    # fir
    library.fir_alloc.argtypes = [ctypes.c_uint32]
    library.fir_alloc.restype = Fir
    library.fir_is_valid_cutoff.argtypes = [ctypes.c_uint32, REAL_T]
    library.fir_is_valid_cutoff.restype = ctypes.c_bool
    for name in ("fir_design_low_pass_with", "fir_design_high_pass_with"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Fir), REAL_T, ctypes.c_int, REAL_T]
        function.restype = ctypes.c_bool
    library.fir_design_band_pass_with.argtypes = [ctypes.POINTER(Fir), REAL_T,
                                                  REAL_T, ctypes.c_int,
                                                  REAL_T]
    library.fir_design_band_pass_with.restype = ctypes.c_bool
    library.fir_transition_width.argtypes = [ctypes.c_int, ctypes.c_uint32]
    library.fir_transition_width.restype = REAL_T
    library.fir_length_for.argtypes = [ctypes.c_int, REAL_T]
    library.fir_length_for.restype = ctypes.c_uint32
    library.fir_get_gain.argtypes = [ctypes.POINTER(Fir), REAL_T]
    library.fir_get_gain.restype = REAL_T
    library.fir_get_coefficient.argtypes = [ctypes.POINTER(Fir),
                                            ctypes.c_uint32]
    library.fir_get_coefficient.restype = REAL_T
    library.fir_free.argtypes = [ctypes.POINTER(Fir)]
    library.fir_free.restype = None

    # quaternion
    library.quaternion_make.argtypes = [REAL_T, REAL_T, REAL_T, REAL_T]
    library.quaternion_make.restype = Quaternion
    library.quaternion_identity.argtypes = []
    library.quaternion_identity.restype = Quaternion
    library.quaternion_magnitude.argtypes = [Quaternion]
    library.quaternion_magnitude.restype = REAL_T
    library.quaternion_dot.argtypes = [Quaternion, Quaternion]
    library.quaternion_dot.restype = REAL_T
    library.quaternion_from_axis_angle.argtypes = [REAL_T, REAL_T, REAL_T,
                                                   REAL_T]
    library.quaternion_from_axis_angle.restype = Quaternion
    for name in ("quaternion_normalise", "quaternion_conjugate"):
        function = getattr(library, name)
        function.argtypes = [Quaternion]
        function.restype = Quaternion
    library.quaternion_multiply.argtypes = [Quaternion, Quaternion]
    library.quaternion_multiply.restype = Quaternion
    library.quaternion_is_same_attitude.argtypes = [Quaternion, Quaternion,
                                                    REAL_T]
    library.quaternion_is_same_attitude.restype = ctypes.c_bool
    library.quaternion_rotate.argtypes = [Quaternion, REAL_T, REAL_T, REAL_T,
                                          FLOAT_POINTER, FLOAT_POINTER,
                                          FLOAT_POINTER]
    library.quaternion_rotate.restype = None
    library.quaternion_to_matrix_into.argtypes = [Quaternion,
                                                  ctypes.POINTER(Matrix)]
    library.quaternion_to_matrix_into.restype = None
    library.quaternion_from_matrix.argtypes = [ctypes.POINTER(Matrix)]
    library.quaternion_from_matrix.restype = Quaternion
    library.quaternion_slerp.argtypes = [Quaternion, Quaternion, REAL_T]
    library.quaternion_slerp.restype = Quaternion

    # peakdetect
    library.peakdetect_no_rules.argtypes = []
    library.peakdetect_no_rules.restype = PeakdetectOptions
    library.peakdetect_prominence.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                              ctypes.c_uint32]
    library.peakdetect_prominence.restype = REAL_T
    library.peakdetect_width.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                         ctypes.c_uint32, REAL_T]
    library.peakdetect_width.restype = REAL_T
    library.peakdetect_find.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                        ctypes.POINTER(PeakdetectOptions),
                                        ctypes.POINTER(ctypes.c_uint32),
                                        ctypes.c_uint32]
    library.peakdetect_find.restype = ctypes.c_uint32

    return library


def float_array(values):
    """Give a C array of real_t that holds the given values."""
    return (REAL_T * len(values))(*values)


def real_buffer(size):
    """Give an empty C array of real_t with room for the given number.

    A test that needs somewhere for the library to write must ask for it here
    and not build it itself. A buffer built as ctypes.c_float would be right
    for a 32 bit build of the library and wrong for a 64 bit one, and ctypes
    would refuse it only when the wider build ran.
    """
    return (REAL_T * size)()


def make_matrix(library, rows):
    """Give a matrix that holds the given rows."""
    m = len(rows)
    n = len(rows[0])
    matrix = library.matrix_alloc(m, n)
    for i in range(m):
        for j in range(n):
            library.matrix_add_element(ctypes.byref(matrix), i, j, rows[i][j])
    return matrix


def matrix_rows(library, matrix):
    """Give the elements of a matrix as a list of lists."""
    return [[library.matrix_get_element(ctypes.byref(matrix), i, j)
             for j in range(matrix.n)]
            for i in range(matrix.m)]


def make_vector(library, values):
    vector = library.vector_alloc(len(values))
    for index, value in enumerate(values):
        library.vector_add_point_at_index(ctypes.byref(vector), index, value)
    return vector
