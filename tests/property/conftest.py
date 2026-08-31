"""Common parts for the property based tests."""

import os
import sys

import pytest
from hypothesis import settings

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ffitt  # noqa: E402

# The tests call C code, thus each example costs more time than a pure Python
# example. This profile keeps the number of examples reasonable.
settings.register_profile("c", max_examples=200, deadline=None)
settings.load_profile("c")


@pytest.fixture(scope="session")
def lib():
    """Build the library one time and give the loaded shared object."""
    return ffitt.load_library()
