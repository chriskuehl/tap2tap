import os.path
import pytest


@pytest.fixture
def tap2tap():
    return os.path.join(os.path.dirname(os.path.dirname(__file__)), 'tap2tap')
