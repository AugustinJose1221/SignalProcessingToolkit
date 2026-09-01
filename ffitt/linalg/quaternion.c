#ifndef TEST
#include <ffitt/linalg/quaternion.h>
#include <ffitt/core/defs.h>
#else
#include "quaternion.h"
#include "defs.h"
#endif

#include <math.h>

quaternion_t quaternion_make(real_t w, real_t x, real_t y, real_t z)
{
    quaternion_t q;

    q.w = w;
    q.x = x;
    q.y = y;
    q.z = z;

    return q;
}

quaternion_t quaternion_identity(void)
{
    return quaternion_make(REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0));
}

quaternion_t quaternion_from_axis_angle(real_t x, real_t y, real_t z,
                                        real_t angle)
{
    real_t length = REAL_SQRT((x * x) + (y * y) + (z * z));

    // An axis of no length says nothing about which way to turn.
    if(length <= REAL_SMALLEST)
    {
        return quaternion_identity();
    }

    // The HALF angle. A turn of a whole circle then gives w = -1, thus a
    // quaternion and its negative are the same attitude.
    real_t half = angle / REAL_C(2.0);
    real_t across = REAL_SIN(half) / length;

    return quaternion_make(REAL_COS(half), x * across, y * across, z * across);
}

void quaternion_to_axis_angle(quaternion_t q, real_t* x, real_t* y, real_t* z,
                              real_t* angle)
{
    q = quaternion_normalise(q);

    // A negative w is the same attitude turned the other way round. Turning it
    // over first brings the angle into 0 to pi rather than pi to two pi.
    if(q.w < REAL_C(0.0))
    {
        q = quaternion_scale(q, REAL_C(-1.0));
    }

    real_t across = REAL_SQRT((q.x * q.x) + (q.y * q.y) + (q.z * q.z));

    if(angle != NULL)
    {
        // Worked out from the sine and the cosine together and not from the
        // cosine alone. Near a turn of nothing the cosine is flat, thus its
        // inverse loses most of its digits there, and a turn of nothing is the
        // most common attitude there is.
        *angle = REAL_C(2.0) * REAL_ATAN2(across, q.w);
    }

    if(across <= REAL_SMALLEST)
    {
        // A turn of nothing has no axis. Any axis would do; this one is chosen
        // so that the answer is always something a caller can use.
        if(x != NULL) { *x = REAL_C(1.0); }
        if(y != NULL) { *y = REAL_C(0.0); }
        if(z != NULL) { *z = REAL_C(0.0); }
        return;
    }

    if(x != NULL) { *x = q.x / across; }
    if(y != NULL) { *y = q.y / across; }
    if(z != NULL) { *z = q.z / across; }
}

real_t quaternion_magnitude(quaternion_t q)
{
    return REAL_SQRT((q.w * q.w) + (q.x * q.x) + (q.y * q.y) + (q.z * q.z));
}

quaternion_t quaternion_normalise(quaternion_t q)
{
    real_t length = quaternion_magnitude(q);

    if(length <= REAL_SMALLEST)
    {
        return quaternion_identity();
    }

    return quaternion_scale(q, REAL_C(1.0) / length);
}

quaternion_t quaternion_conjugate(quaternion_t q)
{
    return quaternion_make(q.w, -q.x, -q.y, -q.z);
}

quaternion_t quaternion_multiply(quaternion_t a, quaternion_t b)
{
    // The turn b followed by the turn a, which is the same order as
    // multiplying two rotation matrices.
    return quaternion_make(
        (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z),
        (a.w * b.x) + (a.x * b.w) + (a.y * b.z) - (a.z * b.y),
        (a.w * b.y) - (a.x * b.z) + (a.y * b.w) + (a.z * b.x),
        (a.w * b.z) + (a.x * b.y) - (a.y * b.x) + (a.z * b.w));
}

quaternion_t quaternion_add(quaternion_t a, quaternion_t b)
{
    return quaternion_make(a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z);
}

quaternion_t quaternion_scale(quaternion_t q, real_t factor)
{
    return quaternion_make(q.w * factor, q.x * factor, q.y * factor,
                           q.z * factor);
}

real_t quaternion_dot(quaternion_t a, quaternion_t b)
{
    return (a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

bool quaternion_is_same_attitude(quaternion_t a, quaternion_t b,
                                 real_t tolerance)
{
    a = quaternion_normalise(a);
    b = quaternion_normalise(b);

    // The size and not the value, because a quaternion and its negative are
    // the same attitude. Two attitudes that agree give 1 or -1 here; two that
    // are a quarter turn apart give 0.
    real_t together = REAL_ABS(quaternion_dot(a, b));

    return together >= (REAL_C(1.0) - tolerance);
}

void quaternion_rotate(quaternion_t q, real_t x, real_t y, real_t z,
                       real_t* out_x, real_t* out_y, real_t* out_z)
{
    ASSERT(out_x != NULL);
    ASSERT(out_y != NULL);
    ASSERT(out_z != NULL);

    // The short way, which needs no quaternion multiplication at all: the
    // vector, plus twice the axis crossed into (the axis crossed into the
    // vector, plus w times the vector). It costs about half of the long way.
    real_t cross_x = (q.y * z) - (q.z * y);
    real_t cross_y = (q.z * x) - (q.x * z);
    real_t cross_z = (q.x * y) - (q.y * x);

    cross_x += q.w * x;
    cross_y += q.w * y;
    cross_z += q.w * z;

    real_t second_x = (q.y * cross_z) - (q.z * cross_y);
    real_t second_y = (q.z * cross_x) - (q.x * cross_z);
    real_t second_z = (q.x * cross_y) - (q.y * cross_x);

    // The three are read before any is written, thus the result may be the
    // same three numbers that were given.
    real_t result_x = x + (REAL_C(2.0) * second_x);
    real_t result_y = y + (REAL_C(2.0) * second_y);
    real_t result_z = z + (REAL_C(2.0) * second_z);

    *out_x = result_x;
    *out_y = result_y;
    *out_z = result_z;
}

void quaternion_to_matrix_into(quaternion_t q, matrix_t* dest)
{
    ASSERT(dest != NULL);
    ASSERT((dest->m == 3u) && (dest->n == 3u));

    q = quaternion_normalise(q);

    real_t xx = q.x * q.x;
    real_t yy = q.y * q.y;
    real_t zz = q.z * q.z;
    real_t xy = q.x * q.y;
    real_t xz = q.x * q.z;
    real_t yz = q.y * q.z;
    real_t wx = q.w * q.x;
    real_t wy = q.w * q.y;
    real_t wz = q.w * q.z;

    matrix_add_element(dest, 0, 0, REAL_C(1.0) - (REAL_C(2.0) * (yy + zz)));
    matrix_add_element(dest, 0, 1, REAL_C(2.0) * (xy - wz));
    matrix_add_element(dest, 0, 2, REAL_C(2.0) * (xz + wy));

    matrix_add_element(dest, 1, 0, REAL_C(2.0) * (xy + wz));
    matrix_add_element(dest, 1, 1, REAL_C(1.0) - (REAL_C(2.0) * (xx + zz)));
    matrix_add_element(dest, 1, 2, REAL_C(2.0) * (yz - wx));

    matrix_add_element(dest, 2, 0, REAL_C(2.0) * (xz - wy));
    matrix_add_element(dest, 2, 1, REAL_C(2.0) * (yz + wx));
    matrix_add_element(dest, 2, 2, REAL_C(1.0) - (REAL_C(2.0) * (xx + yy)));
}

quaternion_t quaternion_from_matrix(matrix_t* matrix)
{
    ASSERT(matrix != NULL);
    ASSERT((matrix->m == 3u) && (matrix->n == 3u));

    real_t m00 = matrix_get_element(matrix, 0, 0);
    real_t m11 = matrix_get_element(matrix, 1, 1);
    real_t m22 = matrix_get_element(matrix, 2, 2);
    real_t trace = m00 + m11 + m22;

    quaternion_t q;

    // Four forms of the same answer, and which one to read matters.
    //
    // Each form divides by a root, and a root that is near nothing loses most
    // of its digits. The four roots are largest for four different attitudes,
    // and at every attitude at least one of them is well away from nothing.
    // Reading whichever is largest is therefore accurate everywhere, where
    // reading one form always is not: the first form alone loses its accuracy
    // near a half turn, which is an ordinary attitude and not a corner case.
    if(trace > REAL_C(0.0))
    {
        real_t root = REAL_SQRT(trace + REAL_C(1.0)) * REAL_C(2.0);

        q.w = REAL_C(0.25) * root;
        q.x = (matrix_get_element(matrix, 2, 1)
               - matrix_get_element(matrix, 1, 2)) / root;
        q.y = (matrix_get_element(matrix, 0, 2)
               - matrix_get_element(matrix, 2, 0)) / root;
        q.z = (matrix_get_element(matrix, 1, 0)
               - matrix_get_element(matrix, 0, 1)) / root;
    }
    else if((m00 > m11) && (m00 > m22))
    {
        real_t root = REAL_SQRT(REAL_C(1.0) + m00 - m11 - m22) * REAL_C(2.0);

        q.w = (matrix_get_element(matrix, 2, 1)
               - matrix_get_element(matrix, 1, 2)) / root;
        q.x = REAL_C(0.25) * root;
        q.y = (matrix_get_element(matrix, 0, 1)
               + matrix_get_element(matrix, 1, 0)) / root;
        q.z = (matrix_get_element(matrix, 0, 2)
               + matrix_get_element(matrix, 2, 0)) / root;
    }
    else if(m11 > m22)
    {
        real_t root = REAL_SQRT(REAL_C(1.0) + m11 - m00 - m22) * REAL_C(2.0);

        q.w = (matrix_get_element(matrix, 0, 2)
               - matrix_get_element(matrix, 2, 0)) / root;
        q.x = (matrix_get_element(matrix, 0, 1)
               + matrix_get_element(matrix, 1, 0)) / root;
        q.y = REAL_C(0.25) * root;
        q.z = (matrix_get_element(matrix, 1, 2)
               + matrix_get_element(matrix, 2, 1)) / root;
    }
    else
    {
        real_t root = REAL_SQRT(REAL_C(1.0) + m22 - m00 - m11) * REAL_C(2.0);

        q.w = (matrix_get_element(matrix, 1, 0)
               - matrix_get_element(matrix, 0, 1)) / root;
        q.x = (matrix_get_element(matrix, 0, 2)
               + matrix_get_element(matrix, 2, 0)) / root;
        q.y = (matrix_get_element(matrix, 1, 2)
               + matrix_get_element(matrix, 2, 1)) / root;
        q.z = REAL_C(0.25) * root;
    }

    return quaternion_normalise(q);
}

quaternion_t quaternion_integrate(quaternion_t q, real_t rate_x, real_t rate_y,
                                  real_t rate_z, real_t step)
{
    // How far it turned in this step, and about which axis. Written this way
    // rather than as a rate multiplied into the attitude, because a turn of a
    // known angle about a known axis is exact for that step where the shorter
    // form is only the first part of the answer.
    real_t speed = REAL_SQRT((rate_x * rate_x) + (rate_y * rate_y)
                             + (rate_z * rate_z));

    if(speed <= REAL_SMALLEST)
    {
        return quaternion_normalise(q);
    }

    quaternion_t turn = quaternion_from_axis_angle(rate_x, rate_y, rate_z,
                                                   speed * step);

    // The rate is measured in the body, thus the turn happens in the frame the
    // attitude already describes and stands on the right.
    return quaternion_normalise(quaternion_multiply(q, turn));
}

quaternion_t quaternion_slerp(quaternion_t a, quaternion_t b, real_t part)
{
    a = quaternion_normalise(a);
    b = quaternion_normalise(b);

    real_t together = quaternion_dot(a, b);

    // A quaternion and its negative are the same attitude, thus if the two
    // point apart the shorter way round is towards the negative of the second.
    // Without this the answer would take the long way round, up to a whole
    // circle where a few degrees would do.
    if(together < REAL_C(0.0))
    {
        b = quaternion_scale(b, REAL_C(-1.0));
        together = -together;
    }

    // Very close together, the angle between them is near nothing and dividing
    // by its sine would lose every digit. A straight line between them is then
    // right to far more than the width can hold.
    if(together > (REAL_C(1.0) - REAL_C(0.000001)))
    {
        quaternion_t straight =
            quaternion_add(quaternion_scale(a, REAL_C(1.0) - part),
                           quaternion_scale(b, part));

        return quaternion_normalise(straight);
    }

    real_t angle = REAL_ATAN2(REAL_SQRT(REAL_C(1.0) - (together * together)),
                              together);
    real_t sine = REAL_SIN(angle);

    real_t from = REAL_SIN((REAL_C(1.0) - part) * angle) / sine;
    real_t to = REAL_SIN(part * angle) / sine;

    return quaternion_add(quaternion_scale(a, from), quaternion_scale(b, to));
}
