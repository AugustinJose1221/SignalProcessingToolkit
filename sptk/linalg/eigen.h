#ifndef EIGEN_H
#define EIGEN_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/matrix.h>
#else
#include "real.h"
#include "matrix.h"
#endif

// The directions a symmetric matrix stretches, and how far it stretches each.
//
// A symmetric matrix does one thing: it stretches space, by different amounts
// in different directions, and those directions stand at right angles to each
// other. THE DIRECTIONS ARE THE EIGENVECTORS AND THE AMOUNTS ARE THE
// EIGENVALUES, and together they are the whole of what the matrix does.
//
// WHAT THIS IS FOR
//
//   READING A COVARIANCE. A covariance says how a set of measurements spreads.
//   Its largest eigenvalue is how far the spread reaches at its widest, and
//   the eigenvector beside it is which way that is. For a sensor of three
//   axes, that direction is the axis the movement really lies along, whatever
//   the axes of the sensor happen to be.
//
//   FINDING WHAT MATTERS AND WHAT DOES NOT. Where one eigenvalue is far larger
//   than the rest, nearly all of the movement lies in one direction and the
//   rest is noise. Where they are all alike, there is no structure to find.
//   This is what principal components means, and it is two lines once the
//   eigenvalues are in hand.
//
//   SAYING WHETHER AN ANSWER CAN BE TRUSTED. eigen_condition gives the largest
//   eigenvalue divided by the smallest, and that number is how much a small
//   error in what goes in is multiplied on its way out. It is the number
//   behind two things this library already records: why lstsq refuses a fit,
//   and why an RLS filter can run correctly for thousands of samples and then
//   fall apart.
//
// SYMMETRIC ONLY, AND THAT IS ON PURPOSE
//
// This module takes a symmetric matrix and nothing else. That is not a
// shortcut; it is the case that signal processing asks for, because every
// covariance is symmetric. It also happens to be the case that behaves: a
// symmetric matrix has real eigenvalues and eigenvectors at right angles, and
// the method below finds them steadily.
//
// A matrix that is NOT symmetric can have eigenvalues that are complex, can
// have eigenvectors that lie almost on top of each other, and needs a method
// several times larger that holds far less well in a float. Where the question
// is really about the roots of a polynomial, ask for those directly rather
// than through a general eigenvalue.
//
// HOW IT IS DONE
//
// The rotations of Jacobi. Each rotation picks the largest element that is off
// the diagonal and turns two rows and two columns so that element becomes
// nothing. That undoes a little of what earlier rotations did, thus it is done
// again and again, and the off-diagonal part falls away quickly. What is left
// on the diagonal are the eigenvalues, and the rotations multiplied together
// are the eigenvectors.
//
// It is not the fastest method for a large matrix. It is the one that keeps
// its accuracy for the small matrices this library works with, and it needs no
// memory of its own beyond what the caller gives.
//
// WHAT IT COSTS IN PRECISION
//
// Measured on matrices built by turning a known set of eigenvalues, so that
// the right answer is known exactly. The worst eigenvalue out by, as a part of
// the largest one, over 200 matrices at each order:
//
//     order            2         3         4         6         8
//     32 bits    0.0000002 0.0000004 0.0000004 0.0000004 0.0000007
//     64 bits    below what these figures can show, at every order
//
// THE ERROR DOES NOT FOLLOW THE CONDITIONING, and that is what parts this
// method from the ones that are quicker. Measured on matrices of order 5 built
// to a chosen conditioning, at 32 bits:
//
//     condition of the matrix      1      10    1 000   100 000   10 000 000
//     worst of A times v less
//       the value times v        0.0    0.00000005  0.00000007  0.00000009  0.00000003
//     each direction is this
//       far from unit length     0.0    0.0000001   0.0000001   0.00000004  0.0000001
//
// A matrix whose widest direction is ten million times its narrowest still
// gives directions that are right to seven digits. A method that worked
// through the normal equations, as lstsq does, would have nothing left at all
// by then.

// How many sweeps of the whole matrix to make before giving up.
//
// A sweep turns every element that is off the diagonal once. The off-diagonal
// part falls away faster than by half each sweep, thus a handful of sweeps
// carries any matrix this library works with past what either width can hold.
// This is well above that, and it is here so that a matrix which somehow will
// not settle cannot spin for ever.
#ifndef EIGEN_LARGEST_SWEEPS
#define EIGEN_LARGEST_SWEEPS    30u
#endif

// How small the off-diagonal part must be, against the diagonal, before the
// work is done.
#ifndef EIGEN_SMALLEST_PART
#define EIGEN_SMALLEST_PART     (REAL_C(10.0) * REAL_EPSILON)
#endif

// True if this matrix can be given to eigen_solve: square, at least one by
// one, and symmetric within the tolerance that eigen_solve uses.
bool eigen_is_valid_matrix(matrix_t* matrix);

// Find the eigenvalues and the eigenvectors of a symmetric matrix.
//
// THE MATRIX LOSES ITS CONTENT. The method works by turning the matrix itself
// until only its diagonal is left, thus a caller that still needs the matrix
// must copy it first with matrix_copy.
//
// The values hold one number for each row and come back LARGEST FIRST, which
// is the order that makes the first of them the one that matters. The vectors
// must be a matrix of the same order; column k of it is the direction that
// belongs to value k, and it is of unit length.
//
// The vectors may be NULL where only the values are wanted, and the work is
// then a little less.
//
// Give false if the matrix is not one eigen_is_valid_matrix accepts, if the
// vectors are the wrong order, or if the rotations did not settle within
// EIGEN_LARGEST_SWEEPS sweeps.
bool eigen_solve(matrix_t* matrix, real_t* values, matrix_t* vectors);

// Give the largest eigenvalue divided by the smallest, both taken by size.
//
// THIS IS THE ONE NUMBER TO LOOK AT. It says how much a small error in what
// goes into a calculation is multiplied on its way out. A condition of 1 is as
// good as a matrix gets; a condition near 1 divided by the smallest step the
// width can tell means the answer is made of rounding.
//
// Give REAL_LARGEST where the smallest eigenvalue is nothing, which means the
// matrix squashes some direction to nothing and cannot be undone at all.
real_t eigen_condition(const real_t* values, uint32_t count);

// How many directions the matrix really stretches, which is how many
// eigenvalues stand above the largest one multiplied by the part given.
//
// A part of about 1000 times the smallest step the width can tell is the usual
// choice. Below that the answer counts directions that are nothing but
// rounding.
uint32_t eigen_rank(const real_t* values, uint32_t count, real_t part);

// How much of the whole spread the first few directions hold, from 0 to 1.
//
// This is what principal components is for: where the first two of six
// directions hold 0.98 of the spread, the other four are noise and the
// measurement really has two dimensions and not six.
//
// Give 0 where the count is nothing or the eigenvalues do not add to anything.
real_t eigen_part_held(const real_t* values, uint32_t count, uint32_t first);

#endif//EIGEN_H
