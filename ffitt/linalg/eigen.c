// This file is left out of the build when FFITT_NO_LINALG is defined.
// ffitt/core/README.md says which areas may be left out and
// which of them need which others.
#ifndef FFITT_NO_LINALG

#ifndef TEST
#include <ffitt/linalg/eigen.h>
#include <ffitt/core/defs.h>
#else
#include "eigen.h"
#include "defs.h"
#endif

#include <math.h>

// How far two elements across the diagonal may stand apart and still count as
// the same. A covariance built by a long chain of arithmetic is symmetric in
// principle and not in its last digits.
static real_t eigen_symmetry_room(matrix_t* matrix)
{
    real_t largest = REAL_C(0.0);

    for(uint32_t row = 0; row < matrix->m; row++)
    {
        for(uint32_t column = 0; column < matrix->n; column++)
        {
            real_t size_of = REAL_ABS(matrix_get_element(matrix, row, column));

            if(size_of > largest) { largest = size_of; }
        }
    }

    return (REAL_C(1000.0) * REAL_EPSILON * largest) + REAL_SMALLEST;
}

bool eigen_is_valid_matrix(matrix_t* matrix)
{
    ASSERT(matrix != NULL);

    if(!matrix_is_square(matrix) || (matrix->m == 0u))
    {
        return false;
    }

    return matrix_is_symmetric(matrix, eigen_symmetry_room(matrix));
}

// Give the sum of the squares of everything that is not on the diagonal.
//
// This is the one number that says how much work is left: the rotations drive
// it towards nothing, and when it is small enough against the diagonal the
// diagonal IS the answer.
static real_t eigen_off_diagonal(matrix_t* matrix)
{
    real_t total = REAL_C(0.0);

    for(uint32_t row = 0; row < matrix->m; row++)
    {
        for(uint32_t column = row + 1u; column < matrix->n; column++)
        {
            real_t value = matrix_get_element(matrix, row, column);

            total += value * value;
        }
    }

    return total;
}

// Turn two rows and two columns so that the element where they cross becomes
// nothing.
//
// THE ANGLE IS NOT WORKED OUT WITH AN ARC TANGENT, and that is deliberate. The
// tangent of the angle is the root of a quadratic, and taking the smaller of
// its two roots keeps the rotation small. A small rotation moves the diagonal
// least, thus it disturbs least of what earlier rotations already settled, and
// the whole method holds its accuracy because of it.
static void eigen_rotate(matrix_t* matrix, matrix_t* vectors, uint32_t p,
                         uint32_t q)
{
    real_t across = matrix_get_element(matrix, p, q);

    if(REAL_ABS(across) <= REAL_SMALLEST)
    {
        return;
    }

    real_t difference = matrix_get_element(matrix, q, q)
                        - matrix_get_element(matrix, p, p);

    // The tangent of the angle, by the smaller root. Written so that no two
    // nearly equal numbers are ever subtracted.
    real_t theta = difference / (REAL_C(2.0) * across);
    real_t tangent;

    if(theta >= REAL_C(0.0))
    {
        tangent = REAL_C(1.0)
                  / (theta + REAL_SQRT(REAL_C(1.0) + (theta * theta)));
    }
    else
    {
        tangent = -REAL_C(1.0)
                  / (-theta + REAL_SQRT(REAL_C(1.0) + (theta * theta)));
    }

    real_t cosine = REAL_C(1.0) / REAL_SQRT(REAL_C(1.0)
                                            + (tangent * tangent));
    real_t sine = tangent * cosine;

    uint32_t order = matrix->m;

    // The two diagonal elements take the whole of what stood across them, and
    // that element becomes exactly nothing rather than nearly nothing.
    real_t moved = tangent * across;

    matrix_add_element(matrix, p, p,
                       matrix_get_element(matrix, p, p) - moved);
    matrix_add_element(matrix, q, q,
                       matrix_get_element(matrix, q, q) + moved);
    matrix_add_element(matrix, p, q, REAL_C(0.0));
    matrix_add_element(matrix, q, p, REAL_C(0.0));

    // Every other element of the two rows and the two columns is turned. The
    // matrix stays symmetric, thus only one half is worked out and the other
    // is written from it.
    for(uint32_t index = 0; index < order; index++)
    {
        if((index == p) || (index == q))
        {
            continue;
        }

        real_t at_p = matrix_get_element(matrix, index, p);
        real_t at_q = matrix_get_element(matrix, index, q);

        real_t next_p = (cosine * at_p) - (sine * at_q);
        real_t next_q = (sine * at_p) + (cosine * at_q);

        matrix_add_element(matrix, index, p, next_p);
        matrix_add_element(matrix, p, index, next_p);
        matrix_add_element(matrix, index, q, next_q);
        matrix_add_element(matrix, q, index, next_q);
    }

    if(vectors == NULL)
    {
        return;
    }

    // The rotations multiplied together are the directions. Each one turns two
    // columns of what has been gathered so far.
    for(uint32_t index = 0; index < order; index++)
    {
        real_t at_p = matrix_get_element(vectors, index, p);
        real_t at_q = matrix_get_element(vectors, index, q);

        matrix_add_element(vectors, index, p, (cosine * at_p) - (sine * at_q));
        matrix_add_element(vectors, index, q, (sine * at_p) + (cosine * at_q));
    }
}

// Put the values in order, largest first, and carry their directions with them.
//
// The order matters: a caller reading the first of them expects the one that
// matters most, and eigen_part_held counts from the front.
static void eigen_order(real_t* values, matrix_t* vectors, uint32_t count)
{
    for(uint32_t place = 0; place < count; place++)
    {
        uint32_t largest = place;

        for(uint32_t index = place + 1u; index < count; index++)
        {
            if(values[index] > values[largest])
            {
                largest = index;
            }
        }

        if(largest == place)
        {
            continue;
        }

        real_t held = values[place];

        values[place] = values[largest];
        values[largest] = held;

        if(vectors == NULL)
        {
            continue;
        }

        for(uint32_t row = 0; row < count; row++)
        {
            real_t at_place = matrix_get_element(vectors, row, place);

            matrix_add_element(vectors, row, place,
                               matrix_get_element(vectors, row, largest));
            matrix_add_element(vectors, row, largest, at_place);
        }
    }
}

bool eigen_solve(matrix_t* matrix, real_t* values, matrix_t* vectors)
{
    ASSERT(matrix != NULL);
    ASSERT(values != NULL);

    if(!eigen_is_valid_matrix(matrix))
    {
        return false;
    }

    uint32_t order = matrix->m;

    if(vectors != NULL)
    {
        if((vectors->m != order) || (vectors->n != order))
        {
            return false;
        }

        // The directions begin as the plain axes and are turned from there.
        matrix_set_unit(vectors);
    }

    // What the diagonal holds to begin with, which is what the off-diagonal
    // part is measured against.
    real_t diagonal = REAL_C(0.0);

    for(uint32_t index = 0; index < order; index++)
    {
        real_t value = matrix_get_element(matrix, index, index);

        diagonal += value * value;
    }

    real_t room = (diagonal * EIGEN_SMALLEST_PART * EIGEN_SMALLEST_PART)
                  + REAL_SMALLEST;

    bool settled = (eigen_off_diagonal(matrix) <= room);

    for(uint32_t sweep = 0; (sweep < EIGEN_LARGEST_SWEEPS) && !settled; sweep++)
    {
        // One sweep turns every element that is off the diagonal once.
        for(uint32_t p = 0; p < order; p++)
        {
            for(uint32_t q = p + 1u; q < order; q++)
            {
                eigen_rotate(matrix, vectors, p, q);
            }
        }

        settled = (eigen_off_diagonal(matrix) <= room);
    }

    if(!settled)
    {
        return false;
    }

    for(uint32_t index = 0; index < order; index++)
    {
        values[index] = matrix_get_element(matrix, index, index);
    }

    eigen_order(values, vectors, order);

    return true;
}

real_t eigen_condition(const real_t* values, uint32_t count)
{
    ASSERT(values != NULL);

    if(count == 0u)
    {
        return REAL_LARGEST;
    }

    real_t largest = REAL_C(0.0);
    real_t smallest = REAL_LARGEST;

    // Taken by size, because an eigenvalue may be negative and it is how far
    // from nothing it stands that matters here.
    for(uint32_t index = 0; index < count; index++)
    {
        real_t size_of = REAL_ABS(values[index]);

        if(size_of > largest) { largest = size_of; }
        if(size_of < smallest) { smallest = size_of; }
    }

    if(smallest <= REAL_SMALLEST)
    {
        return REAL_LARGEST;
    }

    return largest / smallest;
}

uint32_t eigen_rank(const real_t* values, uint32_t count, real_t part)
{
    ASSERT(values != NULL);

    real_t largest = REAL_C(0.0);

    for(uint32_t index = 0; index < count; index++)
    {
        real_t size_of = REAL_ABS(values[index]);

        if(size_of > largest) { largest = size_of; }
    }

    real_t floor_of = largest * part;
    uint32_t found = 0u;

    for(uint32_t index = 0; index < count; index++)
    {
        if(REAL_ABS(values[index]) > floor_of)
        {
            found++;
        }
    }

    return found;
}

real_t eigen_part_held(const real_t* values, uint32_t count, uint32_t first)
{
    ASSERT(values != NULL);

    if((count == 0u) || (first == 0u))
    {
        return REAL_C(0.0);
    }

    if(first > count)
    {
        first = count;
    }

    real_t whole = REAL_C(0.0);
    real_t held = REAL_C(0.0);

    // Taken by size, so that an eigenvalue that has come out a little below
    // nothing through rounding cannot make the answer larger than 1.
    for(uint32_t index = 0; index < count; index++)
    {
        real_t size_of = REAL_ABS(values[index]);

        whole += size_of;

        if(index < first)
        {
            held += size_of;
        }
    }

    if(whole <= REAL_SMALLEST)
    {
        return REAL_C(0.0);
    }

    return held / whole;
}

#else

// An empty translation unit is not C, thus one name is
// declared and nothing is defined. Nothing links against it.
typedef int eigen_is_not_in_this_build_t;

#endif//FFITT_NO_LINALG
