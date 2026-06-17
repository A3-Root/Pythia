# /// script
# dependencies = [
#   "packaging",
#   "pefile",
#   "pyelftools",  # elftools
# ]
# ///

import argparse
import os
import sys
from typing import List

import pefile
from elftools.elf.elffile import ELFFile


def check_dll_architecture(path: str, x86=False):
    arch = '32bit' if x86 else '64bit'
    print(f'Checking if file {path} is {arch}...')

    if not os.path.exists(path):
        print(f'File {path} is missing!')
        sys.exit(1)

    pe = pefile.PE(path)
    arch32 = bool(pe.NT_HEADERS.FILE_HEADER.Characteristics & pefile.IMAGE_CHARACTERISTICS['IMAGE_FILE_32BIT_MACHINE'])

    if (x86 and not arch32) or (not x86 and arch32):
        print(f'File {path} is not {arch}!')
        sys.exit(1)


def check_dll_is_static(path: str, allowed_imports: List = None):
    """
    Ensure a given DLL doesn't try importing some funny dependencies
    because we messed up something in the compiler options or something.
    """

    print(f'Checking if file {path} is static...')

    if not os.path.exists(path):
        print(f'File {path} is missing!')
        sys.exit(1)

    if allowed_imports is None:
        allowed_imports = []

    allowed_imports_lower = {b'kernel32.dll'}
    for allowed_import in allowed_imports:
        allowed_imports_lower.add(allowed_import.lower())

    pe = pefile.PE(path)
    file_imports = [entry.dll.lower() for entry in pe.DIRECTORY_ENTRY_IMPORT]
    for file_import in file_imports:
        if file_import not in allowed_imports_lower:
            print(f'File {path} is not static! It imports {file_import}!')
            sys.exit(1)


def check_so_architecture(path: str, x86=False):
    arch = '32bit' if x86 else '64bit'
    print(f'Checking if file {path} is {arch}...')

    if not os.path.exists(path):
        print(f'File {path} is missing!')
        sys.exit(1)

    with open(path, 'rb') as file:
        elffile = ELFFile(file)

    arch32 = elffile.elfclass == 32

    if (x86 and not arch32) or (not x86 and arch32):
        print(f'File {path} is not {arch}!')
        sys.exit(1)


def safety_checks(python_version):
    major, minor, patch = python_version.split('.')
    dll_import = f'python3{minor}.dll'.encode('ascii')

    # Pythia links the system python3X.dll directly (no bundled interpreter), so
    # that import is expected. ABI compatibility is the responsibility of the
    # system Python install; there is no manylinux/glibc baseline to enforce.
    check_dll_is_static(os.path.join('@Pythia', 'Pythia.dll'), allowed_imports=[dll_import])
    check_dll_is_static(os.path.join('@Pythia', 'Pythia_x64.dll'), allowed_imports=[dll_import])
    check_dll_is_static(os.path.join('@Pythia', 'PythiaSetPythonPath.dll'))
    check_dll_is_static(os.path.join('@Pythia', 'PythiaSetPythonPath_x64.dll'))
    print()
    check_dll_architecture(os.path.join('@Pythia', 'Pythia.dll'), x86=True)
    check_dll_architecture(os.path.join('@Pythia', 'Pythia_x64.dll'), x86=False)
    check_dll_architecture(os.path.join('@Pythia', 'PythiaSetPythonPath.dll'), x86=True)
    check_dll_architecture(os.path.join('@Pythia', 'PythiaSetPythonPath_x64.dll'), x86=False)
    print()
    check_so_architecture(os.path.join('@Pythia', 'Pythia_x64.so'), x86=False)
    check_so_architecture(os.path.join('@Pythia', 'PythiaSetPythonPath_x64.so'), x86=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Perform basic safety checks over the DLLs/SOs')
    parser.add_argument('version', help='Python version against which to check')

    args = parser.parse_args()

    safety_checks(args.version)
