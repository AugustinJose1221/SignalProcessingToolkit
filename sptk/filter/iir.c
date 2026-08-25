#ifndef TEST
#include <sptk/filter/iir.h>
#include <sptk/core/defs.h>
#else
#include "iir.h"
#include "defs.h"
#endif

#include <math.h>

#define IIR_PI      REAL_C(3.14159265358979323846)

static void iir_set_pass_through(iir_t* iir);
static real_t iir_section_angle(uint32_t section, uint32_t sections);

iir_t iir_alloc(uint32_t sections)
{
    ASSERT(sections > 0);

    iir_t iir;

    iir.sections = sections;
    iir.coefficient = (real_t*)malloc(sizeof(real_t)*IIR_COEFFICIENT_SIZE(sections));
    iir.state = (real_t*)malloc(sizeof(real_t)*IIR_STATE_SIZE(sections));
    iir.dynamic_alloc = true;

    iir_set_pass_through(&iir);
    iir_reset(&iir);

    return iir;
}

iir_t iir_static_alloc(uint32_t sections, real_t* coefficient, real_t* state)
{
    ASSERT(sections > 0);
    ASSERT(coefficient != NULL);
    ASSERT(state != NULL);

    iir_t iir;

    iir.sections = sections;
    iir.coefficient = coefficient;
    iir.state = state;
    iir.dynamic_alloc = false;

    iir_set_pass_through(&iir);
    iir_reset(&iir);

    return iir;
}

bool iir_is_valid_cutoff(real_t cutoff)
{
    return (cutoff >= IIR_MIN_CUTOFF) && (cutoff < REAL_C(0.5));
}

bool iir_is_valid_shape(iir_shape_t shape)
{
    return (shape >= IIR_BUTTERWORTH) && (shape <= IIR_ELLIPTIC);
}

bool iir_is_valid_ripple(real_t ripple)
{
    return (ripple >= IIR_SMALLEST_RIPPLE) && (ripple <= IIR_LARGEST_RIPPLE);
}

bool iir_is_valid_attenuation(real_t attenuation)
{
    return (attenuation >= IIR_SMALLEST_ATTENUATION)
           && (attenuation <= IIR_LARGEST_ATTENUATION);
}

// How far off unity a ripple in decibels is, as a plain number.
//
// A ripple of 1 dB means the answer wanders between 1 and 1/sqrt(1+e*e) where
// e is this. The band that is stopped uses the same number the other way up.
static real_t iir_ripple_factor(real_t decibel)
{
    return REAL_SQRT(REAL_POW(REAL_C(10.0), decibel / REAL_C(10.0))
                     - REAL_C(1.0));
}

// The three numbers that describe one section of an analogue prototype.
//
// A section holds a pair of poles and, for the shapes that have them, a pair
// of zeros on the imaginary axis. Every shape here is built by giving these
// three and letting one piece of arithmetic below turn them into a biquad.
typedef struct{
    real_t real_part;           // How far the poles sit left of the axis
    real_t imaginary_part;      // How far above and below it they sit
    real_t zero_place;          // Where the zeros sit, or 0 for none
    bool has_zeros;             // True where the shape puts zeros on the axis
}iir_prototype_t;

// THE ARITHMETIC AN ELLIPTIC FILTER NEEDS, AND WHY IT IS HERE
//
// An elliptic filter places its poles and zeros by the functions of Jacobi
// rather than by a sine and a cosine. Those functions have no closed form, but
// they are reached by the MEAN OF GAUSS: the arithmetic and the geometric mean
// of two numbers, taken again and again, meet after a handful of steps, and
// everything below is built on that meeting.
//
// This is more arithmetic than the other shapes need. It runs once, when the
// filter is designed, and never while a device runs.

// How many steps of the mean to take. The two means meet to the last digit
// either width can hold long before this many.
#define IIR_MEAN_STEPS      24u

// The complete elliptic integral of the first kind.
static real_t iir_elliptic_integral(real_t modulus)
{
    real_t a = REAL_C(1.0);
    real_t b = REAL_SQRT(REAL_C(1.0) - (modulus * modulus));

    for(uint32_t step = 0; step < IIR_MEAN_STEPS; step++)
    {
        real_t next = (a + b) / REAL_C(2.0);

        b = REAL_SQRT(a * b);
        a = next;
    }

    if(a <= REAL_SMALLEST)
    {
        return REAL_LARGEST;
    }

    return IIR_PI / (REAL_C(2.0) * a);
}

// The measure across from a modulus, which is the measure of the modulus that
// completes it.
//
// THIS IS WHERE THE WIDTH OF THE BUILD SHOWS THROUGH, AND IT SHOWS THROUGH
// TWICE IN THIS MODULE. For a small modulus the square falls below what the
// width can tell from nothing, thus one less that square rounds to exactly 1,
// and the measure of 1 is not finite. Worked out anyway it gives a very large
// number that is nothing but the rounding, and whatever is built on it is
// nonsense: an order of eight million was measured before this existed.
//
// For a small modulus the measure across has a closed form, the logarithm of
// four divided by it, and that is what is used there.
static real_t iir_elliptic_integral_across(real_t modulus)
{
    real_t across = REAL_SQRT(REAL_C(1.0) - (modulus * modulus));

    if((across >= REAL_C(1.0)) || (modulus <= REAL_SMALLEST))
    {
        if(modulus <= REAL_SMALLEST)
        {
            return REAL_LARGEST;
        }

        return REAL_LOG(REAL_C(4.0) / modulus);
    }

    return iir_elliptic_integral(across);
}

// The three functions of Jacobi at a real place, by the descending mean.
static void iir_jacobi(real_t u, real_t modulus, real_t* sn, real_t* cn,
                       real_t* dn)
{
    // At a modulus of nothing they are the plain sine and cosine.
    if(modulus <= REAL_SMALLEST)
    {
        *sn = REAL_SIN(u);
        *cn = REAL_COS(u);
        *dn = REAL_C(1.0);
        return;
    }

    real_t a[IIR_MEAN_STEPS + 1u];
    real_t c[IIR_MEAN_STEPS + 1u];

    a[0] = REAL_C(1.0);
    c[0] = modulus;

    real_t b = REAL_SQRT(REAL_C(1.0) - (modulus * modulus));
    uint32_t count = 0;

    while((REAL_ABS(c[count]) > REAL_EPSILON) && (count < IIR_MEAN_STEPS))
    {
        real_t next_a = (a[count] + b) / REAL_C(2.0);
        real_t next_b = REAL_SQRT(a[count] * b);

        c[count + 1u] = (a[count] - b) / REAL_C(2.0);
        a[count + 1u] = next_a;
        b = next_b;
        count++;
    }

    // Climb back up, halving the angle at each step.
    real_t angle = a[count] * u;

    for(uint32_t step = 0; step < count; step++)
    {
        angle *= REAL_C(2.0);
    }

    for(uint32_t step = count; step >= 1u; step--)
    {
        real_t part = (c[step] / a[step]) * REAL_SIN(angle);

        if(part > REAL_C(1.0)) { part = REAL_C(1.0); }
        if(part < -REAL_C(1.0)) { part = -REAL_C(1.0); }

        angle = (angle + REAL_ASIN(part)) / REAL_C(2.0);
    }

    *sn = REAL_SIN(angle);
    *cn = REAL_COS(angle);

    real_t under = REAL_C(1.0) - (modulus * modulus * (*sn) * (*sn));

    *dn = (under > REAL_C(0.0)) ? REAL_SQRT(under) : REAL_C(0.0);
}

// Give the place where sn reaches the given value, by halving the range.
//
// sn climbs steadily from 0 to 1 across a quarter of its period, thus halving
// the range finds the place without any series at all.
static real_t iir_jacobi_place_of(real_t value, real_t modulus)
{
    real_t low = REAL_C(0.0);
    real_t high = iir_elliptic_integral(modulus);

    for(uint32_t step = 0; step < 60u; step++)
    {
        real_t middle = (low + high) / REAL_C(2.0);
        real_t sn;
        real_t cn;
        real_t dn;

        iir_jacobi(middle, modulus, &sn, &cn, &dn);

        if(sn < value)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }

    return (low + high) / REAL_C(2.0);
}

// Give the modulus that an elliptic filter of this order needs, from how far
// apart the two ripples stand.
//
// This is the degree equation, and it is solved through the nome: a number
// that describes the modulus and which the order divides cleanly.
static real_t iir_elliptic_modulus(uint32_t order, real_t selectivity)
{
    // THE NOME, AND WHERE THE WIDTH OF THE BUILD SHOWS THROUGH.
    //
    // The nome is worked out from the two measures of the selectivity: the one
    // straight across it and the one across from it. For a deep band that is
    // stopped the selectivity is very small, its square falls below what the
    // width can tell from nothing, and THE MODULUS ACROSS ROUNDS TO EXACTLY 1.
    // Its measure is then not finite, and the nome taken from it falls to
    // nothing, which would refuse a filter that is perfectly buildable.
    //
    // Where the selectivity is that small the nome has a closed form. The
    // measure straight across approaches a quarter turn and the one across
    // from it approaches the logarithm of four over the selectivity, and the
    // two together leave the selectivity over four, squared. Taking that form
    // is not an approximation of convenience; it is what the limit IS.
    real_t straight = iir_elliptic_integral(selectivity);
    real_t across = iir_elliptic_integral_across(selectivity);

    if((straight <= REAL_SMALLEST) || (across >= REAL_LARGEST))
    {
        return REAL_C(0.0);
    }

    real_t nome = REAL_EXP(-IIR_PI * across / straight);

    if(nome <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    real_t shared = REAL_POW(nome, REAL_C(1.0) / (real_t)order);

    // The modulus from the nome, as the ratio of two series that fall away
    // quickly. Twelve terms carry both past what either width can hold.
    real_t above = REAL_C(0.0);
    real_t below = REAL_C(1.0);

    for(uint32_t m = 0; m < 12u; m++)
    {
        above += REAL_POW(shared, (real_t)(m * (m + 1u)));
    }

    for(uint32_t m = 1; m < 12u; m++)
    {
        below += REAL_C(2.0) * REAL_POW(shared, (real_t)(m * m));
    }

    if(below <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    real_t ratio = above / below;

    return REAL_C(4.0) * REAL_SQRT(shared) * ratio * ratio;
}

// Give the pole and the zero of one section of an elliptic prototype.
static bool iir_elliptic_section(uint32_t section, uint32_t sections,
                                 real_t pass_ripple, real_t stop_ripple,
                                 iir_prototype_t* out)
{
    uint32_t order = sections * 2u;

    real_t pass_epsilon = iir_ripple_factor(pass_ripple);
    real_t stop_epsilon = iir_ripple_factor(stop_ripple);

    if((pass_epsilon <= REAL_SMALLEST) || (stop_epsilon <= REAL_SMALLEST))
    {
        return false;
    }

    // How far apart the two ripples stand. A small number here means the two
    // bands may stand close together, which is what an elliptic filter is for.
    real_t selectivity = pass_epsilon / stop_epsilon;

    if((selectivity <= REAL_SMALLEST) || (selectivity >= REAL_C(1.0)))
    {
        return false;
    }

    real_t modulus = iir_elliptic_modulus(order, selectivity);

    if((modulus <= REAL_SMALLEST) || (modulus >= REAL_C(1.0)))
    {
        return false;
    }

    real_t quarter = iir_elliptic_integral(modulus);
    real_t across = REAL_SQRT(REAL_C(1.0) - (modulus * modulus));

    // Where the band that passes ends, measured through the ripple.
    //
    // THE ONE PLACE THIS ARITHMETIC MEETS THE EDGE OF THE WIDTH. The measure
    // wanted here belongs to the modulus ACROSS from the selectivity, which is
    // the square root of one less its square. For a deep band that is stopped
    // the selectivity is very small, and at 32 bits its square falls below
    // what can be told from nothing: the modulus across then rounds to exactly
    // 1, where the measure is no longer finite and the search below would run
    // on nothing.
    //
    // At that modulus the function of Jacobi becomes the plain hyperbolic
    // tangent, and the place wanted has a closed form: the hyperbolic arc sine
    // of one over the ripple. Taking that form is not an approximation of
    // convenience; it is what the limit IS, and it lets a 32 bit build reach
    // stop bands it would otherwise have to refuse.
    real_t across_ripples = REAL_SQRT(REAL_C(1.0)
                                      - (selectivity * selectivity));
    real_t edge;

    if(across_ripples >= REAL_C(1.0))
    {
        edge = REAL_ASINH(REAL_C(1.0) / pass_epsilon);
    }
    else
    {
        edge = iir_jacobi_place_of(
            REAL_C(1.0) / REAL_SQRT(REAL_C(1.0)
                                    + (pass_epsilon * pass_epsilon)),
            across_ripples);
    }

    real_t straight = iir_elliptic_integral(selectivity);

    if(straight <= REAL_SMALLEST)
    {
        return false;
    }

    real_t depth = edge / ((real_t)order * straight);

    // The place of this section along the quarter period.
    real_t place = ((REAL_C(2.0) * (real_t)section) + REAL_C(1.0))
                   / (real_t)order;

    real_t sn;
    real_t cn;
    real_t dn;

    iir_jacobi(place * quarter, modulus, &sn, &cn, &dn);

    if(REAL_ABS(sn) <= REAL_SMALLEST)
    {
        return false;
    }

    // THE ZEROS ARE WHAT MAKE THIS SHAPE FALL FASTEST. They crowd towards the
    // edge of the band that is stopped instead of spreading evenly, and each
    // one pins the answer to nothing where it stands.
    out->zero_place = REAL_C(1.0) / (modulus * sn);
    out->has_zeros = true;

    // The pole, from the same functions at a place that has stepped sideways
    // off the real line. The step is what pulls the pole away from the axis
    // and gives the band that passes its ripple.
    real_t sn_side;
    real_t cn_side;
    real_t dn_side;

    iir_jacobi(depth * quarter, across, &sn_side, &cn_side, &dn_side);

    real_t divisor = (cn_side * cn_side)
                     + (modulus * modulus * sn * sn * sn_side * sn_side);

    if(REAL_ABS(divisor) <= REAL_SMALLEST)
    {
        return false;
    }

    // cn divided by dn at that sideways place, which is what the pole is.
    real_t cn_real = (cn * cn_side) / divisor;
    real_t cn_imaginary = -(sn * dn * sn_side * dn_side) / divisor;
    real_t dn_real = (dn * cn_side * dn_side) / divisor;
    real_t dn_imaginary = -(modulus * modulus * sn * cn * sn_side) / divisor;

    real_t dn_size = (dn_real * dn_real) + (dn_imaginary * dn_imaginary);

    if(dn_size <= REAL_SMALLEST)
    {
        return false;
    }

    real_t ratio_real = ((cn_real * dn_real) + (cn_imaginary * dn_imaginary))
                        / dn_size;
    real_t ratio_imaginary = ((cn_imaginary * dn_real)
                              - (cn_real * dn_imaginary)) / dn_size;

    // The pole stands at j times that ratio, thus the two parts change places
    // and one changes sign. The real part is taken as positive here, because
    // the arithmetic that follows expects how far LEFT of the axis it sits.
    out->real_part = ratio_imaginary;
    out->imaginary_part = ratio_real;

    if(out->real_part < REAL_C(0.0))
    {
        out->real_part = -out->real_part;
    }

    return true;
}

// Give the section of the analogue prototype for one pair of poles.
//
// THE PROTOTYPE IS A LOW PASS WHOSE EDGE STANDS AT 1. Everything else, the
// cutoff and the high pass, is a matter of where that 1 is put afterwards.
static bool iir_prototype_of(iir_shape_t shape, uint32_t section,
                             uint32_t sections, real_t pass_ripple,
                             real_t stop_ripple, iir_prototype_t* out)
{
    uint32_t order = sections * 2u;

    // The angle of this pair of poles round the circle, which every shape here
    // begins from.
    real_t angle = iir_section_angle(section, sections);

    out->zero_place = REAL_C(0.0);
    out->has_zeros = false;

    if(shape == IIR_BUTTERWORTH)
    {
        // The poles lie on a circle of radius 1, evenly spread.
        out->real_part = REAL_COS(angle);
        out->imaginary_part = REAL_SIN(angle);

        return true;
    }

    if(shape == IIR_CHEBYSHEV_I)
    {
        // The circle becomes an ellipse. How flat it is follows the ripple:
        // more ripple gives a flatter ellipse and poles nearer the axis, which
        // is what makes the fall sharper.
        real_t epsilon = iir_ripple_factor(pass_ripple);

        if(epsilon <= REAL_SMALLEST)
        {
            return false;
        }

        real_t v = REAL_ASINH(REAL_C(1.0) / epsilon) / (real_t)order;

        out->real_part = REAL_SINH(v) * REAL_COS(angle);
        out->imaginary_part = REAL_COSH(v) * REAL_SIN(angle);

        return true;
    }

    if(shape == IIR_CHEBYSHEV_II)
    {
        // The poles of Chebyshev I, turned inside out, and zeros put on the
        // axis. The zeros are what hold the band that is stopped down, and
        // they are why this shape ripples there and nowhere else.
        real_t epsilon = iir_ripple_factor(stop_ripple);

        if(epsilon <= REAL_SMALLEST)
        {
            return false;
        }

        real_t v = REAL_ASINH(epsilon) / (real_t)order;
        real_t re = REAL_SINH(v) * REAL_COS(angle);
        real_t im = REAL_COSH(v) * REAL_SIN(angle);
        real_t size_of = (re * re) + (im * im);

        if(size_of <= REAL_SMALLEST)
        {
            return false;
        }

        out->real_part = re / size_of;
        out->imaginary_part = im / size_of;

        // The zeros stand where the cosine of the angle is nothing, which for
        // an even order never happens, thus every section has a pair.
        real_t sine = REAL_SIN(angle);

        if(REAL_ABS(sine) <= REAL_SMALLEST)
        {
            return false;
        }

        out->zero_place = REAL_C(1.0) / REAL_ABS(sine);
        out->has_zeros = true;

        return true;
    }

    // ELLIPTIC.
    return iir_elliptic_section(section, sections, pass_ripple, stop_ripple,
                                out);
}

// Turn one section of the analogue prototype into a biquad, by the bilinear
// transform, at the given warped cutoff.
//
// The prototype is a low pass whose edge stands at 1. For a high pass the
// prototype is turned inside out first, which exchanges the two ends of the
// frequency axis.
static void iir_bilinear(iir_t* iir, uint32_t section,
                         const iir_prototype_t* prototype, real_t warped,
                         bool high_pass)
{
    real_t re = prototype->real_part;
    real_t im = prototype->imaginary_part;
    real_t size_of = (re * re) + (im * im);

    real_t zero = prototype->zero_place;
    bool has_zeros = prototype->has_zeros;

    if(high_pass)
    {
        // A low pass becomes a high pass when every place on the frequency
        // axis is exchanged with its reciprocal. The poles move, and a pair of
        // zeros at infinity comes down to nothing, which is why a high pass
        // always has zeros even where the low pass had none.
        if(size_of <= REAL_SMALLEST)
        {
            size_of = REAL_SMALLEST;
        }

        re = re / size_of;
        im = im / size_of;

        zero = has_zeros ? (REAL_C(1.0) / zero) : REAL_C(0.0);
        has_zeros = true;
    }

    // The pole pair as a quadratic in s: s squared, plus this much s, plus
    // this much.
    real_t damping = REAL_C(2.0) * re;
    real_t constant = (re * re) + (im * im);

    // The bilinear transform, with the warped cutoff folded in.
    real_t squared = warped * warped;

    real_t a0 = REAL_C(1.0) + (damping * warped) + (constant * squared);
    real_t a1 = REAL_C(2.0) * ((constant * squared) - REAL_C(1.0));
    real_t a2 = REAL_C(1.0) - (damping * warped) + (constant * squared);

    real_t b0;
    real_t b1;
    real_t b2;

    if(has_zeros)
    {
        real_t placed = zero * zero * squared;

        b0 = REAL_C(1.0) + placed;
        b1 = REAL_C(2.0) * (placed - REAL_C(1.0));
        b2 = REAL_C(1.0) + placed;
    }
    else
    {
        // No zeros of its own, thus both sit at the far end of the axis and
        // the numerator is the plain square.
        b0 = squared;
        b1 = REAL_C(2.0) * squared;
        b2 = squared;
    }

    // Bring the section to a gain of 1 where it should have one: at nothing
    // for a low pass, and at half the sample rate for a high pass. Doing it
    // here rather than in the arithmetic above keeps every shape to one rule
    // and leaves no room for a sign to go astray.
    real_t above = high_pass ? ((b0 - b1) + b2) : ((b0 + b1) + b2);
    real_t below = high_pass ? ((a0 - a1) + a2) : ((a0 + a1) + a2);

    if(REAL_ABS(above) > REAL_SMALLEST)
    {
        real_t scale = below / above;

        b0 *= scale;
        b1 *= scale;
        b2 *= scale;
    }

    iir_set_section(iir, section, b0, b1, b2, a0, a1, a2);
}

// Build a filter of the given shape, either way up.
static bool iir_design_with(iir_t* iir, real_t cutoff, iir_shape_t shape,
                            real_t pass_ripple, real_t stop_ripple,
                            bool high_pass)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_shape(shape) || !iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

    // Only the ripples that the shape reads are examined. Asking a Butterworth
    // about a ripple it never looks at would refuse a design that is perfectly
    // sound.
    if(((shape == IIR_CHEBYSHEV_I) || (shape == IIR_ELLIPTIC))
       && !iir_is_valid_ripple(pass_ripple))
    {
        return false;
    }

    if(((shape == IIR_CHEBYSHEV_II) || (shape == IIR_ELLIPTIC))
       && !iir_is_valid_attenuation(stop_ripple))
    {
        return false;
    }

    real_t warped = REAL_TAN(IIR_PI * cutoff);

    if(high_pass)
    {
        // A high pass is a low pass with the axis turned round, thus the
        // cutoff is turned round with it.
        if(warped <= REAL_SMALLEST)
        {
            return false;
        }

        warped = REAL_C(1.0) / warped;
    }

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_prototype_t prototype;

        if(!iir_prototype_of(shape, section, iir->sections, pass_ripple,
                             stop_ripple, &prototype))
        {
            return false;
        }

        iir_bilinear(iir, section, &prototype, warped, high_pass);
    }

    // A Chebyshev I of an even order does not reach 1 at nothing: the ripple
    // is measured DOWN from 1, thus at an even order the answer starts at the
    // bottom of the ripple and climbs to 1. Every section above was brought to
    // a gain of 1 at nothing, which would put the top of the ripple ABOVE 1
    // and make the filter amplify what it is meant to pass.
    //
    // EVERY ORDER HERE IS EVEN, because a section holds two poles. Thus this
    // is not a special case; it is what always has to happen.
    if((shape == IIR_CHEBYSHEV_I) || (shape == IIR_ELLIPTIC))
    {
        real_t epsilon = iir_ripple_factor(pass_ripple);
        real_t scale = REAL_C(1.0)
                       / REAL_SQRT(REAL_C(1.0) + (epsilon * epsilon));

        for(uint32_t which = 0; which < IIR_COEFFICIENT_COUNT - 2u; which++)
        {
            iir->coefficient[which] *= scale;
        }
    }

    iir_reset(iir);

    return true;
}

bool iir_design_low_pass_with(iir_t* iir, real_t cutoff, iir_shape_t shape,
                              real_t pass_ripple, real_t stop_ripple)
{
    return iir_design_with(iir, cutoff, shape, pass_ripple, stop_ripple,
                           false);
}

bool iir_design_high_pass_with(iir_t* iir, real_t cutoff, iir_shape_t shape,
                               real_t pass_ripple, real_t stop_ripple)
{
    return iir_design_with(iir, cutoff, shape, pass_ripple, stop_ripple, true);
}

uint32_t iir_sections_for(iir_shape_t shape, real_t pass_edge,
                          real_t stop_edge, real_t pass_ripple,
                          real_t stop_ripple)
{
    if(!iir_is_valid_shape(shape) || !iir_is_valid_ripple(pass_ripple)
       || !iir_is_valid_attenuation(stop_ripple))
    {
        return 0u;
    }

    if(!iir_is_valid_cutoff(pass_edge) || !iir_is_valid_cutoff(stop_edge)
       || (stop_edge <= pass_edge))
    {
        return 0u;
    }

    // The two edges are bent the same way the design bends them, thus the
    // number that comes out is the number the design will really need.
    real_t pass_warped = REAL_TAN(IIR_PI * pass_edge);
    real_t stop_warped = REAL_TAN(IIR_PI * stop_edge);

    if(pass_warped <= REAL_SMALLEST)
    {
        return 0u;
    }

    // How far apart the two bands stand, and how much is asked of the filter
    // between them. The order is the second divided by the first, measured in
    // whichever way the shape falls.
    real_t apart = stop_warped / pass_warped;
    real_t wanted = iir_ripple_factor(stop_ripple)
                    / iir_ripple_factor(pass_ripple);

    if((apart <= REAL_C(1.0)) || (wanted <= REAL_C(1.0)))
    {
        return 0u;
    }

    real_t order;

    if(shape == IIR_BUTTERWORTH)
    {
        // A Butterworth falls as a plain power of the frequency.
        order = REAL_LOG(wanted) / REAL_LOG(apart);
    }
    else if(shape == IIR_ELLIPTIC)
    {
        // An elliptic filter needs the fewest of all, and how few follows the
        // same integrals that place its poles. This is the degree equation
        // read the other way round: given how far apart the bands and the
        // ripples stand, it gives the order.
        real_t bands = REAL_C(1.0) / apart;
        real_t ripples = REAL_C(1.0) / wanted;

        real_t above = iir_elliptic_integral(bands)
                       * iir_elliptic_integral_across(ripples);
        real_t below = iir_elliptic_integral(ripples)
                       * iir_elliptic_integral_across(bands);

        if((below <= REAL_SMALLEST) || (above >= REAL_LARGEST))
        {
            return 0u;
        }

        order = above / below;
    }
    else
    {
        // Both Chebyshev shapes fall faster, and by how much follows the
        // cosine the other way round. This is why they need fewer sections for
        // the same specification.
        order = REAL_ACOSH(wanted) / REAL_ACOSH(apart);
    }

    // Two poles to a section, thus the order is halved and then rounded up: a
    // filter cannot hold half a section, and rounding down would give a filter
    // that does not meet what was asked.
    real_t sections = REAL_CEIL(order / REAL_C(2.0));

    if(sections < REAL_C(1.0))
    {
        sections = REAL_C(1.0);
    }

    // A number of sections that no filter could hold is not an answer, and
    // handing one back invites a caller to allocate it. Where the arithmetic
    // reaches that far, the specification cannot be met by this module.
    if(sections > (real_t)IIR_LARGEST_SECTIONS)
    {
        return 0u;
    }

    return (uint32_t)sections;
}

real_t iir_phase(iir_t* iir, real_t frequency)
{
    ASSERT(iir != NULL);

    real_t angle = REAL_C(2.0) * IIR_PI * frequency;
    real_t cosine = REAL_COS(angle);
    real_t sine = REAL_SIN(angle);
    real_t cosine_two = REAL_COS(REAL_C(2.0) * angle);
    real_t sine_two = REAL_SIN(REAL_C(2.0) * angle);

    // The turn of the whole filter is the turns of its sections added, thus
    // the parts of one complex number are carried along and multiplied.
    real_t whole_real = REAL_C(1.0);
    real_t whole_imaginary = REAL_C(0.0);

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        const real_t* c = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

        // The five coefficients read at this frequency. The sign of the
        // imaginary part follows z to the minus one, which turns backwards.
        real_t above_real = c[0] + (c[1] * cosine) + (c[2] * cosine_two);
        real_t above_imaginary = -((c[1] * sine) + (c[2] * sine_two));

        real_t below_real = REAL_C(1.0) + (c[3] * cosine) + (c[4] * cosine_two);
        real_t below_imaginary = -((c[3] * sine) + (c[4] * sine_two));

        real_t below_size = (below_real * below_real)
                            + (below_imaginary * below_imaginary);

        if(below_size <= REAL_SMALLEST)
        {
            continue;
        }

        // One section divided by another, and the whole multiplied by it.
        real_t part_real = ((above_real * below_real)
                            + (above_imaginary * below_imaginary))
                           / below_size;
        real_t part_imaginary = ((above_imaginary * below_real)
                                 - (above_real * below_imaginary))
                                / below_size;

        real_t next_real = (whole_real * part_real)
                           - (whole_imaginary * part_imaginary);
        real_t next_imaginary = (whole_real * part_imaginary)
                                + (whole_imaginary * part_real);

        whole_real = next_real;
        whole_imaginary = next_imaginary;
    }

    return REAL_ATAN2(whole_imaginary, whole_real);
}

real_t iir_group_delay(iir_t* iir, real_t frequency)
{
    ASSERT(iir != NULL);

    // How far either side to look. Small enough that the phase between the two
    // is nearly straight, and large enough that the difference is not lost in
    // the rounding of the two phases.
    real_t step = IIR_GROUP_DELAY_STEP;

    real_t low = frequency - step;
    real_t high = frequency + step;

    // Held inside the frequencies that exist. At the very ends the answer is
    // taken from one side only, which is coarser and still worth having.
    if(low < REAL_C(0.0))
    {
        low = REAL_C(0.0);
        high = step * REAL_C(2.0);
    }

    if(high > REAL_C(0.5))
    {
        high = REAL_C(0.5);
        low = REAL_C(0.5) - (step * REAL_C(2.0));
    }

    real_t before = iir_phase(iir, low);
    real_t after = iir_phase(iir, high);

    // The phase comes back folded into one turn, thus a step that crosses the
    // fold looks like a jump of a whole turn. Unfolding it is the whole of
    // what makes this measurement work.
    real_t difference = after - before;

    while(difference > IIR_PI)
    {
        difference -= REAL_C(2.0) * IIR_PI;
    }

    while(difference < -IIR_PI)
    {
        difference += REAL_C(2.0) * IIR_PI;
    }

    // The delay is how fast the phase falls with frequency, and it is measured
    // in samples because the frequency is a part of the sample rate.
    return -difference / (REAL_C(2.0) * IIR_PI * (high - low));
}

bool iir_design_low_pass(iir_t* iir, real_t cutoff)
{
    ASSERT(iir != NULL);

    // The check stands here and not in an assertion. An assertion goes away in
    // a release build, and a cutoff that is too low does not announce itself:
    // the filter answers, and its answer is wrong. Thus the module holds the
    // value itself.
    if(!iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

    // The bilinear transform bends the frequency, thus the design first bends
    // the cutoff the other way. Then the filter holds its cutoff at the place
    // that the caller asked for.
    real_t warped = REAL_TAN(IIR_PI * cutoff);
    real_t squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        // Each section of a filter of Butterworth holds a pair of poles at its
        // own angle on the circle.
        real_t damping = REAL_C(2.0) * REAL_COS(iir_section_angle(section, iir->sections));
        real_t divisor = REAL_C(1.0) + (damping * warped) + squared;

        real_t b0 = squared / divisor;
        real_t b1 = REAL_C(2.0) * b0;
        real_t b2 = b0;
        real_t a1 = (REAL_C(2.0) * (squared - REAL_C(1.0))) / divisor;
        real_t a2 = (REAL_C(1.0) - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, REAL_C(1.0), a1, a2);
    }

    return true;
}

bool iir_design_high_pass(iir_t* iir, real_t cutoff)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(cutoff))
    {
        return false;
    }

    real_t warped = REAL_TAN(IIR_PI * cutoff);
    real_t squared = warped * warped;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t damping = REAL_C(2.0) * REAL_COS(iir_section_angle(section, iir->sections));
        real_t divisor = REAL_C(1.0) + (damping * warped) + squared;

        real_t b0 = REAL_C(1.0) / divisor;
        real_t b1 = -REAL_C(2.0) * b0;
        real_t b2 = b0;
        real_t a1 = (REAL_C(2.0) * (squared - REAL_C(1.0))) / divisor;
        real_t a2 = (REAL_C(1.0) - (damping * warped) + squared) / divisor;

        iir_set_section(iir, section, b0, b1, b2, REAL_C(1.0), a1, a2);
    }

    return true;
}

// The two numbers that every second order design at one frequency needs.
//
// The first is the cosine of the turn that the frequency makes in one sample.
// The second sets how wide the design reaches around that frequency: it falls
// as the quality rises, thus a high quality gives a narrow design.
static void iir_resonance(real_t centre, real_t quality, real_t* cosine, real_t* alpha)
{
    real_t turn = REAL_C(2.0) * IIR_PI * centre;

    *cosine = REAL_COS(turn);
    *alpha = REAL_SIN(turn) / (REAL_C(2.0) * quality);
}

// The middle of a band, and how narrow that band is.
//
// The middle is the GEOMETRIC mean of the two edges and not the plain mean.
// A filter of this kind is symmetric about its middle in the ratio of the
// frequencies and not in their difference: a band from 100 to 400 has its
// middle at 200, because 200 is twice 100 and 400 is twice 200. The plain mean
// would put it at 250 and the two edges would then not fall away equally.
static void iir_band_to_resonance(real_t low_cutoff, real_t high_cutoff,
                                  real_t* centre, real_t* quality)
{
    *centre = REAL_SQRT(low_cutoff * high_cutoff);
    *quality = *centre / (high_cutoff - low_cutoff);
}

bool iir_design_band_pass(iir_t* iir, real_t low_cutoff, real_t high_cutoff)
{
    ASSERT(iir != NULL);

    if((iir->sections % 2u) != 0u)
    {
        return false;
    }
    if(!iir_is_valid_cutoff(low_cutoff) || !iir_is_valid_cutoff(high_cutoff))
    {
        return false;
    }
    if(high_cutoff <= low_cutoff)
    {
        return false;
    }

    // Half of the sections make the high pass at the low edge, and half make
    // the low pass at the high edge. Each half is designed on its own and then
    // copied into its place, thus the design of each edge stays the one that
    // is already tested.
    uint32_t half = iir->sections / 2u;

    iir_t low_part = iir_alloc(half);
    iir_t high_part = iir_alloc(half);

    bool built = iir_design_high_pass(&high_part, low_cutoff)
                 && iir_design_low_pass(&low_part, high_cutoff);

    if(built)
    {
        for(uint32_t section = 0; section < half; section++)
        {
            const real_t* from = &high_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, section, from[0], from[1], from[2],
                            REAL_C(1.0), from[3], from[4]);
        }
        for(uint32_t section = 0; section < half; section++)
        {
            const real_t* from = &low_part.coefficient[section * IIR_COEFFICIENT_COUNT];
            iir_set_section(iir, half + section, from[0], from[1], from[2],
                            REAL_C(1.0), from[3], from[4]);
        }
    }

    iir_free(&low_part);
    iir_free(&high_part);

    return built;
}

bool iir_design_notch(iir_t* iir, real_t centre, real_t quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= REAL_C(0.0)))
    {
        return false;
    }

    real_t cosine;
    real_t alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The zeros stand exactly on the circle at the frequency, thus the gain
    // there is nothing. The poles stand just inside the circle at the same
    // frequency, thus everywhere else the two nearly cancel and the gain stays
    // at one.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        REAL_C(1.0), -REAL_C(2.0) * cosine, REAL_C(1.0),
                        REAL_C(1.0) + alpha, -REAL_C(2.0) * cosine, REAL_C(1.0) - alpha);
    }

    return true;
}

bool iir_design_peak(iir_t* iir, real_t centre, real_t quality)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(centre) || (quality <= REAL_C(0.0)))
    {
        return false;
    }

    real_t cosine;
    real_t alpha;
    iir_resonance(centre, quality, &cosine, &alpha);

    // The same poles as the notch, and zeros at nothing and at half the sample
    // rate instead of on the frequency. Thus the gain is 1 at the frequency
    // and falls away on both sides of it.
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section,
                        alpha, REAL_C(0.0), -alpha,
                        REAL_C(1.0) + alpha, -REAL_C(2.0) * cosine, REAL_C(1.0) - alpha);
    }

    return true;
}

bool iir_design_band_stop(iir_t* iir, real_t low_cutoff, real_t high_cutoff)
{
    ASSERT(iir != NULL);

    if(!iir_is_valid_cutoff(low_cutoff) || !iir_is_valid_cutoff(high_cutoff))
    {
        return false;
    }
    if(high_cutoff <= low_cutoff)
    {
        return false;
    }

    real_t centre;
    real_t quality;
    iir_band_to_resonance(low_cutoff, high_cutoff, &centre, &quality);

    return iir_design_notch(iir, centre, quality);
}

void iir_set_section(iir_t* iir, uint32_t section, real_t b0, real_t b1, real_t b2,
                     real_t a0, real_t a1, real_t a2)
{
    ASSERT(iir != NULL);
    ASSERT(section < iir->sections);
    ASSERT(a0 != REAL_C(0.0));

    real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

    coefficient[0] = b0 / a0;
    coefficient[1] = b1 / a0;
    coefficient[2] = b2 / a0;
    coefficient[3] = a1 / a0;
    coefficient[4] = a2 / a0;
}

real_t iir_process_sample(iir_t* iir, real_t sample)
{
    ASSERT(iir != NULL);

    real_t value = sample;

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];
        real_t* state = &iir->state[section * IIR_STATE_COUNT];

        // Direct Form II transposed.
        real_t result = (coefficient[0] * value) + state[0];
        state[0] = (coefficient[1] * value) - (coefficient[3] * result) + state[1];
        state[1] = (coefficient[2] * value) - (coefficient[4] * result);

        value = result;
    }

    return value;
}

void iir_process_block(iir_t* iir, const real_t* input, real_t* output, uint32_t size)
{
    ASSERT(iir != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    for(uint32_t index = 0; index < size; index++)
    {
        output[index] = iir_process_sample(iir, input[index]);
    }
}

void iir_reset(iir_t* iir)
{
    ASSERT(iir != NULL);

    for(uint32_t index = 0; index < IIR_STATE_SIZE(iir->sections); index++)
    {
        iir->state[index] = REAL_C(0.0);
    }
}

real_t iir_get_gain(iir_t* iir, real_t frequency)
{
    ASSERT(iir != NULL);

    real_t angle = REAL_C(2.0) * IIR_PI * frequency;
    real_t total = REAL_C(1.0);

    for(uint32_t section = 0; section < iir->sections; section++)
    {
        real_t* coefficient = &iir->coefficient[section * IIR_COEFFICIENT_COUNT];

        // The answer of one section is the value of its polynomial of the
        // input divided by the value of its polynomial of the feedback, both
        // at the point on the circle at this angle.
        real_t top_real = coefficient[0] + (coefficient[1] * REAL_COS(-angle))
                         + (coefficient[2] * REAL_COS(-REAL_C(2.0)*angle));
        real_t top_imaginary = (coefficient[1] * REAL_SIN(-angle))
                              + (coefficient[2] * REAL_SIN(-REAL_C(2.0)*angle));

        real_t bottom_real = REAL_C(1.0) + (coefficient[3] * REAL_COS(-angle))
                            + (coefficient[4] * REAL_COS(-REAL_C(2.0)*angle));
        real_t bottom_imaginary = (coefficient[3] * REAL_SIN(-angle))
                                 + (coefficient[4] * REAL_SIN(-REAL_C(2.0)*angle));

        real_t top = REAL_SQRT((top_real*top_real) + (top_imaginary*top_imaginary));
        real_t bottom = REAL_SQRT((bottom_real*bottom_real)
                             + (bottom_imaginary*bottom_imaginary));

        if(bottom == REAL_C(0.0))
        {
            return REAL_C(0.0);
        }

        total *= top / bottom;
    }

    return total;
}

void iir_free(iir_t* iir)
{
    ASSERT(iir != NULL);

    if(iir->dynamic_alloc)
    {
        free(iir->coefficient);
        free(iir->state);
        iir->coefficient = NULL;
        iir->state = NULL;
        iir->dynamic_alloc = false;
    }
}

// Give the angle of the pair of poles of one section of a filter of
// Butterworth. The poles of such a filter lie at even distances on a half
// circle.
static real_t iir_section_angle(uint32_t section, uint32_t sections)
{
    real_t order = REAL_C(2.0) * (real_t)sections;

    return (IIR_PI * (((REAL_C(2.0) * (real_t)section) + REAL_C(1.0)) / (REAL_C(2.0) * order)));
}

// Give every section the coefficients that let the signal pass unchanged.
static void iir_set_pass_through(iir_t* iir)
{
    for(uint32_t section = 0; section < iir->sections; section++)
    {
        iir_set_section(iir, section, REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0));
    }
}
