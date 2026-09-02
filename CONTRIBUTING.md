# Contributing

This repository uses [Conventional Commits](https://www.conventionalcommits.org)
and semantic versioning.

Work goes into a branch with the name `feature/<name>` or `fix/<name>`. Such a
branch merges into `development`. When `development` is stable, a branch with
the name `release/vX.Y.Z` comes from it. Only fixes go into a release branch,
and that branch then merges into `main`.

## The feature freeze

**From the release of 0.17.0 this library is in a feature freeze.** It takes
fixes, tests and documentation. It does not take new modules or new public
functions.

The freeze is not a note in a file. `scripts/check_freeze.py` counts the public
functions in every header and compares them against the counts recorded at
0.17.0, and it runs as its own job in the workflow. Adding a function fails the
build, and so does taking one away: removing one changes what callers may rely
on, and a freeze is exactly the time not to do that by accident.

To lift it on purpose for a release, run

```bash
python3 scripts/check_freeze.py --show
```

and paste the answer into `FROZEN` in that file, so that the change is one a
reviewer can see in the diff.

**What the freeze is for.** An audit before it found 96.5 percent of lines
covered and no function between nothing and sixty percent, but 21 modules that
no generated test has ever exercised. Every catch-up round on that has found
real faults. The freeze is the time to close it.

**What the freeze has closed.** Every module that is not a handful of lines now
has a file of rules that hold it to what it IS and not to what its interface
looks like. Those rules found three faults in the library: a filter that gave a
different shape for the same reading measured in volts and in millivolts, a
step for a derivative that was five thousand times worse than the width could
do, and a decomposition whose envelope stood still while it took the same
amount away again and again, so that a signal of size 3 gave a residue of a
million and a half.

**How much is covered, and why the number is 98 and not 99.** The build fails
below 98 percent of lines and below 90 percent of branches. What is left is not untested behaviour: it is the
guards against the heap giving nothing and against numbers the width cannot
hold. Reaching those needs an allocator that fails on purpose, or calls that
break what the headers say a caller may do. Covering every line a caller CAN
reach still leaves the whole below 99, thus 99 is a number the code cannot meet
while those guards stand, and taking them out to meet it would be the wrong
trade. Two of them were found to be unreachable because the caller's own check
is stricter than the guard, and those are named where they stand.

The guards against the heap giving nothing ARE now reached. `Test_heap_refusal`
asks the linker to send `malloc` and `calloc` through a pair of functions that
refuse on command, which is the only way to ask for a heap that fails. Every
allocator of the library is held to what `ffitt/core/README.md` says it must
then do.

**The branch number leaves the assertions out, and it must.** An `ASSERT`
states what the CALLER must have got right, and its failing side calls abort. A
suite that passes has by construction never taken that side. There are 964 of
them, and counted in they hold the whole at 74 percent and hide every real gap
behind a wall of branches no test could ever turn green. Left out, the number
is 91.2 and it means something: what is still uncovered is mostly the operand
combinations inside the validators, where a refusal is tested and which half of
an `&&` caused it is not.

**Rules are run over and over, not once.** A rule that passes one run has been
given a few hundred cases. The suite is run 25 times at each width before a
release, and that has found five rules whose bound was too tight to be true.
None of them was a fault in the library, and two of them corrected what the
headers claimed: how far a resampler really stops a tone at the very edge of
the band, and what a coefficient of correlation needs of a signal before it
means anything.

## Making a release

The bump command of commitizen is not in use, thus a release is made by hand.
When `development` is stable:

1. Make the release branch from `development`:

   ```bash
   git checkout development && git checkout -b release/vX.Y.Z
   ```

2. Write the entry for the new version at the top of `CHANGELOG.md`. Take the
   text of each entry from the subject of each commit since the last tag:

   ```bash
   git log --reverse --format='%s' <last tag>..HEAD
   ```

3. Change the version in the `project` command of `CMakeLists.txt` to the new
   version.

4. Commit the two files, and give the commit the message
   `docs(changelog): Add the entry for the version X.Y.Z`.

5. Only fixes go into the release branch after this point. Run the tests, the
   naming check and the documentation check again after each fix.

6. Merge the branch into `main` and into `development`, and make the tag:

   ```bash
   git checkout main && git merge --no-ff release/vX.Y.Z -m "Merge branch 'release/vX.Y.Z'"
   ```

   ```bash
   git checkout development && git merge --no-ff release/vX.Y.Z -m "Merge branch 'release/vX.Y.Z' into development"
   ```

   ```bash
   git tag -a X.Y.Z -m "X.Y.Z" main
   ```

   ```bash
   git push origin main development X.Y.Z
   ```

**Both of those are merges and neither can be a fast forward.** The release
branch is merged into two branches, thus each of them gets a merge commit of its
own and from that moment neither branch holds the other. The tags up to 0.8.0
name a plain commit and a fast forward worked then; 0.9.0 is the first that
names a merge commit, and no fast forward onto `main` has been possible since.
These steps said `--ff-only` until 0.14.0, which aborts.

**The tag names `main`.** Both branches hold the same tree at this point but they
are different commits, and every tag from 0.1.0 onwards names the commit on
`main`. Leaving the branch off tags whichever branch happens to be checked out,
which is `development` if the steps are followed in the order above.

The name of the tag holds no letter v, but the name of the branch does. The
tag 0.1.0 set that rule.

Each push runs the workflow in
[.github/workflows/tests.yml](.github/workflows/tests.yml). It runs the
documentation check, the naming check, the example check, the unit tests, the
property based tests, the build, and a build with the warnings of the compiler
switched on. Everything but the first three runs at both widths, which makes
eleven jobs. A warning stops the workflow.