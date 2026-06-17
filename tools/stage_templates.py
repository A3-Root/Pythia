#!/usr/bin/env python3
"""
Stage the files in templates/ into @Pythia/, substituting build-time
placeholders. Standard-library only so the one-shot build.bat / build.sh
scripts can run it with any system Python (no uv / third-party packages).

Placeholders:
  {version}         -> concatenated major+minor, e.g. "312"
  {version_dotted}  -> dotted major.minor, e.g. "3.12"
  {dotnet_timestamp}-> .NET-style UTC tick timestamp (for meta.cpp)
"""

import os
import sys
from datetime import datetime, timezone

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.dirname(THIS_DIR)


def read_python_version():
    """Single source of truth: PYTHON_VERSION in the CI workflow file."""
    yml = os.path.join(ROOT_DIR, '.github', 'workflows', 'build.yml')
    with open(yml, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('PYTHON_VERSION:'):
                return line.split(':', 1)[1].strip()
    raise RuntimeError('Could not find PYTHON_VERSION in build.yml')


def dotnet_timestamp():
    # https://stackoverflow.com/a/15919878/6543759
    kind = (1 << 62)  # UTC
    epoch = datetime(1, 1, 1, tzinfo=timezone.utc)
    ticks = int((datetime.now(timezone.utc) - epoch).total_seconds() * (10 ** 7))
    return str(ticks | kind)


def stage(version):
    major, minor = version.split('.')[:2]
    version_concat = f'{major}{minor}'.encode('ascii')
    version_dotted = f'{major}.{minor}'.encode('ascii')
    timestamp = dotnet_timestamp().encode('ascii')

    templates_dir = os.path.join(ROOT_DIR, 'templates')
    out_dir = os.path.join(ROOT_DIR, '@Pythia')
    os.makedirs(out_dir, exist_ok=True)

    for name in os.listdir(templates_dir):
        src = os.path.join(templates_dir, name)
        if not os.path.isfile(src):
            continue
        with open(src, 'rb') as fread:
            data = fread.read()
        data = data.replace(b'{version_dotted}', version_dotted)
        data = data.replace(b'{version}', version_concat)
        data = data.replace(b'{dotnet_timestamp}', timestamp)
        with open(os.path.join(out_dir, name), 'wb') as fwrite:
            fwrite.write(data)
        print(f'  staged {name}')


if __name__ == '__main__':
    version = sys.argv[1] if len(sys.argv) > 1 else read_python_version()
    print(f'Staging templates for Python {version} into @Pythia/ ...')
    stage(version)
    print('Templates staged.')
