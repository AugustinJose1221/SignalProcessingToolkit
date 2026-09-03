// Three small jobs that need more than a matrix of plain numbers.
//
// The matrix example shows what the matrix module does. This one shows the
// four modules that stand beside it, and why each exists.
//
//   vector2d and point2d  a pair of numbers that means a place or a direction
//   pmatrix               a matrix whose elements are FUNCTIONS
//   cmatrix and cnum      a matrix of complex numbers
//   callback              where a module writes to, on a target with no console
//
// TO PORT THIS: each of the three jobs stands on its own. Take the one that
// looks like your work.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_LINALG_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/core/point2d.h>
#include <ffitt/core/callback.h>
#include <ffitt/linalg/matrix.h>
#include <ffitt/linalg/pmatrix.h>
#include <ffitt/linalg/cmatrix.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/linalg/vector.h>
#include <ffitt/linalg/vector2d.h>
#include <ffitt/linalg/eigen.h>
#include <ffitt/linalg/poly.h>
#include <math.h>
#include <stdio.h>

#define PI      REAL_C(3.14159265358979323846)

// ---------------------------------------------------------------------------
// Where a module writes to.
//
// Every module that writes takes a function of the type print_t. printf has
// that type, thus a caller can hand printf straight over. A target with no
// console hands over its own function instead, and nothing else changes.
//
// This one puts a mark in front of every line, to show that the module really
// is writing through it.
// ---------------------------------------------------------------------------
static int through_the_serial_port(const char* format, ...)
{
    // A real one would write to a port. This one only shows that the module
    // is writing through the caller's function and not to stdout directly.
    (void)format;

    return 0;
}

// A rotation matrix needs the negative of a sine, and the standard library has
// no such function. It must therefore be written here, and it must take and
// give a real_t so that it agrees with the width of the build.
static real_t negative_sine(real_t x)
{
    return -real_sin(x);
}

// ---------------------------------------------------------------------------
// A rotating joint of a robot arm.
//
// The arm turns, and the tool at its end moves. Where the tool is depends on
// the angle, thus the matrix that says where it is has elements that are
// FUNCTIONS OF THE ANGLE and not numbers. That is what pmatrix holds.
//
// Without it a caller writes the four sines and cosines out by hand at every
// angle, and every place that does so is a place to get a sign wrong.
// ---------------------------------------------------------------------------
static void the_rotating_joint(void)
{
    printf("A rotating joint of a robot arm\n");
    printf("-------------------------------\n");

    // The tool sits 0.30 m along the arm and 0.10 m across it.
    vector_t tool = vector2d_alloc();
    real_t at_rest[2] = {REAL_C(0.30), REAL_C(0.10)};
    vector2d_add_from_array(&tool, at_rest);

    printf("  The tool sits at (%.2f, %.2f) with the arm at rest, which is\n",
           vector2d_get(&tool, 0), vector2d_get(&tool, 1));
    printf("  %.3f m from the joint.\n\n", vector2d_norm(&tool));

    // A matrix whose elements are functions. The functions must take and give
    // a real_t: real_cos and not cosf, because the width of a number is a
    // choice of the build and sinf would not agree with it at 64 bits.
    pmatrix_t rotation = pmatrix_alloc(2, 2);
    pmatrix_add_element(&rotation, 0, 0, real_cos);
    pmatrix_add_element(&rotation, 0, 1, negative_sine);
    pmatrix_add_element(&rotation, 1, 0, real_sin);
    pmatrix_add_element(&rotation, 1, 1, real_cos);

    matrix_t place = matrix_create_zero_matrix(2, 1);
    matrix_add_element(&place, 0, 0, vector2d_get(&tool, 0));
    matrix_add_element(&place, 1, 0, vector2d_get(&tool, 1));

    printf("  %8s %10s %10s %10s\n", "angle", "x", "y", "distance");

    for(uint32_t step = 0; step <= 4u; step++)
    {
        real_t angle = (PI / REAL_C(2.0)) * ((real_t)step / REAL_C(4.0));

        // One call gives the whole matrix at this angle.
        matrix_t at_angle = pmatrix_evaluate(&rotation, angle);
        matrix_t moved = matrix_multiply(&at_angle, &place);

        point2d_t where;
        where.x = matrix_get_element(&moved, 0, 0);
        where.y = matrix_get_element(&moved, 1, 0);

        printf("  %6.0f deg %10.3f %10.3f %10.3f\n",
               angle * REAL_C(180.0) / PI, where.x, where.y,
               REAL_SQRT((where.x * where.x) + (where.y * where.y)));

        matrix_free(&at_angle);
        matrix_free(&moved);
    }

    printf("\n  The distance from the joint never changes, which is what a\n");
    printf("  rotation must do and a good check that the matrix is right.\n\n");

    vector_free(&tool);
    matrix_free(&place);
    pmatrix_free(&rotation);
}

// ---------------------------------------------------------------------------
// Two coils that share a magnetic path.
//
// At one frequency each coil has a resistance and a reactance, thus its
// impedance is one complex number. The two coils are coupled, thus the whole
// thing is a 2 by 2 matrix of complex numbers, and finding the currents from
// the voltages means inverting it.
//
// This cannot be done with a matrix of plain numbers. The phase between the
// voltage and the current IS the imaginary part, and throwing it away throws
// away the answer.
// ---------------------------------------------------------------------------
static void the_coupled_coils(void)
{
    printf("Two coils that share a magnetic path\n");
    printf("------------------------------------\n");

    // Each coil: 10 ohms of resistance and 30 ohms of reactance. The coupling
    // between them is 12 ohms of reactance and no resistance.
    cmatrix_t impedance = cmatrix_alloc(2, 2);
    cmatrix_add_element(&impedance, 0, 0, cnum_make(REAL_C(10.0), REAL_C(30.0)));
    cmatrix_add_element(&impedance, 0, 1, cnum_make(REAL_C(0.0), REAL_C(12.0)));
    cmatrix_add_element(&impedance, 1, 0, cnum_make(REAL_C(0.0), REAL_C(12.0)));
    cmatrix_add_element(&impedance, 1, 1, cnum_make(REAL_C(10.0), REAL_C(30.0)));

    // The matrix is symmetric, because what the first coil does to the second
    // is what the second does to the first. It is NOT hermitian, and the
    // difference is worth knowing: a hermitian matrix has a real diagonal, and
    // the diagonal here holds the resistance and the reactance of each coil
    // together. A coil with no resistance would give a hermitian matrix, and a
    // coil with no resistance does not exist.
    cmatrix_t turned = cmatrix_transpose(&impedance);

    printf("  symmetric, as a pair of coils must be : %s\n",
           cmatrix_is_equal(&impedance, &turned) ? "yes" : "no");
    printf("  hermitian                             : %s\n",
           cmatrix_is_hermitian(&impedance) ? "yes" : "no");
    printf("  The second is no, because a hermitian matrix has a real\n");
    printf("  diagonal and these coils have resistance as well as reactance.\n\n");

    cmatrix_free(&turned);

    // 24 volts on the first coil and nothing on the second.
    cmatrix_t voltage = cmatrix_create_zero_matrix(2, 1);
    cmatrix_add_element(&voltage, 0, 0, cnum_make(REAL_C(24.0), REAL_C(0.0)));

    cmatrix_t admittance = cmatrix_inverse(&impedance);
    cmatrix_t current = cmatrix_multiply(&admittance, &voltage);

    for(uint32_t coil = 0; coil < 2u; coil++)
    {
        cnum_t value = cmatrix_get_element(&current, coil, 0);
        real_t size_of = cnum_magnitude(value);
        real_t phase = REAL_ATAN2(cnum_imaginary(value), cnum_real(value))
                       * REAL_C(180.0) / PI;

        printf("  coil %u carries %.4f A at %7.2f degrees\n", coil + 1u,
               size_of, phase);
    }

    printf("\n  The second coil carries a current although no voltage is put\n");
    printf("  on it, because the first one drives it through the shared path.\n");
    printf("  The phase is the whole of the answer, and a matrix of plain\n");
    printf("  numbers holds no phase at all.\n\n");

    cmatrix_free(&impedance);
    cmatrix_free(&voltage);
    cmatrix_free(&admittance);
    cmatrix_free(&current);
}

// ---------------------------------------------------------------------------
// How alike are two readings.
//
// Two runs of the same machine give two lists of numbers. The question is
// whether they are the same shape, which is not the same as whether they hold
// the same numbers: a run that is twice as loud is the same shape.
//
// The angle between the two, taken as vectors, answers that. It does not
// change when either is made larger.
// ---------------------------------------------------------------------------
static void how_alike_are_two_readings(void)
{
    printf("How alike are two readings\n");
    printf("--------------------------\n");

    real_t first_run[6] = {REAL_C(1.0), REAL_C(3.0), REAL_C(5.0),
                           REAL_C(4.0), REAL_C(2.0), REAL_C(1.0)};
    real_t twice_as_loud[6] = {REAL_C(2.0), REAL_C(6.0), REAL_C(10.0),
                               REAL_C(8.0), REAL_C(4.0), REAL_C(2.0)};
    real_t a_different_shape[6] = {REAL_C(5.0), REAL_C(1.0), REAL_C(2.0),
                                   REAL_C(1.0), REAL_C(5.0), REAL_C(3.0)};

    vector_t a = vector_alloc(6);
    vector_t b = vector_alloc(6);
    vector_t c = vector_alloc(6);

    vector_add_from_array(&a, 6u, first_run);
    vector_add_from_array(&b, 6u, twice_as_loud);
    vector_add_from_array(&c, 6u, a_different_shape);

    real_t alike = vector_dot_product(&a, &b)
                   / (vector_norm(&a) * vector_norm(&b));
    real_t different = vector_dot_product(&a, &c)
                       / (vector_norm(&a) * vector_norm(&c));

    printf("  the first run is %.3f long\n", vector_norm(&a));
    printf("  the same run twice as loud is %.3f long\n", vector_norm(&b));
    printf("\n  first against twice as loud : %.4f\n", alike);
    printf("  first against another shape : %.4f\n", different);
    printf("\n  The first pair are the same shape however loud they are.\n\n");

    // Every module that writes takes the caller's function. Giving one that
    // writes nowhere shows that the module really uses it.
    print_t where_to_write = through_the_serial_port;
    printf("  Writing the vector through a function of the caller's:\n");
    vector_printf(&a, where_to_write);
    printf("  (nothing appeared, because that function writes to a port that\n");
    printf("  is not here. Give printf instead and the vector appears.)\n");
    printf("  With printf, the same call gives:\n");
    vector_printf(&a, printf);

    vector_free(&a);
    vector_free(&b);
    vector_free(&c);
}


// ---------------------------------------------------------------------------
// Which way does the structure give, and by how much more.
//
// Two accelerometers on a machine frame, at right angles. Under running the
// frame wobbles, and the wobble is not the same size in every direction: the
// frame is stiff one way and soft another, and the soft way is where a crack
// will start.
//
// The covariance between the two readings holds that, and its eigenvectors are
// the two directions themselves: the largest eigenvalue is the soft direction,
// where the frame moves most, and the smallest is the stiff one. The ratio of
// the two says how much more one gives than the other, which is what
// eigen_condition answers.
//
// A number near 1 means the frame gives equally in every direction. A large
// number means one direction is far softer, and that direction is where to
// look.
// ---------------------------------------------------------------------------
static void which_way_does_it_give(void)
{
    printf("\nWHICH WAY THE FRAME GIVES\n");

    // The covariance of the two readings, worked out from a run. It is
    // symmetric, which is the shape eigen_solve is written for.
    matrix_t covariance = matrix_alloc(2, 2);
    matrix_t directions = matrix_alloc(2, 2);
    real_t sizes[2];

    matrix_add_element(&covariance, 0, 0, REAL_C(4.2));
    matrix_add_element(&covariance, 0, 1, REAL_C(2.6));
    matrix_add_element(&covariance, 1, 0, REAL_C(2.6));
    matrix_add_element(&covariance, 1, 1, REAL_C(2.8));

    if(!eigen_solve(&covariance, sizes, &directions))
    {
        printf("  The eigenvalues could not be found.\n");
    }
    else
    {
        printf("  The frame moves by %.2f one way and %.2f the other.\n",
               (double)sizes[0], (double)sizes[1]);
        printf("  The soft direction is (%.2f, %.2f) in the axes of the two\n",
               (double)matrix_get_element(&directions, 0, 0),
               (double)matrix_get_element(&directions, 1, 0));
        printf("  sensors, which is not either sensor's own axis: nobody\n");
        printf("  mounted them along the way the frame gives.\n");
        printf("  One way gives %.1f times as much as the other.\n",
               (double)eigen_condition(sizes, 2u));
    }

    matrix_free(&covariance);
    matrix_free(&directions);
}

// ---------------------------------------------------------------------------
// Will this filter stay put, or will it run away.
//
// A filter with feedback is stable only if the roots of its denominator lie
// inside the unit circle. Outside it, the filter's own output feeds back
// larger than it went in, and the answer grows without bound: the first
// recording is fine, and an hour later the output is nonsense.
//
// A caller who designs a filter through the iir module gets a stable one. A
// caller who edits the coefficients by hand, or reads them from a file, or
// works them out on a machine with fewer digits than the design assumed, may
// not. This is the check to run before trusting them.
// ---------------------------------------------------------------------------
static void will_the_filter_stay_put(void)
{
    printf("\nWILL THE FILTER STAY PUT\n");

    // Two denominators of order 2, written lowest power first.
    static const real_t STEADY[3] = {
        REAL_C(0.25), REAL_C(-0.90), REAL_C(1.0)
    };
    static const real_t RUNAWAY[3] = {
        REAL_C(1.30), REAL_C(-1.90), REAL_C(1.0)
    };

    static const real_t* const WHICH[2] = {STEADY, RUNAWAY};
    static const char* const NAME[2] = {"the designed one", "one edited by hand"};

    for(uint32_t index = 0; index < 2u; index++)
    {
        cnum_t roots[2];
        bool inside = poly_is_inside_circle(WHICH[index], 2u);

        printf("  %-20s %s\n", NAME[index],
               inside ? "stays put" : "RUNS AWAY");

        // WHERE the roots are, for the one that does not stay put. A caller
        // mending a filter needs to know which pole went out, and by how far.
        if(!inside && poly_roots(WHICH[index], 2u, roots))
        {
            for(uint32_t which = 0; which < 2u; which++)
            {
                printf("      a pole at %.3f%+.3fi, which is %.3f from the\n",
                       (double)cnum_real(roots[which]),
                       (double)cnum_imaginary(roots[which]),
                       (double)cnum_magnitude(roots[which]));
                printf("      middle, and anything past 1.000 runs away\n");
            }
        }
    }
}

int main(void)
{
    the_rotating_joint();
    the_coupled_coils();
    how_alike_are_two_readings();
    which_way_does_it_give();
    will_the_filter_stay_put();

    return 0;
}

#endif//RUN_EXAMPLE
