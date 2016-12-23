import re
import subprocess

import pytest


def run(args):
    proc = subprocess.run(
        args,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return proc.stdout, proc.stderr



@pytest.mark.parametrize('arg', ('-V', '--version'))
def test_version(tap2tap, arg):
    stdout, stderr = run((tap2tap, arg))
    assert stdout == b''
    assert re.match(b'^tap2tap v[\d\.]+$', stderr)
