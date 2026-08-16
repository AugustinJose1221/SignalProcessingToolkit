# Estimation

These two modules follow a state that you cannot measure directly, through
measurements that hold noise.

The idea behind both: hold a guess of the state and a number that says how much
doubt that guess holds. At each step, move the guess forward through a model of
how the state behaves, which makes the doubt larger. Then read a measurement,
and pull the guess towards it by an amount that depends on which of the two
holds less doubt. That amount is the gain.

| Module | The model of the state and of the measurement |
| --- | --- |
| `kalman` | Two matrices |
| `ekf` | Two functions |

## kalman

The plain Kalman filter works with a model of this shape:

    x = A*x + B*u        how the state moves
    y = C*x              what the measurement reads

Each step holds two halves.

**Predict** moves the state through `A` and the input through `B`, and it makes
the covariance `P` larger by the process noise `Q`:

    x = A*x + B*u
    P = A*P*A' + Q

**Update** reads the measurement `y`. The innovation is the difference between
that measurement and the measurement that the present state would produce. The
gain `K` decides how much of that difference goes into the state:

    S = C*P*C' + R
    K = P*C'*inverse(S)
    x = x + K*(y - C*x)
    P = (I - K*C)*P

`S` holds the doubt of the innovation: the doubt of the state seen through `C`,
plus the noise of the measurement `R`. When the measurement holds little noise,
`K` grows and the filter follows the measurement. When the state holds little
doubt, `K` falls and the filter keeps its own guess.

**Setting Q and R.** `R` comes from the sensor, and its data sheet often gives
it. `Q` says how much the model itself is wrong. A larger `Q` makes the filter
follow the measurements more closely and react faster; a smaller `Q` makes it
smoother and slower.

**The filter needs no memory while it runs.** Every intermediate result goes
into scratch matrices that the allocation makes. `KALMAN_MEMPOOL_SIZE` gives
the size of the memory pool for a filter that takes memory from the caller.

## ekf

Many real models do not have the shape above. A radar gives a distance, which
is a square root of a sum of squares. A pendulum turns with the sine of its
angle. Such a model needs a function.

This filter takes two functions: `f`, which gives the next state from the
present state and the input, and `h`, which gives the measurement that a state
would produce.

The covariance still moves through a matrix. The filter gets that matrix from
the slope of the function at the present state, which is the Jacobian matrix.
It calculates the Jacobian with the central difference: it moves one element of
the state a little to each side, calls the function two times, and divides the
difference by the whole distance. Thus you write the two functions only, and
you write no derivative.

**Choose the step of the difference with care.** A step that is too small loses
every digit in a float. A step that is too large gives the slope of the wrong
place. The default is 0.001, and `ekf_set_derivative_step` changes it. Match it
to the size of the values of your state.

**Before you build a model, ask whether the measurements can name the state.**
A radar that reads a distance alone can never say where on the circle of that
distance the object is. No filter can find what the measurements do not hold.
Add a second reading, such as an angle, or a second station.

`sptk/linalg` gives every matrix operation that both filters use, in the form
that writes into a matrix that already holds memory. That is why neither filter
needs a heap while it runs.
