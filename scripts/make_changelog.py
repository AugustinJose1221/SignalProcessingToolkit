#!/usr/bin/env python3
"""Make CHANGELOG.md from the commits, one section for each tag.

WHY THIS EXISTS AND NOT `cz changelog`.

The changelog is meant to come from the commits themselves, so that what it
says and what was done cannot drift apart. commitizen is the tool for that, and
against this repository it gives a wrong answer in three different ways:

  - It compares the tags AS TEXT. "0.9.0" stands above "0.17.1" when the two
    are compared letter by letter, thus every release from 0.10.0 onward is
    filed under 0.9.0. Measured: 48 of 131 entries landed there and the
    sections for 0.10.0 to 0.17.1 came out empty.

  - Giving it a range does not mend that. The same collapse happens.

  - Asking it for one pair of tags at a time mends the collapse and brings two
    new faults: the entries of a release land under the release BEFORE it, and
    a commit that reached a branch and then a merge is counted twice.

This file does the same job with the tags sorted as NUMBERS and the merges left
out, thus a commit is counted once and lands where it belongs. The format is
the one commitizen writes, so nothing downstream needs to know the difference.

    python3 scripts/make_changelog.py            write CHANGELOG.md
    python3 scripts/make_changelog.py --check    give 1 if it is out of date

A release is written before it is tagged, thus the version being made has no
tag yet. Name it and the commits since the last tag are gathered under it:

    python3 scripts/make_changelog.py --unreleased-version 0.17.2
"""

import re
import subprocess
import sys
from collections import OrderedDict

# Only the kinds that say something happened to the library. A change to the
# tests or to the build is real work and belongs in the history, not in a list
# a caller reads to find out what moved under them.
TYPES = OrderedDict([("feat", "Feat"), ("fix", "Fix"), ("perf", "Perf")])

PATTERN = re.compile(r"^(feat|fix|perf)(?:\(([^)]*)\))?!?:\s*(.+)$")


def run(*arguments):
    return subprocess.run(arguments, capture_output=True, text=True).stdout


def as_numbers(tag):
    return [int(part) for part in tag.split(".")]


def build(unreleased=None):
    tags = sorted(run("git", "tag").split(), key=as_numbers)

    # The release being made has no tag yet. It stands at the end, and what
    # belongs to it is everything since the last tag there is.
    order = tags + ([unreleased] if unreleased else [])
    sections = []

    for index, tag in enumerate(order):
        span = ("%s..%s" % (order[index - 1], tag)) if index else tag
        if tag == unreleased:
            span = "%s..HEAD" % tags[-1] if tags else "HEAD"

        # --no-merges keeps a commit from being counted once on the branch that
        # made it and again through the merge that brought it in.
        subjects = run("git", "log", "--no-merges", "--format=%s",
                       span).splitlines()
        at = "HEAD" if tag == unreleased else tag
        date = run("git", "log", "-1", "--format=%cs", at).strip()

        grouped = OrderedDict((kind, []) for kind in TYPES)
        seen = set()

        for subject in subjects:
            found = PATTERN.match(subject.strip())
            if not found:
                continue
            kind, scope, text = found.group(1), found.group(2), found.group(3)
            line = "- **%s**: %s" % (scope, text) if scope else "- %s" % text
            if line in seen:
                continue
            seen.add(line)
            grouped[kind].append(line)

        block = ["## %s (%s)" % (tag, date)]
        for kind, title in TYPES.items():
            if grouped[kind]:
                block += ["", "### %s" % title, ""] + grouped[kind]

        # A RELEASE THAT MOVES NOTHING UNDER A CALLER MUST SAY SO.
        #
        # Only the kinds above are listed, thus a release whose work went into
        # the tests, the documentation or the build has nothing to show. A bare
        # heading with nothing under it reads as an oversight. This says what
        # is true instead.
        if not any(grouped[kind] for kind in TYPES):
            block += ["", "Nothing here changes what a caller sees. The work of"
                          " this release went into the tests, the"
                          " documentation or the build."]

        sections.append("\n".join(block))

    return "\n\n".join(reversed(sections)) + "\n"


def main():
    unreleased = None
    if "--unreleased-version" in sys.argv:
        unreleased = sys.argv[sys.argv.index("--unreleased-version") + 1]

    wanted = build(unreleased)

    if "--check" in sys.argv:
        try:
            held = open("CHANGELOG.md").read()
        except OSError:
            held = ""
        if held != wanted:
            print("CHANGELOG.md is not what the commits say. Make it again:")
            print("    python3 scripts/make_changelog.py")
            return 1
        print("The changelog check found no fault.")
        return 0

    open("CHANGELOG.md", "w").write(wanted)
    print("Wrote CHANGELOG.md: %d releases." % wanted.count("\n## "))
    return 0


if __name__ == "__main__":
    sys.exit(main())
