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
    "sptk/linalg/pmatrix.c",
    "sptk/linalg/vector.c",
    "sptk/linalg/vector2d.c",
    "sptk/interpolate/cspline.c",
    "sptk/transform/dwt.c",
    "sptk/transform/fft.c",
    "sptk/transform/goertzel.c",
    "sptk/transform/hht.c",
    "sptk/transform/hilbert.c",
    "sptk/transform/window.c",
    "sptk/transform/correlate.c",
    "sptk/transform/psd.c",
    "sptk/filter/fir.c",
    "sptk/filter/iir.c",
    "sptk/filter/savgol.c",
    "sptk/filter/movavg.c",
    "sptk/filter/medfilt.c",
    "sptk/filter/dcblock.c",
    "sptk/estimate/ekf.c",
    "sptk/estimate/kalman.c",
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


FLOAT_POINTER = ctypes.POINTER(REAL_T)


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
