#!/bin/bash
# ===========================================================================
#  One-shot Pythia build for Linux (x64).
#
#  Produces the Linux binaries (Pythia_x64.so, PythiaSetPythonPath_x64.so)
#  inside @Pythia and stages the templates. The signed addon PBOs are built
#  on Windows with Mikero's tools (see build.bat) or taken from a release;
#  they are platform-independent, so a complete @Pythia is binaries from both
#  OSes plus one shared set of PBOs.
#
#  Usage:  ./build.sh
#
#  Requirements (Debian/Ubuntu names):
#     - python3.<minor>-dev  (the version in .github/workflows/build.yml)
#     - build-essential cmake ninja-build
# ===========================================================================
set -e

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "Building Pythia (Linux x64)"
echo "========================================"

# --- Read the target Python version (single source of truth) ---------------
PYVER="$(grep -oE 'PYTHON_VERSION:[[:space:]]*[0-9.]+' .github/workflows/build.yml | grep -oE '[0-9.]+')"
if [ -z "$PYVER" ]; then
    echo "ERROR: Could not read PYTHON_VERSION from .github/workflows/build.yml"
    exit 1
fi
PYVER_MM="$(echo "$PYVER" | cut -d. -f1-2)"
echo "Target Python: $PYVER ($PYVER_MM)"

# --- Tool checks -----------------------------------------------------------
for tool in cmake g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: $tool not found. Install with: sudo apt install build-essential cmake"
        exit 1
    fi
done

# Prefer the exact interpreter, fall back to python3.
PYCMD="python$PYVER_MM"
command -v "$PYCMD" >/dev/null 2>&1 || PYCMD="python3"
if ! command -v "$PYCMD" >/dev/null 2>&1; then
    echo "ERROR: Python $PYVER_MM not found. Install with: sudo apt install python$PYVER_MM python$PYVER_MM-dev"
    exit 1
fi

PYTHON_INCLUDE="$("$PYCMD" -c 'import sysconfig; print(sysconfig.get_path("include"))')"
if [ ! -f "$PYTHON_INCLUDE/Python.h" ]; then
    echo "ERROR: Python development headers not found ($PYTHON_INCLUDE/Python.h)."
    echo "Install with: sudo apt install python$PYVER_MM-dev"
    exit 1
fi
echo "Python development headers: OK ($PYTHON_INCLUDE)"

# --- Configure + build -----------------------------------------------------
GENERATOR=""
command -v ninja >/dev/null 2>&1 && GENERATOR="-G Ninja"

rm -rf build_x64
mkdir -p build_x64
cmake -S . -B build_x64 $GENERATOR -DUSE_64BIT_BUILD=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build_x64

mkdir -p "@Pythia"

# --- Stage templates -------------------------------------------------------
echo ""
echo "Staging templates..."
"$PYCMD" tools/stage_templates.py "$PYVER"

echo ""
echo "========================================"
echo "Build complete: @Pythia (Linux binaries)"
echo "========================================"
echo "Deploy:"
echo "  1. Copy the @Pythia folder into your Arma 3 directory."
echo "  2. Ensure @Pythia/addons + @Pythia/keys (the PBOs) are present"
echo "     (built on Windows with build.bat, or taken from a release)."
echo "  3. Install Python $PYVER_MM on every machine that runs it."
echo "  4. The .so requires that exact Python version at runtime."
echo "See USAGE.md for how to write Python extensions."
