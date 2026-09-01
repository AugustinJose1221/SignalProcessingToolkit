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

`ffitt/linalg` gives every matrix operation that both filters use, in the form
that writes into a matrix that already holds memory. That is why neither filter
needs a heap while it runs.


## ukf

The unscented Kalman filter: following a state through a model that bends,
without ever taking a derivative.

**Which of the three to take.**

| | |
| --- | --- |
| the model is straight | `kalman`. Exact, and the cheapest |
| it bends gently | `ekf`. One Jacobian, less work than `ukf` |
| it bends sharply | `ukf` |
| the derivative is awkward | `ukf`. It needs none |

The last line is often the real reason. `ekf` works its Jacobians out by a
central difference, which needs the model to be smooth and needs a step chosen
for it. This filter needs neither, thus a model that is a table, or a piece of
code with a condition in it, is no trouble.

**What it does that a straight line cannot.** Put a spread through a bend and
its middle moves. A straight line through the middle cannot show that.
Measured, a spread put through a square, where the true middle of what comes
out is the middle squared plus the spread:

| middle in | spread in | truth | `ukf` | a straight line |
| --- | --- | --- | --- | --- |
| 0.0 | 9.0 | 9.0 | 9.0 | 0.0 |
| 1.0 | 4.0 | 5.0 | 5.0 | 1.0 |
| 3.0 | 1.0 | 10.0 | 10.0 | 9.0 |

This filter is exact there. A straight line misses the spread entirely, and at
a middle of zero it reports nothing at all where the answer is nine.

**What that does not mean.** It does not beat `ekf` at everything. For a smooth
model with many measurements both settle to the same answer, and `ekf` often
gets there with slightly less work: measured on a state that does not move seen
through a square over sixty readings, the two ended within 3 percent of each
other and `ekf` was marginally the closer. The gain is in **one** step through
a bend, which is what matters when readings are few or the model is run far
forward between them, and in needing no derivative.

**How small alpha may be follows the width of the build.** The weights of the
points are about `1/(alpha*alpha*nx)` in size and must add up to 1, thus a
small alpha makes very large weights that add to a very small number. Measured,
what the weights really add up to for a state of 3:

| alpha | 0.001 | 0.010 | 0.050 | 0.100 |
| --- | --- | --- | --- | --- |
| 32 bits | 1.0625 | 1.0000 | 0.9999 | 1.0000 |
| 64 bits | 1.0000 | 1.0000 | 1.0000 | 1.0000 |

At 32 bits and an alpha of 0.001 the weights are 6 percent wrong before the
filter has done anything. The literature gives 0.001 as the usual choice
because it assumes a wide number. `UKF_DEFAULT_ALPHA` therefore follows the
width, and `ukf_is_valid_spread` says whether a given alpha can be held.

**It refuses rather than carrying on.** The points are placed with the factor
of Cholesky of the covariance, thus the covariance must stay a real spread.
Arithmetic can take it out of that state, and `ukf_predict` and `ukf_update`
both give false when it has.

## propagate

**Every estimator here asks for a discrete step, and nobody writes a model that
way.** A model of anything physical is written as a rate of change: a temperature
falls at a rate following how far above the room it is, a pendulum turns back at
a rate following how far over it leans. `propagate` turns one into the other.

**The three methods differ in how the error falls as the step shrinks**, and that
is what the order of a method means. Measured on a turning with a known answer,
across one second at 64 bits:

| step | 0.1 | 0.05 | 0.025 | 0.0125 |
|---|---|---|---|---|
| euler | 5.1e-02 | 2.5e-02 | 1.3e-02 | 6.3e-03 |
| midpoint | 1.7e-03 | 4.2e-04 | 1.0e-04 | 2.6e-05 |
| runge | 8.3e-07 | 5.2e-08 | 3.3e-09 | 2.0e-10 |

Read along each row: euler halves, midpoint quarters, runge falls to a
sixteenth — exactly.

**At 32 bits the method outruns the width.** The same measurement gives runge
8.5e-07, 1.4e-07, 2.3e-07, 1.2e-07 — it stops at about a part in ten million and
goes no further, because by then the error of the method is below the rounding of
the state itself and halving the step only adds more roundings. **There is
nothing to gain from a smaller step than that, and a little to lose.**

**Split the sample interval rather than taking it in one step.** The sample rate
fixes how far apart the measurements are, and that distance is usually far too
large for one step. `propagate_state_over` splits it, and splitting costs exactly
what one step of that total size would have cost.

**Do not reach for the best method every time.** The `continuous` example
measures midpoint and Runge giving the same answer, because at that step the
midpoint error had already fallen below the noise on the measurements. Carry the
model well enough that it is not the worst thing in the answer, then stop.

## pll

A transform reads a block and gives the frequencies in it. **That is a trade you
cannot escape by writing better code**: make the block long enough to tell 50.0
Hz from 50.1 and the frequency has moved before the block is over; make it short
enough to follow the movement and it can no longer tell the two apart.

A loop does not have that trade. It holds a guess of the frequency and a guess of
the phase, compares its guess against what arrives, and moves. It gives a **new
answer at every sample**, and how quickly it follows a change is a number you set
rather than a length you have to choose.

Reach for it where the frequency **is** the measurement and will not stay still:
a tachometer, a mains watcher, a tag whose carrier the motion has shifted.

**What is paid for it.** A loop can be wrong in three ways a transform cannot:

| | |
|---|---|
| It must be started near the answer | `pll_pull_range` says how far it reaches |
| It can settle onto something that is not there | `pll_lock_quality` is the only thing that says so |
| It takes time to arrive | `pll_settle_samples` gives a rough figure |

**The second one is the trap.** Given noise and no tone at all, the loop settles
somewhere and reports a frequency with exactly the confidence it reports a real
one. Read the lock quality.

**The bandwidth is the whole of the trade.** Measured on a tone at a tenth of the
sample rate, with noise as loud as the tone for the wander:

| bandwidth | wander of the answer | samples to find the tone |
|---|---|---|
| 0.0005 | 0.001107 | 4603 |
| 0.0010 | 0.001540 | 1154 |
| 0.0050 | 0.006196 | 57 |
| 0.0100 | 0.012724 | 42 |

Twenty times the bandwidth finds the tone a hundred times as fast and wanders
eleven times as far. Neither end is right.

**It measures how loud the signal is and divides by it.** Without that the gain
of the loop would be the gain you asked for multiplied by the loudness of
whatever arrived — a quiet tone would never lock, a loud one would be unstable,
and the bandwidth would mean nothing.

**The bandwidth must stay well below the frequency being followed.** The detector
gives the error it wants and a ripple at twice the tone on top of it, and the
loop leans on being too slow to follow that ripple.
