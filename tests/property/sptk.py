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
    "sptk/linalg/poly.c",
    "sptk/linalg/quaternion.c",
    "sptk/linalg/vector.c",
    "sptk/linalg/vector2d.c",
    "sptk/interpolate/cspline.c",
    "sptk/interpolate/interp.c",
    "sptk/transform/bluestein.c",
    "sptk/transform/cepstrum.c",
    "sptk/transform/dct.c",
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
    "sptk/filter/lattice.c",
    "sptk/filter/rls.c",
    "sptk/filter/filtfilt.c",
    "sptk/filter/farrow.c",
    "sptk/estimate/ekf.c",
    "sptk/estimate/kalman.c",
    "sptk/estimate/propagate.c",
    "sptk/estimate/pll.c",
    "sptk/estimate/ukf.c",
    "sptk/decompose/emd.c",
    "sptk/decompose/imf.c",
    "sptk/util/binarysearch.c",
    "sptk/util/curve.c",
    "sptk/util/generate.c",
    "sptk/util/quantise.c",
    "sptk/util/stats.c",
    "sptk/util/peakdetect.c",
    "sptk/util/valleydetect.c",
    "sptk/detect/matched.c",
    "sptk/detect/delay.c",
    "sptk/detect/changepoint.c",
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


class Rls(ctypes.Structure):
    _fields_ = [
        ("history", Ringbuf),
        ("coefficient", ctypes.POINTER(REAL_T)),
        ("inverse", ctypes.POINTER(REAL_T)),
        ("gain", ctypes.POINTER(REAL_T)),
        ("carried", ctypes.POINTER(REAL_T)),
        ("length", ctypes.c_uint32),
        ("forgetting", REAL_T),
        ("doubt", REAL_T),
        ("healthy", ctypes.c_bool),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Lattice(ctypes.Structure):
    _fields_ = [
        ("reflection", ctypes.POINTER(REAL_T)),
        ("forward", ctypes.POINTER(REAL_T)),
        ("backward", ctypes.POINTER(REAL_T)),
        ("held", ctypes.POINTER(REAL_T)),
        ("energy", ctypes.POINTER(REAL_T)),
        ("weight", ctypes.POINTER(REAL_T)),
        ("stages", ctypes.c_uint32),
        ("rate", REAL_T),
        ("forgetting", REAL_T),
        ("before", REAL_T),
        ("after", REAL_T),
        ("counted", REAL_T),
        ("designed", ctypes.c_bool),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Matched(ctypes.Structure):
    _fields_ = [
        ("pattern", ctypes.POINTER(REAL_T)),
        ("length", ctypes.c_uint32),
        ("root_energy", REAL_T),
        ("designed", ctypes.c_bool),
    ]


DELAY_CORRELATE = 0
DELAY_PHASE = 1

CHANGEPOINT_NONE = 0
CHANGEPOINT_ROSE = 1
CHANGEPOINT_FELL = 2


class Changepoint(ctypes.Structure):
    _fields_ = [
        ("expected", REAL_T),
        ("deviation", REAL_T),
        ("smallest_change", REAL_T),
        ("threshold", REAL_T),
        ("high", REAL_T),
        ("low", REAL_T),
        ("since_high", ctypes.c_uint32),
        ("since_low", ctypes.c_uint32),
        ("counted", ctypes.c_uint32),
        ("designed", ctypes.c_bool),
    ]


class Pll(ctypes.Structure):
    _fields_ = [
        ("phase", REAL_T),
        ("step", REAL_T),
        ("free_step", REAL_T),
        ("fast", REAL_T),
        ("slow", REAL_T),
        ("gathered", REAL_T),
        ("loudness", REAL_T),
        ("quality", REAL_T),
        ("quality_keep", REAL_T),
        ("bandwidth", REAL_T),
        ("damping", REAL_T),
        ("designed", ctypes.c_bool),
    ]


class Farrow(ctypes.Structure):
    _fields_ = [
        ("history", Ringbuf),
        ("weight", ctypes.POINTER(REAL_T)),
        ("working", ctypes.POINTER(REAL_T)),
        ("order", ctypes.c_uint32),
        ("delay", REAL_T),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Cepstrum(ctypes.Structure):
    _fields_ = [
        ("fft", Fft),
        ("work", ctypes.POINTER(Cnum)),
        ("window", ctypes.POINTER(REAL_T)),
        ("windowed", ctypes.POINTER(REAL_T)),
        ("size", ctypes.c_uint32),
        ("dynamic_alloc", ctypes.c_bool),
    ]


class Csd(ctypes.Structure):
    _fields_ = [
        ("block", ctypes.c_uint32),
        ("overlap", ctypes.c_uint32),
        ("kind", ctypes.c_int),
        ("parameter", REAL_T),
        ("window", ctypes.POINTER(REAL_T)),
        ("windowed", ctypes.POINTER(REAL_T)),
        ("first", ctypes.POINTER(Cnum)),
        ("second", ctypes.POINTER(Cnum)),
        ("cross", ctypes.POINTER(Cnum)),
        ("first_power", ctypes.POINTER(REAL_T)),
        ("second_power", ctypes.POINTER(REAL_T)),
        ("fft", Fft),
        ("window_power", REAL_T),
        ("designed", ctypes.c_bool),
        ("dynamic_alloc", ctypes.c_bool),
    ]


CORRELATE_RAW = 0
CORRELATE_BIASED = 1
CORRELATE_UNBIASED = 2
CORRELATE_COEFFICIENT = 3

CORRELATE_SCALINGS = (CORRELATE_RAW, CORRELATE_BIASED, CORRELATE_UNBIASED,
                      CORRELATE_COEFFICIENT)


CURVE_GAUSSIAN = 0
CURVE_LORENTZIAN = 1
CURVE_SKEWED_GAUSSIAN = 2

CURVE_SHAPES = (CURVE_GAUSSIAN, CURVE_LORENTZIAN, CURVE_SKEWED_GAUSSIAN)

# The shapes that are the same either side of their middle.
CURVE_EVEN_SHAPES = (CURVE_GAUSSIAN, CURVE_LORENTZIAN)

# What every shape has fallen to at one width from its middle.
CURVE_AT_ONE_WIDTH = 0.6065306597126334


GENERATE_PINK_PARTS = 7


class Generate(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_int),
        ("phase", REAL_T),
        ("step", REAL_T),
        ("sweep", REAL_T),
        ("last_step", REAL_T),
        ("seed", ctypes.c_uint32),
        ("pink", REAL_T * GENERATE_PINK_PARTS),
        ("part", REAL_T),
        ("running", REAL_T),
        ("last_pink", REAL_T),
        ("spare", REAL_T),
        ("counted", ctypes.c_uint32),
        ("has_spare", ctypes.c_bool),
        ("designed", ctypes.c_bool),
    ]


class Quantise(ctypes.Structure):
    _fields_ = [
        ("way", ctypes.c_int),
        ("step", REAL_T),
        ("reach", REAL_T),
        ("carried", REAL_T),
        ("seed", ctypes.c_uint32),
        ("designed", ctypes.c_bool),
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

# How a model of a rate of change is written, so that a Python function can be
# handed to the propagate module as one.
RATE_FUNCTION = ctypes.CFUNCTYPE(None, REAL_T, FLOAT_POINTER, FLOAT_POINTER,
                                 FLOAT_POINTER, ctypes.c_uint32)

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

GENERATE_SINE = 0
GENERATE_SQUARE = 1
GENERATE_SAWTOOTH = 2
GENERATE_TRIANGLE = 3
GENERATE_WHITE_NOISE = 4
GENERATE_PINK_NOISE = 5
GENERATE_BROWN_NOISE = 6
GENERATE_BLUE_NOISE = 7
GENERATE_GAUSSIAN_NOISE = 8
GENERATE_PULSE = 9
GENERATE_GAUSSIAN_PULSE = 10
GENERATE_IMPULSE = 11

GENERATE_LAST_KIND = GENERATE_IMPULSE

# The shapes that swing either way about nothing and follow a phase.
GENERATE_WAVES = (GENERATE_SINE, GENERATE_SQUARE, GENERATE_SAWTOOTH,
                  GENERATE_TRIANGLE, GENERATE_PULSE)

# Every kind that follows a phase, which is every kind but the noises.
GENERATE_PHASED = GENERATE_WAVES + (GENERATE_GAUSSIAN_PULSE,
                                    GENERATE_IMPULSE)

GENERATE_NOISES = (GENERATE_WHITE_NOISE, GENERATE_PINK_NOISE,
                   GENERATE_BROWN_NOISE, GENERATE_BLUE_NOISE,
                   GENERATE_GAUSSIAN_NOISE)

# Every kind, which is what a rule about all of them is given.
GENERATE_KINDS = GENERATE_PHASED + GENERATE_NOISES

# The kinds held inside the range of one. The gaussian noise runs as far as its
# tails go, the brown noise is a walk with no bound, and the blue noise is a
# difference and reaches further than what it is taken of.
GENERATE_BOUNDED = GENERATE_PHASED + (GENERATE_WHITE_NOISE,
                                      GENERATE_PINK_NOISE)

QUANTISE_PLAIN = 0
QUANTISE_DITHER = 1
QUANTISE_SHAPED = 2

QUANTISE_WAYS = (QUANTISE_PLAIN, QUANTISE_DITHER, QUANTISE_SHAPED)

PROPAGATE_EULER = 0
PROPAGATE_MIDPOINT = 1
PROPAGATE_RUNGE = 2

PROPAGATE_METHODS = (PROPAGATE_EULER, PROPAGATE_MIDPOINT, PROPAGATE_RUNGE)


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

    # eigen
    library.eigen_is_valid_matrix.argtypes = [ctypes.POINTER(Matrix)]
    library.eigen_is_valid_matrix.restype = ctypes.c_bool
    library.eigen_solve.argtypes = [ctypes.POINTER(Matrix), FLOAT_POINTER,
                                    ctypes.POINTER(Matrix)]
    library.eigen_solve.restype = ctypes.c_bool
    library.eigen_condition.argtypes = [FLOAT_POINTER, ctypes.c_uint32]
    library.eigen_condition.restype = REAL_T
    library.eigen_rank.argtypes = [FLOAT_POINTER, ctypes.c_uint32, REAL_T]
    library.eigen_rank.restype = ctypes.c_uint32
    library.eigen_part_held.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                        ctypes.c_uint32]
    library.eigen_part_held.restype = REAL_T

    # poly
    library.poly_is_valid_order.argtypes = [ctypes.c_uint32]
    library.poly_is_valid_order.restype = ctypes.c_bool
    library.poly_evaluate.argtypes = [FLOAT_POINTER, ctypes.c_uint32, REAL_T]
    library.poly_evaluate.restype = REAL_T
    library.poly_evaluate_complex.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                              Cnum]
    library.poly_evaluate_complex.restype = Cnum
    library.poly_multiply.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                      FLOAT_POINTER, ctypes.c_uint32,
                                      FLOAT_POINTER, ctypes.c_uint32]
    library.poly_multiply.restype = ctypes.c_bool
    library.poly_derivative.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                        FLOAT_POINTER]
    library.poly_derivative.restype = ctypes.c_bool
    library.poly_roots.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                   ctypes.POINTER(Cnum)]
    library.poly_roots.restype = ctypes.c_bool
    library.poly_is_inside_circle.argtypes = [FLOAT_POINTER, ctypes.c_uint32]
    library.poly_is_inside_circle.restype = ctypes.c_bool

    # rls
    library.rls_is_valid_forgetting.argtypes = [REAL_T]
    library.rls_is_valid_forgetting.restype = ctypes.c_bool
    library.rls_alloc.argtypes = [ctypes.c_uint32]
    library.rls_alloc.restype = Rls
    library.rls_design.argtypes = [ctypes.POINTER(Rls), REAL_T, REAL_T]
    library.rls_design.restype = ctypes.c_bool
    for name in ("rls_process_sample", "rls_error"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Rls), REAL_T, REAL_T]
        function.restype = REAL_T
    library.rls_is_healthy.argtypes = [ctypes.POINTER(Rls)]
    library.rls_is_healthy.restype = ctypes.c_bool
    library.rls_get_coefficient.argtypes = [ctypes.POINTER(Rls),
                                            ctypes.c_uint32]
    library.rls_get_coefficient.restype = REAL_T
    library.rls_reset.argtypes = [ctypes.POINTER(Rls)]
    library.rls_reset.restype = None
    library.rls_free.argtypes = [ctypes.POINTER(Rls)]
    library.rls_free.restype = None

    # lattice
    library.lattice_is_valid_rate.argtypes = [REAL_T]
    library.lattice_is_valid_rate.restype = ctypes.c_bool
    library.lattice_is_valid_forgetting.argtypes = [REAL_T]
    library.lattice_is_valid_forgetting.restype = ctypes.c_bool
    library.lattice_alloc.argtypes = [ctypes.c_uint32]
    library.lattice_alloc.restype = Lattice
    library.lattice_design.argtypes = [ctypes.POINTER(Lattice), REAL_T, REAL_T]
    library.lattice_design.restype = ctypes.c_bool
    library.lattice_process_sample.argtypes = [ctypes.POINTER(Lattice), REAL_T,
                                               REAL_T]
    library.lattice_process_sample.restype = REAL_T
    for name in ("lattice_error_before", "lattice_error_after"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Lattice)]
        function.restype = REAL_T
    library.lattice_get_reflection.argtypes = [ctypes.POINTER(Lattice),
                                               ctypes.c_uint32]
    library.lattice_get_reflection.restype = REAL_T
    library.lattice_reset.argtypes = [ctypes.POINTER(Lattice)]
    library.lattice_reset.restype = None
    library.lattice_free.argtypes = [ctypes.POINTER(Lattice)]
    library.lattice_free.restype = None

    # generate
    library.generate_is_valid_kind.argtypes = [ctypes.c_int]
    library.generate_is_valid_kind.restype = ctypes.c_bool
    library.generate_is_valid_frequency.argtypes = [REAL_T, REAL_T]
    library.generate_is_valid_frequency.restype = ctypes.c_bool
    library.generate_make.argtypes = [ctypes.c_int]
    library.generate_make.restype = Generate
    library.generate_design.argtypes = [ctypes.POINTER(Generate), REAL_T,
                                        REAL_T]
    library.generate_design.restype = ctypes.c_bool
    library.generate_design_sweep.argtypes = [ctypes.POINTER(Generate), REAL_T,
                                              REAL_T, REAL_T, ctypes.c_uint32]
    library.generate_design_sweep.restype = ctypes.c_bool
    library.generate_is_valid_part.argtypes = [REAL_T]
    library.generate_is_valid_part.restype = ctypes.c_bool
    library.generate_set_part.argtypes = [ctypes.POINTER(Generate), REAL_T]
    library.generate_set_part.restype = ctypes.c_bool
    library.generate_get_part.argtypes = [ctypes.POINTER(Generate)]
    library.generate_get_part.restype = REAL_T
    library.generate_set_seed.argtypes = [ctypes.POINTER(Generate),
                                          ctypes.c_uint32]
    library.generate_set_seed.restype = None
    library.generate_sample.argtypes = [ctypes.POINTER(Generate)]
    library.generate_sample.restype = REAL_T
    library.generate_block.argtypes = [ctypes.POINTER(Generate), FLOAT_POINTER,
                                       ctypes.c_uint32]
    library.generate_block.restype = ctypes.c_bool
    library.generate_reset.argtypes = [ctypes.POINTER(Generate)]
    library.generate_reset.restype = None
    library.generate_get_phase.argtypes = [ctypes.POINTER(Generate)]
    library.generate_get_phase.restype = REAL_T
    library.generate_set_phase.argtypes = [ctypes.POINTER(Generate), REAL_T]
    library.generate_set_phase.restype = None

    # quantise
    library.quantise_is_valid_way.argtypes = [ctypes.c_int]
    library.quantise_is_valid_way.restype = ctypes.c_bool
    library.quantise_is_valid_bits.argtypes = [ctypes.c_uint32]
    library.quantise_is_valid_bits.restype = ctypes.c_bool
    library.quantise_make.argtypes = []
    library.quantise_make.restype = Quantise
    library.quantise_design.argtypes = [ctypes.POINTER(Quantise), ctypes.c_int,
                                        ctypes.c_uint32, REAL_T]
    library.quantise_design.restype = ctypes.c_bool
    library.quantise_set_seed.argtypes = [ctypes.POINTER(Quantise),
                                          ctypes.c_uint32]
    library.quantise_set_seed.restype = None
    library.quantise_sample.argtypes = [ctypes.POINTER(Quantise), REAL_T]
    library.quantise_sample.restype = REAL_T
    library.quantise_block.argtypes = [ctypes.POINTER(Quantise), FLOAT_POINTER,
                                       FLOAT_POINTER, ctypes.c_uint32]
    library.quantise_block.restype = ctypes.c_bool
    library.quantise_step_of.argtypes = [ctypes.POINTER(Quantise)]
    library.quantise_step_of.restype = REAL_T
    library.quantise_noise_floor.argtypes = [ctypes.c_uint32]
    library.quantise_noise_floor.restype = REAL_T
    library.quantise_reset.argtypes = [ctypes.POINTER(Quantise)]
    library.quantise_reset.restype = None

    # propagate
    library.propagate_is_valid_method.argtypes = [ctypes.c_int]
    library.propagate_is_valid_method.restype = ctypes.c_bool
    library.propagate_is_valid_count.argtypes = [ctypes.c_uint32]
    library.propagate_is_valid_count.restype = ctypes.c_bool
    library.propagate_asks_for_each_step.argtypes = [ctypes.c_int]
    library.propagate_asks_for_each_step.restype = ctypes.c_uint32
    library.propagate_state.argtypes = [ctypes.c_int, RATE_FUNCTION, REAL_T,
                                        REAL_T, FLOAT_POINTER, FLOAT_POINTER,
                                        ctypes.c_uint32]
    library.propagate_state.restype = ctypes.c_bool
    library.propagate_state_over.argtypes = [ctypes.c_int, RATE_FUNCTION,
                                             REAL_T, REAL_T, ctypes.c_uint32,
                                             FLOAT_POINTER, FLOAT_POINTER,
                                             ctypes.c_uint32]
    library.propagate_state_over.restype = ctypes.c_bool

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

    # dct
    library.dct_is_valid_size.argtypes = [ctypes.c_uint32]
    library.dct_is_valid_size.restype = ctypes.c_bool
    for name in ("dct_forward", "dct_inverse"):
        function = getattr(library, name)
        function.argtypes = [FLOAT_POINTER, FLOAT_POINTER, ctypes.c_uint32]
        function.restype = ctypes.c_bool
    library.dct_count_for_share.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                            REAL_T]
    library.dct_count_for_share.restype = ctypes.c_uint32

    # pll
    for name in ("pll_is_valid_bandwidth", "pll_is_valid_damping"):
        function = getattr(library, name)
        function.argtypes = [REAL_T]
        function.restype = ctypes.c_bool
    library.pll_make.argtypes = []
    library.pll_make.restype = Pll
    library.pll_design.argtypes = [ctypes.POINTER(Pll), REAL_T, REAL_T,
                                   REAL_T, REAL_T]
    library.pll_design.restype = ctypes.c_bool
    library.pll_process_sample.argtypes = [ctypes.POINTER(Pll), REAL_T]
    library.pll_process_sample.restype = REAL_T
    library.pll_get_frequency.argtypes = [ctypes.POINTER(Pll), REAL_T]
    library.pll_get_frequency.restype = REAL_T
    for name in ("pll_get_phase", "pll_lock_quality", "pll_pull_range"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Pll)]
        function.restype = REAL_T
    library.pll_settle_samples.argtypes = [ctypes.POINTER(Pll)]
    library.pll_settle_samples.restype = ctypes.c_uint32
    library.pll_reset.argtypes = [ctypes.POINTER(Pll)]
    library.pll_reset.restype = None

    # farrow
    library.farrow_is_valid_order.argtypes = [ctypes.c_uint32]
    library.farrow_is_valid_order.restype = ctypes.c_bool
    for name in ("farrow_smallest_delay", "farrow_largest_delay"):
        function = getattr(library, name)
        function.argtypes = [ctypes.c_uint32]
        function.restype = REAL_T
    library.farrow_is_valid_delay.argtypes = [ctypes.POINTER(Farrow), REAL_T]
    library.farrow_is_valid_delay.restype = ctypes.c_bool
    library.farrow_alloc.argtypes = [ctypes.c_uint32]
    library.farrow_alloc.restype = Farrow
    library.farrow_set_delay.argtypes = [ctypes.POINTER(Farrow), REAL_T]
    library.farrow_set_delay.restype = ctypes.c_bool
    library.farrow_get_delay.argtypes = [ctypes.POINTER(Farrow)]
    library.farrow_get_delay.restype = REAL_T
    library.farrow_process_sample.argtypes = [ctypes.POINTER(Farrow), REAL_T]
    library.farrow_process_sample.restype = REAL_T
    library.farrow_process_block.argtypes = [ctypes.POINTER(Farrow),
                                             FLOAT_POINTER, FLOAT_POINTER,
                                             ctypes.c_uint32]
    library.farrow_process_block.restype = ctypes.c_bool
    library.farrow_reset.argtypes = [ctypes.POINTER(Farrow)]
    library.farrow_reset.restype = None
    library.farrow_free.argtypes = [ctypes.POINTER(Farrow)]
    library.farrow_free.restype = None

    # cepstrum
    library.cepstrum_is_valid_size.argtypes = [ctypes.c_uint32]
    library.cepstrum_is_valid_size.restype = ctypes.c_bool
    library.cepstrum_alloc.argtypes = [ctypes.c_uint32]
    library.cepstrum_alloc.restype = Cepstrum
    library.cepstrum_real.argtypes = [ctypes.POINTER(Cepstrum), FLOAT_POINTER,
                                      FLOAT_POINTER]
    library.cepstrum_real.restype = ctypes.c_bool
    library.cepstrum_best_quefrency.argtypes = [FLOAT_POINTER,
                                                ctypes.c_uint32,
                                                ctypes.c_uint32,
                                                ctypes.c_uint32,
                                                FLOAT_POINTER]
    library.cepstrum_best_quefrency.restype = ctypes.c_uint32
    library.cepstrum_free.argtypes = [ctypes.POINTER(Cepstrum)]
    library.cepstrum_free.restype = None

    # correlate
    library.correlate_is_valid_scaling.argtypes = [ctypes.c_int]
    library.correlate_is_valid_scaling.restype = ctypes.c_bool
    library.correlate_auto.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                       FLOAT_POINTER, ctypes.c_uint32,
                                       ctypes.c_int]
    library.correlate_auto.restype = ctypes.c_bool
    library.correlate_cross.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                        ctypes.c_uint32, FLOAT_POINTER,
                                        ctypes.c_uint32, ctypes.c_int]
    library.correlate_cross.restype = ctypes.c_bool
    library.correlate_best_lag.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                           FLOAT_POINTER, ctypes.c_uint32,
                                           ctypes.c_uint32, FLOAT_POINTER]
    library.correlate_best_lag.restype = ctypes.c_uint32
    library.correlate_transform_size.argtypes = [ctypes.c_uint32]
    library.correlate_transform_size.restype = ctypes.c_uint32
    library.correlate_auto_by_transform.argtypes = [
        FLOAT_POINTER, ctypes.c_uint32, FLOAT_POINTER, ctypes.c_uint32,
        ctypes.c_int, ctypes.POINTER(Fft), ctypes.POINTER(Cnum),
        FLOAT_POINTER]
    library.correlate_auto_by_transform.restype = ctypes.c_bool

    # csd
    library.csd_is_valid_block.argtypes = [ctypes.c_uint32]
    library.csd_is_valid_block.restype = ctypes.c_bool
    library.csd_alloc.argtypes = [ctypes.c_uint32]
    library.csd_alloc.restype = Csd
    library.csd_design.argtypes = [ctypes.POINTER(Csd), ctypes.c_uint32,
                                   ctypes.c_int, REAL_T]
    library.csd_design.restype = ctypes.c_bool
    library.csd_block_count.argtypes = [ctypes.POINTER(Csd), ctypes.c_uint32]
    library.csd_block_count.restype = ctypes.c_uint32
    library.csd_bin_frequency.argtypes = [ctypes.POINTER(Csd),
                                          ctypes.c_uint32, REAL_T]
    library.csd_bin_frequency.restype = REAL_T
    library.csd_estimate.argtypes = [ctypes.POINTER(Csd), FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32, REAL_T,
                                     ctypes.POINTER(Cnum)]
    library.csd_estimate.restype = ctypes.c_bool
    library.csd_coherence.argtypes = [ctypes.POINTER(Csd), FLOAT_POINTER,
                                      FLOAT_POINTER, ctypes.c_uint32,
                                      FLOAT_POINTER]
    library.csd_coherence.restype = ctypes.c_bool
    library.csd_transfer.argtypes = [ctypes.POINTER(Csd), FLOAT_POINTER,
                                     FLOAT_POINTER, ctypes.c_uint32,
                                     ctypes.POINTER(Cnum)]
    library.csd_transfer.restype = ctypes.c_bool
    library.csd_free.argtypes = [ctypes.POINTER(Csd)]
    library.csd_free.restype = None

    # curve
    library.curve_is_valid_width.argtypes = [REAL_T]
    library.curve_is_valid_width.restype = ctypes.c_bool
    library.curve_is_valid_shape.argtypes = [ctypes.c_int]
    library.curve_is_valid_shape.restype = ctypes.c_bool
    for name in ("curve_gaussian", "curve_lorentzian"):
        function = getattr(library, name)
        function.argtypes = [REAL_T, REAL_T, REAL_T]
        function.restype = REAL_T
    library.curve_skewed_gaussian.argtypes = [REAL_T, REAL_T, REAL_T, REAL_T]
    library.curve_skewed_gaussian.restype = REAL_T
    library.curve_skewed_gaussian_top.argtypes = [REAL_T, REAL_T, REAL_T]
    library.curve_skewed_gaussian_top.restype = REAL_T
    library.curve_value.argtypes = [ctypes.c_int, REAL_T, REAL_T, REAL_T,
                                    REAL_T]
    library.curve_value.restype = REAL_T
    library.curve_block.argtypes = [ctypes.c_int, REAL_T, REAL_T, REAL_T,
                                    REAL_T, REAL_T, FLOAT_POINTER,
                                    ctypes.c_uint32]
    library.curve_block.restype = ctypes.c_bool

    # matched
    library.matched_is_valid_length.argtypes = [ctypes.c_uint32]
    library.matched_is_valid_length.restype = ctypes.c_bool
    library.matched_make.argtypes = []
    library.matched_make.restype = Matched
    library.matched_design.argtypes = [ctypes.POINTER(Matched), FLOAT_POINTER,
                                       ctypes.c_uint32]
    library.matched_design.restype = ctypes.c_bool
    library.matched_score_at.argtypes = [ctypes.POINTER(Matched),
                                         FLOAT_POINTER]
    library.matched_score_at.restype = REAL_T
    library.matched_score_block.argtypes = [ctypes.POINTER(Matched),
                                            FLOAT_POINTER, ctypes.c_uint32,
                                            FLOAT_POINTER]
    library.matched_score_block.restype = ctypes.c_bool
    library.matched_best.argtypes = [ctypes.POINTER(Matched), FLOAT_POINTER,
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     FLOAT_POINTER]
    library.matched_best.restype = ctypes.c_bool
    library.matched_threshold_for.argtypes = [REAL_T, ctypes.c_uint32]
    library.matched_threshold_for.restype = REAL_T

    # delay
    library.delay_is_valid_way.argtypes = [ctypes.c_int]
    library.delay_is_valid_way.restype = ctypes.c_bool
    library.delay_refine_peak.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                          ctypes.c_uint32]
    library.delay_refine_peak.restype = REAL_T
    library.delay_by_correlation.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                             ctypes.c_uint32, ctypes.c_uint32,
                                             FLOAT_POINTER, FLOAT_POINTER,
                                             FLOAT_POINTER]
    library.delay_by_correlation.restype = ctypes.c_bool
    library.delay_by_phase.argtypes = [FLOAT_POINTER, FLOAT_POINTER,
                                       ctypes.c_uint32, ctypes.POINTER(Fft),
                                       ctypes.POINTER(Cnum),
                                       ctypes.POINTER(Cnum), FLOAT_POINTER]
    library.delay_by_phase.restype = ctypes.c_bool

    # changepoint
    for name in ("changepoint_is_valid_deviation",
                 "changepoint_is_valid_change",
                 "changepoint_is_valid_threshold"):
        function = getattr(library, name)
        function.argtypes = [REAL_T]
        function.restype = ctypes.c_bool
    library.changepoint_make.argtypes = []
    library.changepoint_make.restype = Changepoint
    library.changepoint_design.argtypes = [ctypes.POINTER(Changepoint), REAL_T,
                                           REAL_T, REAL_T, REAL_T]
    library.changepoint_design.restype = ctypes.c_bool
    library.changepoint_process_sample.argtypes = [
        ctypes.POINTER(Changepoint), REAL_T]
    library.changepoint_process_sample.restype = ctypes.c_int
    library.changepoint_began_ago.argtypes = [ctypes.POINTER(Changepoint)]
    library.changepoint_began_ago.restype = ctypes.c_uint32
    for name in ("changepoint_running_high", "changepoint_running_low"):
        function = getattr(library, name)
        function.argtypes = [ctypes.POINTER(Changepoint)]
        function.restype = REAL_T
    library.changepoint_delay_for.argtypes = [ctypes.POINTER(Changepoint),
                                              REAL_T]
    library.changepoint_delay_for.restype = REAL_T
    library.changepoint_reset.argtypes = [ctypes.POINTER(Changepoint)]
    library.changepoint_reset.restype = None

    # peakdetect
    library.peakdetect_no_rules.argtypes = []
    library.peakdetect_no_rules.restype = PeakdetectOptions
    library.peakdetect_prominence.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                              ctypes.c_uint32]
    library.peakdetect_prominence.restype = REAL_T
    library.peakdetect_width.argtypes = [FLOAT_POINTER, ctypes.c_uint32,
                                         ctypes.c_uint32, REAL_T]
    library.peakdetect_width.restype = REAL_T
    for name in ("peakdetect_refine", "peakdetect_refine_height"):
        function = getattr(library, name)
        function.argtypes = [FLOAT_POINTER, ctypes.c_uint32, ctypes.c_uint32]
        function.restype = REAL_T
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
